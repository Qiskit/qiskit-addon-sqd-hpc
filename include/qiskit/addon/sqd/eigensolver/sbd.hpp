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

#ifndef QISKIT_ADDON_SQD_SUBSAMPLING_HPP_
#define QISKIT_ADDON_SQD_SUBSAMPLING_HPP_

/// Support for SBD eigensolver

#include <string>
#include <vector>

#include "qiskit/addon/sqd/bitset_full.hpp"

#if __has_include(<sbd/sbd.h>)

#include <mpi.h>
#include <sbd/sbd.h>

namespace Qiskit
{

namespace addon
{

namespace sqd
{

namespace internal
{

}

template <typename BitstringVectorType>
struct SBDResult {
    const double energy;
    const std::vector<double> density;
    BitstringVectorType carryover_bitstrings;
    const std::vector<std::vector<double>> one_p_rdm, two_p_rdm;
};

template <typename BitstringVectorType>
SBDResult<BitstringVectorType> sbd_solve(
    const MPI_Comm &comm, const sbd::tpb::SBD &sbd_data, const sbd::FCIDump &fcidump,
    const BitstringVectorType &alpha_determinants,
    const BitstringVectorType &beta_determinants, const std::string &loadname = "",
    const std::string &savename = ""
)
{
    // Populate adet and bdet in storage format that sbd expects
    std::vector<std::vector<size_t>> adet, bdet;
    adet.reserve(alpha_determinants.size());
    for (const auto &bitstring : alpha_determinants) {
        adet.push_back(
            sbd::from_string(
                internal::bitset_to_string(bitstring), sbd_data.bit_length,
                bitstring.size()
            )
        );
    }
    bdet.reserve(beta_determinants.size());
    for (const auto &bitstring : beta_determinants) {
        bdet.push_back(
            sbd::from_string(
                internal::bitset_to_string(bitstring), sbd_data.bit_length,
                bitstring.size()
            )
        );
    }

    // Call sbd
    double energy;
    std::vector<double> density;
    std::vector<std::vector<size_t>> carryover_sbd_bitstrings;
    std::vector<std::vector<double>> one_p_rdm, two_p_rdm;
    sbd::tpb::diag(
        comm, sbd_data, fcidump, adet, bdet, loadname, savename, energy, density,
        carryover_sbd_bitstrings, one_p_rdm, two_p_rdm
    );

    // Convert carryover bitstrings to desired bitstring format
    BitstringVectorType carryover_bitstrings;
    carryover_bitstrings.reserve(carryover_sbd_bitstrings.size());
    for (const auto &sbd_bitstring : carryover_sbd_bitstrings) {
        carryover_bitstrings.emplace_back(
            sbd::makestring(
                sbd_bitstring, sbd_data.bit_length, alpha_determinants[0].size()
            )
        );
    }

    // Construct and return result object
    return {
        energy, std::move(density), std::move(carryover_bitstrings),
        std::move(one_p_rdm), std::move(two_p_rdm)
    };
}

} // namespace sqd

} // namespace addon

} // namespace Qiskit

#endif // __has_include(<sbd/sbd.h>)

#endif // QISKIT_ADDON_SQD_SUBSAMPLING_HPP_
