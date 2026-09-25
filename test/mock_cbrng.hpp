// This code is part of Qiskit.
//
// (C) Copyright IBM 2026.
//
// This code is licensed under the Apache License, Version 2.0. You may
// obtain a copy of this license in the LICENSE.txt file in the root directory
// of this source tree or at http://www.apache.org/licenses/LICENSE-2.0.
//
// Any modifications or derivative works of this code must retain this
// copyright notice, and modified files need to carry a notice indicating
// that they have been altered from the originals.

#ifndef MOCK_CBRNG_HPP_
#define MOCK_CBRNG_HPP_

// Mock counter-based generators, shared by the tests that exercise substream
// keying.  They live in a header (rather than in one test translation unit)
// because two test files need them: test_rng_substitutability.cpp asserts the
// keying properties directly, and test_configuration_recovery.cpp drives them
// end-to-end through recover_configurations at several thread counts.
//
// std::philox_engine exists only on C++26, so without these mocks the entire
// counter-based code path -- the headline feature -- would be compiled and
// exercised on exactly one row of the CI matrix.

#include <array>
#include <cstddef>
#include <cstdint>

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

// Mock 3: the shape that separates the engine's *word* width from the width of
// the type carrying it: a 64-bit result_type with 32-bit words, so `seed()`
// reduces the key mod 2^32 and `set_counter` reduces each word likewise.
//
// This is the only mock that exercises `key_bits_of`'s `word_size`
// specialization.  MockCBRNGTemplate exposes neither `word_size` nor
// `word_count`, and MockCBRNGOverloaded has `word_count` but no `word_size`, so
// both take the `sizeof(result_type) * 8` fallback and neither can detect a fold
// that narrows to the wrong width.
//
// It is also the *portable* guarantor of that coverage, which is why the widths
// here are written out rather than taken from philox.  `std::philox4x32` is
// `philox_engine<uint_fast32_t, 32, 4, 10>`, and `uint_fast32_t` is 64 bits on
// x86-64 glibc but only 32 on MSVC and some ARM ABIs.  Where it is 32 bits,
// philox4x32's carrier and word widths coincide and the tests that use it to
// check narrowing pass vacuously; this mock hard-codes the mismatch, so the
// coverage holds on every platform.
class MockCBRNGNarrowWord
{
  public:
    using result_type = std::uint64_t; // wider than a word, as in philox4x32
    static constexpr std::size_t word_count = 4;
    static constexpr int word_size = 32;

  private:
    static constexpr result_type word_mask =
        (static_cast<result_type>(1) << word_size) - 1;
    std::array<result_type, word_count> counter_{};
    result_type key_ = 0;
    std::array<result_type, word_count> buffer_{};
    std::size_t index_ = word_count;

    static result_type mix(result_type z)
    {
        z += 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    void generate_block()
    {
        result_type acc = key_;
        for (std::size_t i = 0; i < word_count; ++i) {
            acc = mix(acc ^ counter_[i] ^ static_cast<result_type>(i));
        }
        for (std::size_t i = 0; i < word_count; ++i) {
            buffer_[i] = mix(acc ^ mix(static_cast<result_type>(i))) & word_mask;
        }
        for (std::size_t i = 0; i < word_count; ++i) {
            counter_[i] = (counter_[i] + 1) & word_mask;
            if (counter_[i] != 0) {
                break;
            }
        }
        index_ = 0;
    }

  public:
    explicit MockCBRNGNarrowWord(result_type s = 0)
    {
        seed(s);
    }
    // Reduces the key mod 2^word_size, exactly as std::philox_engine does -- this
    // truncation is what makes folding the seed to the wrong width observable.
    void seed(result_type s)
    {
        key_ = s & word_mask;
        counter_.fill(0);
        index_ = word_count;
    }
    void set_counter(const std::array<result_type, word_count> &c)
    {
        for (std::size_t i = 0; i < word_count; ++i) {
            counter_[i] = c[word_count - 1 - i] & word_mask;
        }
        index_ = word_count;
    }
    static constexpr result_type min()
    {
        return 0;
    }
    static constexpr result_type max()
    {
        return word_mask;
    }
    result_type operator()()
    {
        if (index_ >= word_count) {
            generate_block();
        }
        return buffer_[index_++];
    }
};

// Mock 4: a word so narrow that `max_substreams()` is small enough to exceed in a
// test.  Every other engine here -- and both standard philox widths -- can key at
// least 2^32 substreams, so the workload guard in `recover_configurations` is
// unreachable with them: allocating 2^32 bitstrings to trip it is not a test.
//
// With `word_size = 8` the capacity is 256, so a few hundred bitstrings are enough
// to distinguish "accepted" from "rejected".  The mock is otherwise a working
// generator, so the accepted case really does run the correction to completion
// rather than merely failing to throw.
//
// An 8-bit word makes for a terrible RNG.  That is not the point: what is under
// test is the capacity arithmetic in `max_substreams()` and the guard that
// consumes it, neither of which depends on the quality of the stream.
class MockCBRNGTinyWord
{
  public:
    using result_type = std::uint64_t;
    static constexpr std::size_t word_count = 4;
    static constexpr int word_size = 8;

  private:
    static constexpr result_type word_mask =
        (static_cast<result_type>(1) << word_size) - 1;
    std::array<result_type, word_count> counter_{};
    result_type key_ = 0;
    std::array<result_type, word_count> buffer_{};
    std::size_t index_ = word_count;

    static result_type mix(result_type z)
    {
        z += 0x9e3779b97f4a7c15ULL;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    void generate_block()
    {
        result_type acc = key_;
        for (std::size_t i = 0; i < word_count; ++i) {
            acc = mix(acc ^ counter_[i] ^ static_cast<result_type>(i));
        }
        for (std::size_t i = 0; i < word_count; ++i) {
            buffer_[i] = mix(acc ^ mix(static_cast<result_type>(i))) & word_mask;
        }
        for (std::size_t i = 0; i < word_count; ++i) {
            counter_[i] = (counter_[i] + 1) & word_mask;
            if (counter_[i] != 0) {
                break;
            }
        }
        index_ = 0;
    }

  public:
    explicit MockCBRNGTinyWord(result_type s = 0)
    {
        seed(s);
    }
    void seed(result_type s)
    {
        key_ = s & word_mask;
        counter_.fill(0);
        index_ = word_count;
    }
    void set_counter(const std::array<result_type, word_count> &c)
    {
        for (std::size_t i = 0; i < word_count; ++i) {
            counter_[i] = c[word_count - 1 - i] & word_mask;
        }
        index_ = word_count;
    }
    static constexpr result_type min()
    {
        return 0;
    }
    static constexpr result_type max()
    {
        return word_mask;
    }
    result_type operator()()
    {
        if (index_ >= word_count) {
            generate_block();
        }
        return buffer_[index_++];
    }
};

#endif // MOCK_CBRNG_HPP_
