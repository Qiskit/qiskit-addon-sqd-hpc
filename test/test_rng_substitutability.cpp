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
#include "qiskit/addon/sqd/subsampling.hpp"

#include <array>
#include <bitset>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

using Qiskit::addon::sqd::recover_configurations;
using Qiskit::addon::sqd::subsample_multiple_batches;

namespace
{

// A minimal std::uniform_random_bit_generator: nothing beyond what the concept
// requires (result_type, min, max, operator()).  A SplitMix64 step keeps it
// self-contained and well-distributed without pulling in <random> engines.
class MinimalURBG
{
    std::uint64_t state_;

  public:
    using result_type = std::uint64_t;
    explicit MinimalURBG(std::uint64_t seed) : state_(seed)
    {
    }
    // A seed() so this exercises the OpenMP per-thread path too; realistic
    // engines all provide one.
    void seed(result_type s)
    {
        state_ = s;
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
// a hand-rolled minimal generator that models nothing beyond the concept, and
// -- on C++26 -- the counter-based std::philox_engine.
#define RNG_ENGINE_LIST                                                                \
    std::mt19937, std::mt19937_64, std::minstd_rand, std::ranlux48_base, std::knuth_b, \
        MinimalURBG RNG_PHILOX_IF_AVAILABLE

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
