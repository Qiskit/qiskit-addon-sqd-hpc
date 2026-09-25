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

#include "mock_cbrng.hpp"

#include "qiskit/addon/sqd/configuration_recovery.hpp"
#include "qiskit/addon/sqd/internal/parallel-rng.hpp"
#include "qiskit/addon/sqd/subsampling.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <tuple>
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
// The mock that separates word width from carrier width, which is the shape that
// makes a wrongly-narrowed seed fold observable.
static_assert(
    tr::is_counter_based_rng_v<MockCBRNGNarrowWord>,
    "MockCBRNGNarrowWord is counter-based"
);
// The trait must report the engine's *word* width, not the width of the type
// carrying it.  Getting this backwards is the bug the fold and the index check
// both depended on.
static_assert(
    tr::key_bits_of<MockCBRNGNarrowWord>::value == 32,
    "an engine declaring word_size must be taken at its word"
);
static_assert(
    sizeof(MockCBRNGNarrowWord::result_type) * 8 == 64,
    "...and that word width must differ from the carrier width, or this mock "
    "would not be testing anything"
);
// The engines with no word_size fall back to the carrier width.
static_assert(
    tr::key_bits_of<MockCBRNGTemplate>::value == 64,
    "absent word_size, the carrier width is the bound"
);
static_assert(
    tr::key_bits_of<MockCBRNGOverloaded>::value == 32,
    "absent word_size, the carrier width is the bound"
);
#if defined(__cpp_lib_philox_engine)
// The real engine that motivated all of this: 64-bit result_type, 32-bit words.
static_assert(
    tr::key_bits_of<std::philox4x32>::value == 32,
    "philox4x32 keys 32 bits despite its 64-bit result_type"
);
static_assert(tr::key_bits_of<std::philox4x64>::value == 64, "philox4x64 keys 64 bits");
#endif
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
// philox4x32 separately: it is the standard engine whose word_size (32) is
// narrower than its result_type (64 bits), so it is the real-engine counterpart
// to MockCBRNGNarrowWord and belongs in the cases about key width.
#define RNG_PHILOX32_IF_AVAILABLE , std::philox4x32
#else
#define RNG_PHILOX_IF_AVAILABLE
#define RNG_PHILOX32_IF_AVAILABLE
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
    substream_distinctness, MockCBRNGTemplate, MockCBRNGOverloaded,
    MockCBRNGNarrowWord RNG_PHILOX_IF_AVAILABLE RNG_PHILOX32_IF_AVAILABLE
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
    substream_no_overlap, MockCBRNGTemplate, MockCBRNGOverloaded,
    MockCBRNGNarrowWord RNG_PHILOX_IF_AVAILABLE RNG_PHILOX32_IF_AVAILABLE
);

// The seed-narrowing path.  MockCBRNGOverloaded has a 32-bit result_type, so a
// plain static_cast of the 64-bit base seed would discard its high half and two
// seeds differing only above bit 32 would key identical streams.
// fold_seed_to_bits mixes first, so they do not.
TEST_CASE_TEMPLATE_DEFINE(
    "a narrow-word engine still sees the whole 64-bit base seed", RNGType,
    narrow_word_seed
)
{
    const std::uint64_t low_only = 0x00000000abcdef01ULL;
    const std::uint64_t with_high = 0x12345678abcdef01ULL; // same low 32 bits

    auto a = keyed_streams<RNGType>(low_only, {4, 8});
    auto b = keyed_streams<RNGType>(with_high, {4, 8});
    CHECK(a != b);
}
TEST_CASE_TEMPLATE_INVOKE(
    narrow_word_seed, MockCBRNGOverloaded, MockCBRNGNarrowWord RNG_PHILOX32_IF_AVAILABLE
);

// The specific seed pair that the earlier implementation keyed to *identical*
// streams, and why a generic "two different seeds differ" case did not catch it.
//
// The old code folded the base seed to `sizeof(result_type)` bits rather than to
// the engine's `word_size`.  For an engine whose words are narrower than its
// result_type -- philox4x32, and MockCBRNGNarrowWord here -- that fold is a no-op
// and seed() then simply truncates, so any two seeds agreeing in their low 32
// bits collide.
//
// Masking the mixed value to word_size instead of folding it does *not* fix this,
// which is why the pair is pinned explicitly: the SplitMix64 finalizer ends in
// `z ^= z >> 31`, so these two seeds avalanche to values that agree in their low
// 32 bits and differ only above.  Masking keeps exactly the bits that already
// match; only XOR-folding the discarded half back down separates them.
TEST_CASE_TEMPLATE_DEFINE(
    "seeds colliding in the low word still key distinct substreams", RNGType,
    narrow_word_seed_collision
)
{
    // Chosen so that *truncating* SplitMix64 to 32 bits maps both seeds to the
    // same key, while folding to 32 bits separates them.
    //
    // The distinction matters, and an earlier version of this case got it wrong: a
    // pair that merely shares its low 32 bits is not enough.  The correct fold's
    // `z ^= z >> 32` mixes the high half down, so such a pair comes apart anyway --
    // and so does a truncation-only implementation, because the engine itself
    // reduces the key mod 2^32.  A pair chosen that way therefore passes whether or
    // not `key_bits_of` respects `word_size`, which is the very bug this case
    // exists to catch.  This pair collides in the *keyed result* under truncation.
    constexpr std::uint64_t seed_a = 65336;
    constexpr std::uint64_t seed_b = 81207;

    namespace tr = Qiskit::addon::sqd::internal;

    // Precondition 1: truncation really does collide these seeds, so a fold that
    // ignored `word_size` would alias them.  If a future change to the mixer breaks
    // this, the case would silently stop testing anything.
    const auto mixed_a = tr::fold_seed_to_bits<std::uint64_t>(seed_a, 64);
    const auto mixed_b = tr::fold_seed_to_bits<std::uint64_t>(seed_b, 64);
    REQUIRE(mixed_a != mixed_b);
    REQUIRE_MESSAGE(
        (mixed_a & 0xffffffffULL) == (mixed_b & 0xffffffffULL),
        "truncating the mixed seeds to 32 bits no longer collides this pair, so "
        "this case would pass for the wrong reason; pick a new pair"
    );

    // Precondition 2: the 32-bit fold separates them.  Together with the above,
    // this is what makes the checks below sensitive to `key_bits_of` respecting
    // `word_size` rather than to the engine's own truncation.
    REQUIRE(
        tr::fold_seed_to_bits<std::uint32_t>(seed_a, 32) !=
        tr::fold_seed_to_bits<std::uint32_t>(seed_b, 32)
    );

    auto a = keyed_streams<RNGType>(seed_a, {4, 16});
    auto b = keyed_streams<RNGType>(seed_b, {4, 16});
    CHECK(a != b);
    // Not merely different somewhere: the very first output of each substream must
    // differ, since a work item may draw only once.
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i][0] != b[i][0]);
    }
}
TEST_CASE_TEMPLATE_INVOKE(
    narrow_word_seed_collision, MockCBRNGNarrowWord RNG_PHILOX32_IF_AVAILABLE
);

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

// ---------------------------------------------------------------------------
// The substream *capacity* guard.
//
// A counter-based engine keys work item i by writing i into a counter word, so it
// can address only 2^word_size items; beyond that two items alias onto one
// substream and silently draw identical randomness.  `recover_configurations`
// rejects such a workload up front.
//
// Nothing else in the suite can reach that guard.  Both philox widths and the
// other mocks key at least 2^32 substreams, and allocating 2^32 bitstrings to trip
// the check is not a test.  MockCBRNGTinyWord exists for this: `word_size = 8`
// puts the capacity at 256, so a few hundred bitstrings straddle it.

namespace
{

// Run `recover_configurations` over `count` bitstrings with a generator of type
// `RNGType`.  Bitstrings repeat once `count` exceeds the 8-bit space, which is
// fine: the guard is about how many *work items* there are, not how many are
// distinct.
template <typename RNGType>
std::pair<std::vector<std::bitset<8>>, std::vector<double>>
run_recover_n(RNGType &rng, std::size_t count)
{
    constexpr unsigned int N = 8;
    constexpr unsigned int half = N / 2;
    std::vector<std::bitset<N>> bitstrings;
    bitstrings.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        // Hamming weight 2 per half, so no correction is unsatisfiable.
        bitstrings.emplace_back(
            static_cast<unsigned long long>(0x33u + (i % 4u) * 0x11u)
        );
    }
    std::vector<double> probabilities(bitstrings.size(), 1.0);
    std::array<std::vector<double>, 2> occs{
        std::vector<double>(half, 0.4), std::vector<double>(half, 0.6)
    };
    return recover_configurations(bitstrings, probabilities, occs, {2, 2}, rng);
}

} // namespace

TEST_CASE("max_substreams reports the engine's addressable capacity")
{
    namespace tr = Qiskit::addon::sqd::internal;

    // 8-bit words: 256 substreams, small enough to exceed in a test.
    static_assert(tr::key_bits_of<MockCBRNGTinyWord>::value == 8, "");
    static_assert(tr::max_substreams<MockCBRNGTinyWord>() == 256, "");

    // 32-bit words fit a 64-bit size_t, so the capacity is finite but unreachable.
    static_assert(
        tr::max_substreams<MockCBRNGNarrowWord>() == (std::size_t{1} << 32), ""
    );

    // A word at least as wide as size_t cannot be overflowed by any index, which
    // `max_substreams` reports as 0 rather than computing an overflowing 1 << 64.
    static_assert(tr::max_substreams<MockCBRNGTemplate>() == 0, "");

    // The guard consuming this must treat 0 as "unbounded", not as "capacity
    // zero" -- a sign inversion there would reject every workload.
    CHECK(tr::max_substreams<MockCBRNGTemplate>() == 0);
}

// Gated on exceptions: with QKA_SQD_DISABLE_EXCEPTIONS the throw macros call
// std::terminate(), which cannot be caught by design.
#if !QKA_SQD_DISABLE_EXCEPTIONS
TEST_CASE("recover_configurations rejects more work items than the engine can key")
{
    namespace tr = Qiskit::addon::sqd::internal;
    constexpr std::size_t capacity = tr::max_substreams<MockCBRNGTinyWord>();
    static_assert(capacity == 256, "this case assumes an 8-bit counter word");

    // Just inside the capacity: accepted, and the correction really runs.
    {
        MockCBRNGTinyWord rng(4u);
        auto [bs, probs] = run_recover_n(rng, capacity - 56);
        CHECK(bs.size() == probs.size());
        CHECK(bs.size() >= 1);
    }

    // Exactly at the capacity: still accepted.  The largest index used is
    // capacity - 1, which fits the word, so the boundary must not be off by one.
    {
        MockCBRNGTinyWord rng(4u);
        auto [bs, probs] = run_recover_n(rng, capacity);
        CHECK(bs.size() == probs.size());
        CHECK(bs.size() >= 1);
    }

    // One past the capacity: rejected, because index `capacity` would alias onto
    // index 0.
    {
        MockCBRNGTinyWord rng(4u);
        CHECK_THROWS_AS(run_recover_n(rng, capacity + 1), std::invalid_argument);
    }

    // Well past it, to confirm the check is a comparison and not an exact match.
    {
        MockCBRNGTinyWord rng(4u);
        CHECK_THROWS_AS(run_recover_n(rng, capacity + 300), std::invalid_argument);
    }

    // An engine with a wide counter word accepts the same workload that the
    // narrow one rejected, so the rejection is a property of the engine's
    // capacity rather than of the workload size.
    {
        MockCBRNGTemplate rng(4u);
        auto [bs, probs] = run_recover_n(rng, capacity + 300);
        CHECK(bs.size() == probs.size());
        CHECK(bs.size() >= 1);
    }
}
#endif // !QKA_SQD_DISABLE_EXCEPTIONS

// `substream_keying` is the documented specialization point for an engine whose
// counter is shaped or ordered differently, so a user can reach it.  These cases
// pin the contract it must satisfy: the default keys through it, and a
// specialization actually displaces that default.

namespace
{

// An engine identical to MockCBRNGTemplate, distinct only as a type, so a
// specialization of substream_keying can be attached to it without affecting the
// engines used elsewhere in this file.
class MockCBRNGCustomKeyed : public MockCBRNGTemplate
{
  public:
    using MockCBRNGTemplate::MockCBRNGTemplate;
};

} // namespace

namespace Qiskit
{
namespace addon
{
namespace sqd
{
namespace internal
{

// Keys the index into the *least* significant counter word rather than the most.
// This is a deliberately poor layout -- it gives adjacent items a stride of one
// counter value -- but it is observably different from the default, which is what
// the test needs.
//
// The key is derived exactly as the default does, so the counter layout is the
// *only* difference between this and the default.  Seeding differently here would
// make the comparison below pass for the wrong reason: it would detect the changed
// seed rather than the changed layout, and would still pass if the specialization
// were never consulted at all.
template <>
struct substream_keying<MockCBRNGCustomKeyed> {
    // The signature must match the primary template, which carries the same
    // suppression for the same reason.
    static void
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    key(MockCBRNGCustomKeyed &engine, std::uint64_t base_seed, std::uint64_t index)
    {
        using result_type = MockCBRNGCustomKeyed::result_type;
        engine.seed(
            fold_seed_to_bits<result_type>(
                base_seed, key_bits_of<MockCBRNGCustomKeyed>::value
            )
        );
        MockCBRNGCustomKeyed::counter_type counter{};
        counter[std::tuple_size<MockCBRNGCustomKeyed::counter_type>::value - 1] = index;
        engine.set_counter(counter);
    }
};

} // namespace internal
} // namespace sqd
} // namespace addon
} // namespace Qiskit

TEST_CASE("a substream_keying specialization displaces the default")
{
    namespace tr = Qiskit::addon::sqd::internal;

    // The specialization must not change how the engine is classified: it is the
    // keying that is being replaced, not the detection.
    static_assert(tr::is_counter_based_rng_v<MockCBRNGCustomKeyed>, "");

    constexpr std::uint64_t seed = 0x0123456789abcdefULL;

    // The custom layout writes a different counter word than the default, so the
    // resulting streams must differ.  Were the specialization ignored, these would
    // be identical.
    MockCBRNGCustomKeyed custom(1u);
    tr::key_counter_based_rng(custom, seed, std::size_t{5});

    MockCBRNGTemplate def(1u);
    tr::key_counter_based_rng(def, seed, std::size_t{5});

    CHECK(custom() != def());

    // The specialization is still required to give distinct items distinct
    // streams, which is the contract every keying must honor.
    MockCBRNGCustomKeyed a(1u);
    MockCBRNGCustomKeyed b(1u);
    tr::key_counter_based_rng(a, seed, std::size_t{5});
    tr::key_counter_based_rng(b, seed, std::size_t{6});
    CHECK(a() != b());
}
