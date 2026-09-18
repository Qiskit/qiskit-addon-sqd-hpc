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
//     of which thread runs it.  Results are then identical for any thread count.
//
//   * An ordinary engine is given one independently-seeded copy per thread.
//     Results are valid but depend on the thread count (a work item's stream
//     depends on which thread happens to run it).
//
// These helpers are generic and intended to be reused by future distributed
// (e.g. MPI) work, where the item index becomes a global index.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

/// Trait: is `R` a counter-based engine, i.e. addressable by counter?
///
/// Detected by the presence of a `set_counter` member (which the C++26
/// std::philox_engine has and ordinary engines do not).  Specialize this for a
/// counter-based engine that spells its interface differently.
template <typename R, typename = void>
struct is_counter_based_rng : std::false_type {
};

template <typename R>
struct is_counter_based_rng<R, std::void_t<decltype(&R::set_counter)>>
  : std::true_type {
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

/// Seed a counter-based engine to a base seed and position it at the stream
/// keyed by `index` (the low word of its counter).  Item `index` then draws an
/// independent stream, addressable regardless of which thread computes it.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
template <typename RNGType>
void key_counter_based_rng(RNGType &engine, std::uint64_t base_seed, std::size_t index)
{
    engine.seed(static_cast<typename RNGType::result_type>(base_seed));
    std::array<typename RNGType::result_type, RNGType::word_count> counter{};
    counter[0] = static_cast<typename RNGType::result_type>(index);
    engine.set_counter(counter);
}

} // namespace internal

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // QISKIT_ADDON_SQD_INTERNAL_PARALLEL_RNG_HPP_
