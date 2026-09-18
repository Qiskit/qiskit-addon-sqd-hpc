# Investigation: Efraimidis–Spirakis for weighted sampling without replacement

**Status: investigated, not adopted.** The current `NoReplacementSampler` is
retained. This directory records why, so the question does not have to be
re-litigated from scratch and so a future hybrid approach has a starting point.

## Background

`recover_configurations` corrects each bitstring by flipping `k` bits chosen by
weighted sampling **without replacement** from `n` candidate positions. That
sampling is `internal::NoReplacementSampler`, which draws proportionally to the
weights, removes the drawn index, renormalizes, and repeats — the *sequential*
(or *successive*) scheme. It is implemented with rejection: it reuses one
`std::discrete_distribution`, zeroing drawn weights, and rebuilds the
distribution when repeated draws collide.

Efraimidis–Spirakis (E–S) is the standard keys-based alternative: give each
candidate `i` a key `ln(u_i) / w_i` (with `u_i ~ U(0,1]`), then take the
candidates with the largest keys. It uses exactly one random draw per candidate
and has no rejection or rebuild. `es_sampler.hpp` here implements it.

## Semantics (the part that is easy to get wrong)

The target is the **sequential scheme**, which is what `NoReplacementSampler`
implements and what NumPy's `Generator.choice(replace=False, p=...)`, Julia
StatsBase, and R's `sample()` all expose. E–S provably reproduces this same
distribution.

It is **not** inclusion-probability-proportional-to-size (true πPS), where each
item's marginal probability of appearing in the final set is proportional to its
weight. That is a different distribution requiring different algorithms
(Sampford, Tillé). Validating a correct sequential/E–S sampler against the πPS
invariant makes it look broken — a classic source of lost confidence.

## Correctness: confirmed

`verify_distribution.cpp` builds an exact reference for small `(n, k)` by
enumerating every ordered draw path (each path's probability is the product of
renormalized weights) and aggregating to the distribution over selected sets.

A single G (log-likelihood-ratio) statistic near its threshold is meaningless,
so the program instead runs 400 independent replicates per case and checks that
the *distribution* of G follows chi-squared: `mean(G) ≈ dof` and
`P(G > 95% critical) ≈ 5%`. Both samplers pass, indistinguishably, across
ascending, uniform, dominant-weight, zero-weight, extreme-spread (1e-6 … 1e6),
and `k`-near-`n` cases:

```
  ascending 1,2,3,4 k=2  current  dof=5  mean(G)=4.95  exceed95=5.2%
  ascending 1,2,3,4 k=2  E-S      dof=5  mean(G)=5.07  exceed95=6.5%
  1..6 k=5 (k near n)    current  dof=5  mean(G)=5.27  exceed95=6.5%
  1..6 k=5 (k near n)    E-S      dof=5  mean(G)=5.17  exceed95=5.8%
  dominant 5,1,1,1 k=2   current  dof=5  mean(G)=4.81  exceed95=3.8%
  dominant 5,1,1,1 k=2   E-S      dof=5  mean(G)=4.88  exceed95=3.5%
  zeros 0,2,3,0,5 k=2    current  dof=2  mean(G)=2.01  exceed95=5.5%
  zeros 0,2,3,0,5 k=2    E-S      dof=2  mean(G)=1.98  exceed95=4.2%
```

(`test/test_sample_without_replacement.cpp` in the main tree is the committed,
CI-run version of the exact-reference check for the current sampler.)

## Performance: the reason it was not adopted

`benchmark_vs_current.cpp` measures constructing a sampler and drawing `k`
indices. The two have fundamentally different scaling:

- **E–S is `O(n log n)` regardless of `k`** — it computes and sorts one key per
  candidate, so its cost is essentially flat in `k`.
- **The current sampler is roughly `O(k + rejections)`** — sublinear in `n` for
  small `k`, but its rejection cost grows as `k` approaches `n`.

The crossover is around **k/n ≈ 0.5–0.6**. Representative results (ns per
construct-plus-draw-`k`, lower is better):

| n | k | k/n | current | E–S | winner |
|---|---|-----|---------|-----|--------|
| 1000 | 20 | 0.02 | 4,606 | 47,668 | current 10.3× |
| 500 | 10 | 0.02 | 2,287 | 22,143 | current 9.7× |
| 250 | 20 | 0.08 | 1,923 | 10,243 | current 5.3× |
| 100 | 50 | 0.50 | 3,183 | 3,668 | current 1.15× |
| 1000 | 500 | 0.50 | 47,291 | 47,726 | ~tie |
| 250 | 150 | 0.60 | 11,702 | 10,276 | E–S 1.14× |
| 1000 | 900 | 0.90 | 96,832 | 47,856 | E–S 2.0× |

E–S wins only when `k` is more than about half of `n`, and only modestly there;
below the crossover the current sampler is faster, by up to ~10×.

## Which regime does `recover_configurations` hit?

Here `k = num_flip` (how far a sampled bitstring's Hamming weight is from the
target electron count) and `n` = the number of flippable positions. `num_flip`
is an `O(√n)`-scale fluctuation of the occupancy around its mean, while `n`
grows linearly — so **`k/n` shrinks as systems grow**, except near half-filling
where `k` can be a large fraction of `n`. The regime is therefore
filling-dependent and spans both sides of the crossover.

A profiling run (separate from this directory) found the sampler is 72%–83% of
correction time, and the draws dominate within it — so the sampler is worth
optimizing. But plain E–S is not a robust win across the regimes SQD hits, so it
was not adopted.

## If revisited

- **Hybrid**: pick the sampler by the `k/n` ratio at construction. Simple, but
  adds a branch and two code paths to maintain, for a win only near
  half-filling.
- **A different rejection-free method**: a Fenwick/BIT-based exact sequential
  draw is `O(k log n)` with no rejection and no `O(n)` floor, so it could beat
  the current sampler for small `k` *and* not blow up near half-filling. This
  was not implemented here; it is the more promising direction than plain E–S.

## Reproducing

Build commands are in the header comment of each `.cpp`. Both compile against
the repository's `include/` and vendored Boost headers; the benchmark also needs
the `nanobench` submodule.
