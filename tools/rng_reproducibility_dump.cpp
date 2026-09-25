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

// Dump the output of recover_configurations for a fixed seed, so that a serial
// build and an OpenMP build can be compared against each other.
//
// This exists because the guarantee it checks cannot be expressed as a unit
// test.  A counter-based engine is keyed per work item identically in both code
// paths, so the two builds must agree -- but `_OPENMP` selects between them at
// compile time, so no single binary can compare them.  A golden value cannot
// stand in either: `std::discrete_distribution` consumes an
// implementation-defined number of engine draws, so the specific bitstrings
// recovered are not portable across standard libraries.  Comparing two builds of
// the *same* standard library sidesteps that entirely.
//
// Build this twice, once with OpenMP and once without, and diff the output; see
// the `openmp-tests` job in .github/workflows/test_development_versions.yml.
// The philox rows must match.  The mt19937 rows are expected to differ, since an
// ordinary engine is seeded per thread rather than keyed per item.

#include "qiskit/addon/sqd/configuration_recovery.hpp"

#include <array>
#include <bitset>
#include <cstdio>
#include <exception>
#include <random>
#include <vector>

namespace
{

// Emit one line per recovered bitstring, prefixed by the engine name.  Frequency
// is printed with enough digits to round-trip a double, so a diff catches any
// numerical difference and not merely a different set of bitstrings.
template <typename RNGType>
void dump(const char *label)
{
    constexpr unsigned int num_orbs = 8;
    constexpr unsigned int half = num_orbs / 2;

    std::vector<std::bitset<num_orbs>> bitstrings;
    std::vector<double> probs;
    for (unsigned int i = 0; i < 500; ++i) {
        bitstrings.emplace_back((i * 37u + 5u) & 0xffu);
        probs.push_back(1.0 + (i % 7));
    }
    std::array<std::vector<double>, 2> occs{
        std::vector<double>(half, 0.3), std::vector<double>(half, 0.7)
    };

    // A constant seed is the entire point: the two builds must be compared at
    // the same starting state.  (bugprone-random-generator-seed is disabled for
    // this directory in tools/.clang-tidy.)
    RNGType rng(2024u);
    const auto result = Qiskit::addon::sqd::recover_configurations(
        bitstrings, probs, occs, {2, 2}, rng
    );
    for (std::size_t i = 0; i < result.first.size(); ++i) {
        std::printf(
            "%s %s %.17g\n", label, result.first[i].to_string().c_str(),
            result.second[i]
        );
    }
}

} // namespace

int main()
{
    // Report a failure rather than letting an exception escape main().  The
    // library can be built with exceptions disabled, so this is compiled only
    // when they are available.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    try {
#endif
        dump<std::mt19937>("mt19937");
#if defined(__cpp_lib_philox_engine)
        dump<std::philox4x64>("philox4x64");
#endif
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    } catch (const std::exception &e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
#endif
    return 0;
}
