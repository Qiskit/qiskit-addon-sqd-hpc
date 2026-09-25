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

#ifndef QISKIT_ADDON_SQD_INTERNAL_PARALLEL_RNG_HPP_
#define QISKIT_ADDON_SQD_INTERNAL_PARALLEL_RNG_HPP_

// Helpers for giving each unit of parallel work its own independent random
// stream, so a shared generator need not be serialized across threads.
//
// Two reproducibility tiers, selected automatically by the caller's RNG type:
//
//   * A counter-based engine (e.g. C++26 std::philox_engine) is addressable by
//     index via set_counter, so work-item i can draw from stream i regardless
//     of which thread runs it.  Results are then identical for any thread count,
//     and the serial path keys items identically, so a serial build and an
//     OpenMP build agree as well.
//
//   * An ordinary engine is given one independently-seeded copy per thread.
//     Results are valid but depend on the thread count (a work item's stream
//     depends on which thread happens to run it).
//
// These helpers are generic and intended to be reused by future distributed
// (e.g. MPI) work, where the item index becomes a global index.
//
// What a counter-based engine must provide for the strong tier:
//
//   * `result_type`, and `seed(result_type)` to set its key.  The key space is
//     bounded by the engine's `word_size` when it declares one, which may be
//     narrower than `result_type` (std::philox4x32: 64-bit result_type, 32-bit
//     words) -- see `key_bits_of`;
//   * a counter type, either named as a nested `counter_type` or (the standard
//     shape) implied by a static `word_count` alongside `result_type`, in which
//     case it is `std::array<result_type, word_count>`;
//   * `set_counter(const counter_type &)` following the std::philox_engine
//     semantics described at `substream_keying` below.
//
// An engine whose counter is shaped or ordered differently specializes
// `substream_keying` rather than editing the helpers here.

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

/// Trait: does `R` name its own counter type?
template <typename R, typename = void>
struct has_counter_type_member : std::false_type {
};

template <typename R>
struct has_counter_type_member<R, std::void_t<typename R::counter_type>>
  : std::true_type {
};

/// Trait: the type `R` accepts in `set_counter`.
///
/// An engine that names a nested `counter_type` is taken at its word;
/// otherwise the standard shape `std::array<result_type, word_count>` is
/// derived.  `type` is absent when `R` is not counter-shaped at all, which is
/// what makes `is_counter_based_rng` below SFINAE cleanly rather than hard
/// error.
template <typename R, typename = void>
struct counter_type_of {
};

/// Specialization for an engine that names its own `counter_type`.
template <typename R>
struct counter_type_of<R, std::enable_if_t<has_counter_type_member<R>::value>> {
    /// The engine's own counter type, taken at its word.
    using type = typename R::counter_type;
};

// The `!has_counter_type_member` guard is load-bearing, not stylistic: without
// it, an engine providing *both* `word_count` and `counter_type` makes the two
// partial specializations ambiguous ("ambiguous template instantiation").
/// Specialization deriving the standard counter shape from `word_count`.
template <typename R>
struct counter_type_of<
    R, std::enable_if_t<
           !has_counter_type_member<R>::value &&
           std::is_integral<std::remove_cv_t<decltype(R::word_count)>>::value
       >
> {
    /// The standard shape implied by `result_type` and `word_count`.
    using type = std::array<typename R::result_type, R::word_count>;
};

/// Trait: how many bits of key the engine actually uses.
///
/// This is *not* `sizeof(result_type) * 8`.  A counter-based engine's key and
/// counter words are `word_size` bits wide, and `word_size` may be narrower than
/// the type that carries them: `std::philox4x32` has a 64-bit `result_type` but a
/// 32-bit `word_size`, and its `seed()` reduces the key mod `2^word_size`.  Using
/// the type's width instead would let two base seeds that differ only above
/// `word_size` key the *same* substream.
///
/// An engine exposing `word_size` is taken at its word; otherwise the carrier
/// type's width is the best available bound.
template <typename R, typename = void>
struct key_bits_of {
    /// Fallback: the full width of the type carrying the key.
    static constexpr int value = static_cast<int>(sizeof(typename R::result_type) * 8);
};

/// Specialization for an engine that declares its own word width.
template <typename R>
struct key_bits_of<
    R,
    std::enable_if_t<std::is_integral<std::remove_cv_t<decltype(R::word_size)>>::value>
> {
    /// The engine's own word width, which bounds its key space.
    static constexpr int value = static_cast<int>(R::word_size);
};

/// Trait: is `R` a counter-based engine, i.e. addressable by counter?
///
/// Detected by whether `set_counter` is *callable* with the engine's counter
/// type.  Note this deliberately probes a call expression rather than
/// `decltype(&R::set_counter)`: taking a member's address is ill-formed for an
/// overload set and for a member template, so the address form silently
/// misclassifies such an engine as ordinary -- it would still compile and run,
/// but drop to the weaker thread-count-dependent tier with no diagnostic.
/// Specialize this for a counter-based engine that spells its interface
/// differently.
template <typename R, typename = void>
struct is_counter_based_rng : std::false_type {
};

template <typename R>
struct is_counter_based_rng<
    R, std::void_t<decltype(std::declval<R &>().set_counter(
           std::declval<const typename counter_type_of<R>::type &>()
       ))>
> : std::true_type {
};

template <typename R>
inline constexpr bool is_counter_based_rng_v = is_counter_based_rng<R>::value;

/// Trait: can `R` be re-seeded from a single value, i.e. does it have a
/// `seed(result_type)` member?  Standard random number engines do; a
/// concept-only `uniform_random_bit_generator` need not.  This is what the
/// per-thread (weak-reproducibility) parallel path requires.
template <typename R, typename = void>
struct is_seedable_rng : std::false_type {
};

template <typename R>
struct is_seedable_rng<
    R, std::void_t<decltype(std::declval<R &>()
                                .seed(std::declval<typename R::result_type>()))>
> : std::true_type {
};

template <typename R>
inline constexpr bool is_seedable_rng_v = is_seedable_rng<R>::value;

/// Mix a 64-bit seed into the low `bits` bits of `T`, so that every input bit
/// can affect the result even when the engine's key space is narrower than 64
/// bits.
///
/// Two separate narrowings are at play, and only doing both is correct:
///
///   * the SplitMix64 finalizer, whose avalanche spreads every input bit across
///     all 64 output bits; and
///   * an explicit XOR-fold of everything above `bits` back down into the low
///     `bits`, before the truncating conversion.
///
/// The XOR-fold is not redundant with the avalanche, which is the subtle part.
/// SplitMix64 ends in `z ^= z >> 31`, so its output bits are already mixed -- but
/// mixed is not the same as *reduced*, and the engine reduces the key mod
/// `2^bits` regardless.  Two seeds can avalanche to values that agree in their
/// low `bits` and differ only above: masking alone then keys the identical
/// substream.  (Measured: base seeds 149694 and 149778 produce bit-identical
/// `std::philox4x32` streams if the high half is merely dropped or masked off.)
/// Folding the discarded part in first makes it contribute instead.
///
/// When `bits` is at least 64 nothing is discarded and this is exactly the
/// SplitMix64 finalizer, so engines with 64-bit words are unaffected.
///
/// Used for the *key* only -- never for the item index, which must stay an exact
/// address (see `substream_keying::key`).
template <typename T>
constexpr T fold_seed_to_bits(std::uint64_t z, int bits) noexcept
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    if (bits < 64) {
        // Fold the part that seed() would discard back into the part it keeps.
        z ^= z >> bits;
        z &= (static_cast<std::uint64_t>(1) << bits) - 1;
    }
    return static_cast<T>(z);
}

/// Position a counter-based engine on the independent substream belonging to
/// `index`, keyed by `base_seed`.  Item `index` then draws the same stream no
/// matter which thread (or, later, which rank) computes it.
///
/// The default implementation follows std::philox_engine's `set_counter`
/// semantics, which are worth stating because they are easy to get backwards:
/// the counter is one large integer `Z = sum_j X_j * 2^(w*j)` incremented once
/// per block of `word_count` outputs, and `set_counter(c)` assigns it in
/// *reverse* word order (`X_k = c[n-1-k]`).  So `c[0]` is the MOST significant
/// word.
///
/// The index goes in that most significant word, giving each item a stride of
/// `2^(w*(n-1))` counter values -- 2^192 for philox4x64 -- instead of the
/// `word_count` outputs that adjacent counter values would afford.  That
/// headroom is required rather than merely generous: a work item's draw count is
/// unbounded (`NoReplacementSampler` rejection-samples) and the draws consumed
/// per sample by `std::discrete_distribution` are implementation-defined, so no
/// fixed per-item budget can be assumed.  With adjacent counters, item `i`'s
/// draws `n..2n-1` would be item `i+1`'s first `n` draws, bit for bit.
///
/// Specialize this for an engine whose counter is ordered or shaped differently.
template <typename R, typename = void>
struct substream_keying {
    /// Key `engine` to the substream belonging to work item `index`.
    ///
    /// @param[in,out] engine Engine to position.
    /// @param[in] base_seed Seed shared by every substream of this call.
    /// @param[in] index Index of the work item, which selects the substream.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    static void key(R &engine, std::uint64_t base_seed, std::uint64_t index)
    {
        using result_type = typename R::result_type;
        using counter_type = typename counter_type_of<R>::type;
        static_assert(
            is_seedable_rng_v<R>,
            "A counter-based engine must also provide seed(result_type) to set "
            "its key; specialize substream_keying for an engine that does not."
        );
        // A single-word counter would make `index` the *entire* counter below,
        // giving consecutive items a stride of one counter value -- immediate
        // overlap, which is exactly what keying by counter exists to prevent.
        // No standard philox is shaped that way, but this is the documented
        // generic extension point, so an engine that is must specialize
        // `substream_keying` rather than silently alias.
        static_assert(
            std::tuple_size<counter_type>::value >= 2,
            "A single-word counter leaves no room to separate substreams; "
            "specialize substream_keying for such an engine."
        );
        // The key space is the engine's own, not 64 bits, and not the width of
        // `result_type` either: for std::philox_engine the scalar seed() sets K_0
        // and zeros the remaining key words, so it reaches `word_size` bits --
        // which for philox4x32 is 32, half of its 64-bit result_type.  See
        // `key_bits_of`.
        constexpr int key_bits = key_bits_of<R>::value;
        engine.seed(fold_seed_to_bits<result_type>(base_seed, key_bits));
        counter_type counter{};
        // `index` is an address, not entropy: it is assigned verbatim so that
        // distinct items are *guaranteed* distinct substreams.  Hashing it here
        // would reduce that guarantee to a birthday argument, which is the very
        // thing keying by counter avoids.
        //
        // The bound is the engine's word width, not the width of `result_type`:
        // `set_counter` takes each word mod `2^word_size`, so for philox4x32 the
        // indices 7 and 7 + 2^32 name the same substream even though both fit a
        // 64-bit result_type.  `recover_configurations` enforces this up front
        // too, since assert() vanishes under NDEBUG and a release build must not
        // alias silently; this remains as the backstop for other callers.
        // Written as a mask rather than `1 << key_bits` so the expression is
        // well-formed at key_bits == 64, where the shift would overflow.
        constexpr std::uint64_t index_mask =
            key_bits >= 64 ? ~static_cast<std::uint64_t>(0)
                           : (static_cast<std::uint64_t>(1) << (key_bits % 64)) - 1;
        assert(
            (index & ~index_mask) == 0 &&
            "work-item index does not fit in the engine's word; substreams would alias"
        );
        counter[0] = static_cast<result_type>(index);
        engine.set_counter(counter);
    }
};

/// Seed a counter-based engine to a base seed and position it at the stream
/// keyed by `index`.  Thin forwarder to `substream_keying`, which owns the
/// counter layout.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
template <typename RNGType>
void key_counter_based_rng(RNGType &engine, std::uint64_t base_seed, std::size_t index)
{
    substream_keying<RNGType>::key(
        engine, base_seed, static_cast<std::uint64_t>(index)
    );
}

/// How many distinct substreams `RNGType` can address, or 0 for "more than any
/// `std::size_t` can count".
///
/// A caller keying work items by index uses this to reject a workload too large
/// to key *before* starting it.  The in-`key` assert cannot serve that purpose on
/// its own: it vanishes under NDEBUG, and it fires inside the parallel region
/// where throwing is not an option.
///
/// Returning 0 for the unbounded case keeps the comparison honest -- computing
/// `1 << 64` to compare against would be undefined behavior.
template <typename RNGType>
constexpr std::size_t max_substreams() noexcept
{
    constexpr int bits = key_bits_of<RNGType>::value;
    // `if constexpr`, not a plain `if`: on the unbounded branch the shift below
    // would be ill-formed (shift count >= width), and a runtime `if` still
    // compiles both arms.
    if constexpr (bits >= static_cast<int>(sizeof(std::size_t) * 8)) {
        return 0; // no std::size_t index can exceed the engine's word
    } else {
        return static_cast<std::size_t>(1) << bits;
    }
}

} // namespace internal

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_INTERNAL_PARALLEL_RNG_HPP_
