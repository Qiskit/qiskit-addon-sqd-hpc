// This code is part of Qiskit.
//
// (C) Copyright IBM 2025.
//
// This code is licensed under the Apache License, Version 2.0. You may
// obtain a copy of this license in the LICENSE.txt file in the root directory
// of this source tree or at http://www.apache.org/licenses/LICENSE-2.0.
//
// Any modifications or derivative works of this code must retain this
// copyright notice, and modified files need to carry a notice indicating
// that they have been altered from the originals.

// These tests assert that the public entry points depend only on the
// std::uniform_random_bit_generator contract, not on any concrete engine.  They
// exercise several standard engines with differing result_type widths and
// ranges, a deliberately minimal hand-rolled generator, and -- where the
// standard library provides it (C++26) -- the counter-based std::philox_engine.
// They also pin the determinism the parallel work will rely on: the same engine
// with the same seed reproduces the same result.
//
// The generator list is defined once (RNG_ENGINE_LIST) and applied to each
// entry point via TEST_CASE_TEMPLATE_DEFINE / _INVOKE, so a single list drives
// every case and std::philox_engine is folded in on C++26 without a separate
// test body.

#include "doctest.h"

#include "qiskit/addon/sqd/configuration_recovery.hpp"
#include "qiskit/addon/sqd/internal/parallel-rng.hpp"
#include "qiskit/addon/sqd/subsampling.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

using Qiskit::addon::sqd::recover_configurations;
using Qiskit::addon::sqd::subsample_multiple_batches;

namespace
{

// A minimal std::uniform_random_bit_generator: nothing beyond what the concept
// requires (result_type, min, max, operator()) -- in particular, no seed().  A
// SplitMix64 step keeps it self-contained and well-distributed without pulling
// in <random> engines.
//
// This is the weakest generator the library accepts, and it is accepted in
// serial builds only: the OpenMP path needs an engine it can either key by
// counter or re-seed per thread, and this is neither.  Keeping the two
// capabilities in separate types is what lets the suite test both sides of that
// boundary -- MinimalSeedableURBG below adds seed() and nothing else, so the
// pair differs by exactly the property under test.
class MinimalURBG
{
  protected:
    std::uint64_t state_;

  public:
    using result_type = std::uint64_t;
    explicit MinimalURBG(std::uint64_t seed) : state_(seed)
    {
    }
    static constexpr result_type min()
    {
        return 0;
    }
    static constexpr result_type max()
    {
        return UINT64_MAX;
    }
    result_type operator()()
    {
        std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
};

// The same generator plus seed(), which is what realistic engines provide and
// what the OpenMP per-thread path requires.  Deriving rather than copying keeps
// the draw sequence identical to the base, so a test comparing the two is
// measuring seedability alone.
class MinimalSeedableURBG : public MinimalURBG
{
  public:
    using MinimalURBG::MinimalURBG;
    void seed(result_type s)
    {
        state_ = s;
    }
};

// Build a small but non-trivial recover_configurations workload and return its
// result, driven by the supplied (freshly seeded) generator.
template <typename RNGType>
std::pair<std::vector<std::bitset<8>>, std::vector<double>> run_recover(RNGType &rng)
{
    constexpr unsigned int N = 8;
    constexpr unsigned int half = N / 2;
    std::vector<std::bitset<N>> bitstrings;
    bitstrings.reserve(12);
    for (unsigned int i = 0; i < 12; ++i) {
        bitstrings.emplace_back(i * 7u + 1u);
    }
    std::vector<double> probabilities(bitstrings.size(), 1.0);
    std::array<std::vector<double>, 2> occs{
        std::vector<double>(half, 0.4), std::vector<double>(half, 0.6)
    };
    return recover_configurations(bitstrings, probabilities, occs, {2, 2}, rng);
}

// Two mock counter-based generators, shaped deliberately *unlike*
// std::philox_engine, so the substream-keying abstraction is validated against
// something other than the single engine it was written against.  Neither needs
// a __cpp_lib_philox_engine gate, so the counter-based path gets exercised on
// every row of the CI matrix rather than only the C++26 one.
//
// Both are real (if simple) counter-based generators in the
// Salmon-Moraes-Dror-Shaw sense: a stateless keyed bijection of the counter,
// buffered a block at a time.  The mixing is a SplitMix64 round rather than a
// Philox round -- enough to be well-distributed, and the point here is the
// interface, not the statistical quality.

// Mock 1: names its own `counter_type`, carries a *full-width* key (Threefry and
// ARS do; Philox's key is half the counter width), and spells `set_counter` as a
// member *template*.  That template is the interesting part: taking the address
// of a member template is ill-formed, so the old `decltype(&R::set_counter)`
// detection silently classified this engine as ordinary and dropped it to the
// weaker thread-count-dependent tier with no diagnostic.
class MockCBRNGTemplate
{
  public:
    using result_type = std::uint64_t;
    static constexpr std::size_t counter_words = 4;
    using counter_type = std::array<result_type, counter_words>;

  private:
    counter_type counter_{};
    counter_type key_{}; // full-width key, unlike Philox's half-width one
    std::array<result_type, counter_words> buffer_{};
    std::size_t index_ = counter_words; // empty: refill on first draw

    static result_type mix(result_type z)
    {
        z += 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    void generate_block()
    {
        // Every output word must depend on the *whole* counter and key, which is
        // what a real CBRNG round function does.  Mixing word i from counter word
        // i alone would leave outputs blind to the counter words that keying
        // actually writes to.
        result_type acc = 0;
        for (std::size_t i = 0; i < counter_words; ++i) {
            acc = mix(acc ^ counter_[i]);
            acc = mix(acc ^ key_[i]);
        }
        for (std::size_t i = 0; i < counter_words; ++i) {
            buffer_[i] = mix(acc ^ mix(static_cast<result_type>(i)));
        }
        // Advance the counter as one big integer, low word first.
        for (std::size_t i = 0; i < counter_words; ++i) {
            if (++counter_[i] != 0) {
                break;
            }
        }
        index_ = 0;
    }

  public:
    explicit MockCBRNGTemplate(result_type s = 0)
    {
        seed(s);
    }
    void seed(result_type s)
    {
        key_.fill(0);
        key_[0] = s;
        counter_.fill(0);
        index_ = counter_words;
    }
    // A member *template*, accepting any array-like counter.
    template <typename CounterLike>
    void set_counter(const CounterLike &c)
    {
        // Follow std::philox_engine: reversed word order, so c[0] is the most
        // significant word.
        for (std::size_t i = 0; i < counter_words; ++i) {
            counter_[i] = c[counter_words - 1 - i];
        }
        index_ = counter_words; // refill on next draw
    }
    static constexpr result_type min()
    {
        return 0;
    }
    static constexpr result_type max()
    {
        return UINT64_MAX;
    }
    result_type operator()()
    {
        if (index_ >= counter_words) {
            generate_block();
        }
        return buffer_[index_++];
    }
};

// Mock 2: the standard shape (a static `word_count`, no nested `counter_type`,
// so the counter type is *derived* as std::array<result_type, word_count>), but
// with an *overloaded* `set_counter`.  Taking the address of an overload set is
// also ill-formed, so this is the second way the old detection failed silently.
// Its 32-bit result_type additionally exercises the seed-narrowing path in
// fold_seed_to, which nothing else in the suite reaches.
class MockCBRNGOverloaded
{
  public:
    using result_type = std::uint32_t;
    static constexpr std::size_t word_count = 4;

  private:
    std::array<result_type, word_count> counter_{};
    result_type key_ = 0;
    std::array<result_type, word_count> buffer_{};
    std::size_t index_ = word_count;

    static std::uint64_t mix(std::uint64_t z)
    {
        z += 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    void generate_block()
    {
        // As in MockCBRNGTemplate: every output word depends on the whole counter.
        std::uint64_t acc = key_;
        for (std::size_t i = 0; i < word_count; ++i) {
            acc = mix(acc ^ (static_cast<std::uint64_t>(counter_[i]) << 32) ^ i);
        }
        for (std::size_t i = 0; i < word_count; ++i) {
            buffer_[i] = static_cast<result_type>(mix(acc ^ i));
        }
        for (std::size_t i = 0; i < word_count; ++i) {
            if (++counter_[i] != 0) {
                break;
            }
        }
        index_ = 0;
    }

  public:
    explicit MockCBRNGOverloaded(result_type s = 0)
    {
        seed(s);
    }
    void seed(result_type s)
    {
        key_ = s;
        counter_.fill(0);
        index_ = word_count;
    }
    // Overload set: the array form is what the keying helper calls, but the
    // presence of a second overload is what breaks address-of detection.
    void set_counter(const std::array<result_type, word_count> &c)
    {
        for (std::size_t i = 0; i < word_count; ++i) {
            counter_[i] = c[word_count - 1 - i];
        }
        index_ = word_count;
    }
    void set_counter(result_type low_word)
    {
        counter_.fill(0);
        counter_[0] = low_word;
        index_ = word_count;
    }
    static constexpr result_type min()
    {
        return 0;
    }
    static constexpr result_type max()
    {
        return UINT32_MAX;
    }
    result_type operator()()
    {
        if (index_ >= word_count) {
            generate_block();
        }
        return buffer_[index_++];
    }
};

} // namespace

// Trait assertions.  These pin the classification itself, which no test
// previously checked: an engine misclassified into the weaker tier still
// compiles and produces valid output, so only a static_assert catches it.
namespace
{
namespace tr = Qiskit::addon::sqd::internal;

// The two mocks are the regression test for the address-of detection bug: both
// were reported as *not* counter-based before the call-expression probe.
static_assert(
    tr::is_counter_based_rng_v<MockCBRNGTemplate>,
    "a counter-based engine with a template set_counter must be detected"
);
static_assert(
    tr::is_counter_based_rng_v<MockCBRNGOverloaded>,
    "a counter-based engine with an overloaded set_counter must be detected"
);
// Counter type resolution: mock 1 names its own, mock 2 has it derived.
static_assert(
    std::is_same_v<
        tr::counter_type_of<MockCBRNGTemplate>::type, MockCBRNGTemplate::counter_type
    >,
    "a nested counter_type must be taken at its word"
);
static_assert(
    std::is_same_v<
        tr::counter_type_of<MockCBRNGOverloaded>::type, std::array<std::uint32_t, 4>
    >,
    "absent a nested counter_type, the standard array shape must be derived"
);
// Ordinary engines must stay in the seedable tier.
static_assert(
    !tr::is_counter_based_rng_v<std::mt19937>, "mt19937 is not counter-based"
);
static_assert(
    !tr::is_counter_based_rng_v<MinimalURBG>, "MinimalURBG is not counter-based"
);
// The seedable/non-seedable boundary, pinned from both sides.  MinimalURBG is
// the generator the library supports in serial builds only; the trait returning
// false for it is what makes the OpenMP static_assert fire, and what makes the
// serial path skip the per-thread seeding it cannot perform.
static_assert(
    !tr::is_seedable_rng_v<MinimalURBG>, "MinimalURBG deliberately has no seed()"
);
static_assert(
    tr::is_seedable_rng_v<MinimalSeedableURBG>, "MinimalSeedableURBG provides seed()"
);
static_assert(
    !tr::is_counter_based_rng_v<MinimalSeedableURBG>,
    "MinimalSeedableURBG is not counter-based"
);
#if defined(__cpp_lib_philox_engine)
static_assert(
    tr::is_counter_based_rng_v<std::philox4x64>, "philox4x64 is counter-based"
);
static_assert(
    tr::is_counter_based_rng_v<std::philox4x32>, "philox4x32 is counter-based"
);
#endif

} // namespace

// The engine list, defined once and reused by every invocation below.  On C++26
// the counter-based std::philox_engine joins the same list.
#if defined(__cpp_lib_philox_engine)
#define RNG_PHILOX_IF_AVAILABLE , std::philox4x64
#else
#define RNG_PHILOX_IF_AVAILABLE
#endif

// A deliberately diverse set: both result_type widths (32- and 64-bit), the
// three base engine families (linear-congruential, Mersenne twister,
// subtract-with-carry), an adaptor engine (knuth_b is a shuffle_order adaptor),
// a hand-rolled minimal generator that adds nothing to the concept but seed(),
// two mock counter-based engines shaped unlike philox, and -- on C++26 -- the
// counter-based std::philox_engine.
//
// Every entry is usable in both build configurations, so these cases can run
// unconditionally.  The non-seedable MinimalURBG is therefore not a member: it is
// rejected at compile time under OpenMP by design, and is covered separately
// below.
#define RNG_ENGINE_LIST                                                                \
    std::mt19937, std::mt19937_64, std::minstd_rand, std::ranlux48_base, std::knuth_b, \
        MinimalSeedableURBG, MockCBRNGTemplate,                                        \
        MockCBRNGOverloaded RNG_PHILOX_IF_AVAILABLE

TEST_CASE_TEMPLATE_DEFINE(
    "recover_configurations accepts any uniform_random_bit_generator", RNGType,
    recover_substitutability
)
{
    // Runs to completion and produces a consistent (bitstrings, probs) pair.
    RNGType rng(12345u);
    auto [bs, probs] = run_recover(rng);
    CHECK(bs.size() == probs.size());
    CHECK(bs.size() >= 1);

    // Determinism: same engine, same seed -> identical result.  This is the
    // property the keyed / parallel path will depend on.
    RNGType rng_a(777u);
    RNGType rng_b(777u);
    auto ra = run_recover(rng_a);
    auto rb = run_recover(rng_b);
    CHECK(ra.first == rb.first);
    CHECK(ra.second == rb.second);
}
TEST_CASE_TEMPLATE_INVOKE(recover_substitutability, RNG_ENGINE_LIST);

TEST_CASE_TEMPLATE_DEFINE(
    "subsample_multiple_batches accepts any uniform_random_bit_generator", RNGType,
    subsample_substitutability
)
{
    constexpr unsigned int N = 4;
    std::vector<std::bitset<N>> bitstrings;
    bitstrings.reserve(5);
    for (unsigned int i = 0; i < 5; ++i) {
        bitstrings.emplace_back(i);
    }
    std::vector<double> weights{1, 2, 3, 4, 5};

    RNGType rng_a(42u);
    RNGType rng_b(42u);
    auto a = subsample_multiple_batches(bitstrings, weights, 4u, 2u, rng_a);
    auto b = subsample_multiple_batches(bitstrings, weights, 4u, 2u, rng_b);
    CHECK(a == b); // same engine + seed -> identical batches
}
TEST_CASE_TEMPLATE_INVOKE(subsample_substitutability, RNG_ENGINE_LIST);

// Substream keying: the properties the parallel path actually depends on.
//
// The pre-existing thread-count-independence test in
// test_configuration_recovery.cpp only compares whole-call results across thread
// counts, which keying by index satisfies for the wrong reasons too.  Replacing
// the index with a constant -- so every work item draws the identical stream --
// was measured to still pass it at 1/2/4/8 threads, and the resulting
// distribution still looks healthy from the outside (same count of unique
// bitstrings).  That is the danger: two items whose corrections are supposed to
// be independent draws instead making the same one, invisibly, biasing the
// refined distribution with no test or summary statistic complaining.
//
// So these cases assert the property directly rather than through an output
// comparison: distinct indices get distinct, non-overlapping randomness.  They
// are deliberately ungated -- the mocks make them run on every configuration,
// not just the C++26 row.

namespace
{

// How many substreams to key, and how deep to draw from each.
struct StreamSpec {
    std::size_t count;
    std::size_t draws;
};

// Key `spec.count` engines to indices 0..count-1, collecting `spec.draws`
// outputs from each.
template <typename RNGType>
std::vector<std::vector<typename RNGType::result_type>>
keyed_streams(std::uint64_t base_seed, StreamSpec spec)
{
    const std::size_t count = spec.count;
    const std::size_t draws = spec.draws;
    std::vector<std::vector<typename RNGType::result_type>> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        RNGType engine(1u);
        Qiskit::addon::sqd::internal::key_counter_based_rng(engine, base_seed, i);
        std::vector<typename RNGType::result_type> s;
        s.reserve(draws);
        for (std::size_t d = 0; d < draws; ++d) {
            s.push_back(engine());
        }
        out.push_back(std::move(s));
    }
    return out;
}

} // namespace

TEST_CASE_TEMPLATE_DEFINE(
    "keyed substreams are pairwise distinct", RNGType, substream_distinctness
)
{
    // Consecutive indices must not merely differ somewhere -- their *first*
    // outputs must differ, since a work item may draw only once.
    const auto streams = keyed_streams<RNGType>(0xfeedfacecafebeefULL, {64, 1});
    for (std::size_t i = 0; i < streams.size(); ++i) {
        for (std::size_t j = i + 1; j < streams.size(); ++j) {
            CHECK(streams[i][0] != streams[j][0]);
        }
    }
}
TEST_CASE_TEMPLATE_INVOKE(
    substream_distinctness, MockCBRNGTemplate,
    MockCBRNGOverloaded RNG_PHILOX_IF_AVAILABLE
);

TEST_CASE_TEMPLATE_DEFINE(
    "keyed substreams do not overlap under deep draws", RNGType, substream_no_overlap
)
{
    // The failure this targets: with the index in the *low* counter word, item i
    // and item i+1 are the same stream offset by word_count, so item i's draws
    // word_count..2*word_count-1 are item i+1's first outputs, bit for bit.
    // Drawing well past one block is what exposes it -- a shallow test cannot.
    constexpr std::size_t kDraws = 96;
    const auto streams = keyed_streams<RNGType>(0x0123456789abcdefULL, {8, kDraws});

    for (std::size_t i = 0; i + 1 < streams.size(); ++i) {
        for (std::size_t shift = 1; shift < kDraws; ++shift) {
            // Is streams[i+1] a shifted copy of streams[i]?  Require a decent
            // run length so an incidental single match is not flagged.
            const std::size_t overlap = kDraws - shift;
            if (overlap < 8) {
                break;
            }
            bool all_equal = true;
            for (std::size_t k = 0; k < overlap; ++k) {
                if (streams[i][shift + k] != streams[i + 1][k]) {
                    all_equal = false;
                    break;
                }
            }
            CHECK_MESSAGE(
                !all_equal, "stream ", i + 1, " is stream ", i, " shifted by ", shift
            );
        }
    }

    // Also assert the streams are not merely non-shifted but genuinely unrelated:
    // no single output value should repeat across the whole set at the same depth.
    for (std::size_t d = 0; d < kDraws; ++d) {
        for (std::size_t i = 0; i < streams.size(); ++i) {
            for (std::size_t j = i + 1; j < streams.size(); ++j) {
                CHECK(streams[i][d] != streams[j][d]);
            }
        }
    }
}
TEST_CASE_TEMPLATE_INVOKE(
    substream_no_overlap, MockCBRNGTemplate, MockCBRNGOverloaded RNG_PHILOX_IF_AVAILABLE
);

// The seed-narrowing path.  MockCBRNGOverloaded has a 32-bit result_type, so a
// plain static_cast of the 64-bit base seed would discard its high half and two
// seeds differing only above bit 32 would key identical streams.  fold_seed_to
// mixes first, so they do not.  Nothing else in the suite reaches this path.
TEST_CASE("a narrow-word engine still sees the whole 64-bit base seed")
{
    const std::uint64_t low_only = 0x00000000abcdef01ULL;
    const std::uint64_t with_high = 0x12345678abcdef01ULL; // same low 32 bits

    auto a = keyed_streams<MockCBRNGOverloaded>(low_only, {4, 8});
    auto b = keyed_streams<MockCBRNGOverloaded>(with_high, {4, 8});
    CHECK(a != b);
}

// The per-thread seeding path, for an engine that is not counter-based.
//
// This is the other half of the parallel path, and the same kind of gap as the
// keying tests above address: giving every thread the *same* seed makes all
// threads draw one identical stream, so items on different threads correlate
// rather than being independent.  A whole-output comparison does not notice --
// the output still changes with the thread count, because OpenMP hands each
// thread a different slice of the loop, so a cross-build or cross-thread diff
// stays green.  Measured: dropping the thread id from the seed expression in
// `recover_configurations` leaves the CI serial-vs-OpenMP check passing.
//
// So assert the property directly.  The expression mirrors the one in
// `recover_configurations`; the multiplier is the odd integer nearest
// 2^64 / golden-ratio, chosen so consecutive thread ids map to far-apart seeds.
TEST_CASE("per-thread seeds give each thread a distinct stream")
{
    constexpr std::uint64_t base_seed = 0x0123456789abcdefULL;
    constexpr int num_threads = 8;

    std::vector<std::vector<std::uint_fast32_t>> streams;
    streams.reserve(num_threads);
    for (int tid = 0; tid < num_threads; ++tid) {
        std::mt19937 rng;
        rng.seed(
            static_cast<std::mt19937::result_type>(
                base_seed +
                0x9e3779b97f4a7c15ULL * (static_cast<std::uint64_t>(tid) + 1)
            )
        );
        constexpr int depth = 16;
        std::vector<std::uint_fast32_t> draws;
        draws.reserve(depth);
        for (int i = 0; i < depth; ++i) {
            draws.push_back(rng());
        }
        streams.push_back(std::move(draws));
    }

    // Pairwise distinct, and no stream is a shifted copy of another -- the
    // failure mode an equal-seeds bug would produce.
    for (std::size_t a = 0; a < streams.size(); ++a) {
        for (std::size_t b = a + 1; b < streams.size(); ++b) {
            CHECK(streams[a] != streams[b]);
            for (std::size_t shift = 1; shift < streams[b].size(); ++shift) {
                const bool overlaps = std::equal(
                    streams[a].begin() + static_cast<std::ptrdiff_t>(shift),
                    streams[a].end(), streams[b].begin()
                );
                CHECK_FALSE(overlaps);
            }
        }
    }
}

// The non-seedable generator, which is supported in serial builds only.
//
// This is the one case that cannot join RNG_ENGINE_LIST: under OpenMP the
// static_assert in recover_configurations rejects a generator that is neither
// counter-based nor seedable, so instantiating it there would fail to compile --
// which is the documented and intended behavior.  Gating the case on _OPENMP
// keeps that promise under test in the configuration where it applies, rather
// than leaving the entire `!is_seedable_rng_v` branch uninstantiated everywhere,
// which is what happened while MinimalURBG still carried a seed() it did not
// need.
//
// Concretely this covers the seedability guard on the serial path's per-thread
// seeding: that path calls rng.seed() for an ordinary engine, so without the
// guard this generator would not compile in a serial build either.
#if !defined(_OPENMP)
TEST_CASE("recover_configurations accepts a generator with no seed()")
{
    static_assert(
        !tr::is_seedable_rng_v<MinimalURBG> && !tr::is_counter_based_rng_v<MinimalURBG>,
        "this case is only meaningful for a generator that is neither seedable nor "
        "counter-based"
    );

    MinimalURBG rng(12345u);
    auto [bs, probs] = run_recover(rng);
    CHECK(bs.size() == probs.size());
    CHECK(bs.size() >= 1);

    // Determinism still holds: the same seed reproduces the same result even
    // though the generator cannot be re-seeded after construction.
    MinimalURBG again(12345u);
    auto [bs2, probs2] = run_recover(again);
    CHECK(bs == bs2);
    CHECK(probs == probs2);
}
#endif
