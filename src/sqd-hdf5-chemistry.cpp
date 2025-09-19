// (C) Copyright IBM 2025.

// sqd-hdf5-chemistry: Example program which loads an HDF5 input file and
//                     performs SQD using the HPC-ready SQD addon/library

#include <mpi.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>

#include <boost/dynamic_bitset.hpp>

#include <xtensor/containers/xtensor.hpp>
#include <xtensor/io/xio.hpp>
#include <xtensor/views/xview.hpp>

#include <highfive/H5Easy.hpp>
#include <highfive/highfive.hpp>

#include "qiskit/addon/sqd/postselection.hpp"
#include "qiskit/addon/sqd/subsampling.hpp"

#include "sbd/sbd.h"

struct SQDInputParameters {
    unsigned int num_batches = 0;
};

struct MoleculeData {
};

struct SQDResult {
};

static SQDResult run_sqd(MPI_Comm comm, const SQDInputParameters &params)
{
    return {};
}

static void require_mpi_success(int return_code)
{
    QKA_SQD_IF_UNLIKELY_(return_code != MPI_SUCCESS)
    {
        std::cerr << "Unexpected MPI failure (" << return_code << "): " << std::flush;
        char err_msg[MPI_MAX_ERROR_STRING];
        int err_msg_len;
        MPI_Error_string(return_code, err_msg, &err_msg_len);
        std::cerr << err_msg << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

static void sqd_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    // Initialize MPI (should happen before command-line arguments are processed)
    int thread_support_provided;
    int mpi_init_status =
        MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &thread_support_provided);
    if (mpi_init_status != MPI_SUCCESS) {
        char err_msg[MPI_MAX_ERROR_STRING];
        int err_msg_len;
        MPI_Error_string(mpi_init_status, err_msg, &err_msg_len);
        std::cerr << "MPI_Init failed: " << err_msg << std::endl;
        return mpi_init_status;
    }

    try {
        sqd_main(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    } catch (...) {
        std::cerr << "Unknown exception caught." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Finalize MPI
    require_mpi_success(MPI_Finalize());
}

static void sqd_main(int argc, char *argv[])
{
    // Obtain MPI world rank and size
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::mt19937 rng(1234); // fixme: currently uses same constant seed on all workers

    SQDInputParameters params;

    // Inputs from the parameter file
    std::vector<boost::dynamic_bitset<>> bitstrings; // only exists on rank 0 (so far)
    std::vector<double> bitstring_probs;             // only exists on rank 0 (so far)
    std::uint32_t num_qubits;
    unsigned int num_elec_a, num_elec_b;

    // Load the parameter file on rank 0 worker
    if (world_rank == 0) {
        // Obtain name of input file from the command line
        if (argc < 2) {
            std::cerr << "ERROR: Filename must be provided as first argument"
                      << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        const char *input_filename = argv[1];

        // Open HDF5 input file
        HighFive::File file(input_filename, HighFive::File::ReadOnly);

        // Load molecule data from input file
        auto hcore = H5Easy::load<xt::xtensor<double, 2>>(file, "/molecule/hcore");
        auto eri = H5Easy::load<xt::xtensor<double, 4>>(file, "/molecule/eri");
        num_elec_a = H5Easy::load<decltype(num_elec_a)>(file, "/molecule/num_elec_a");
        num_elec_b = H5Easy::load<decltype(num_elec_b)>(file, "/molecule/num_elec_b");

        // TEMPORARY: Dump data to stdout
        std::cout << "hcore\n" << hcore << std::endl;
        std::cout << "eri\n" << eri << std::endl;

        // Load bitstrings
        auto bitstrings_matrix =
            H5Easy::load<xt::xtensor<std::uint8_t, 2>>(file, "/bitstrings");
        bitstring_probs = H5Easy::load<std::vector<double>>(file, "/probs");
        num_qubits = H5Easy::load<std::uint32_t>(file, "/bitstring_length");

        // NOTE: We are currently assuming that the input file contains no duplicate
        // bitstrings.

        std::cout << num_qubits << " qubits, " << num_elec_a << " alpha electrons, "
                  << num_elec_b << " beta electrons" << std::endl;

        if (bitstrings_matrix.shape(0) != bitstring_probs.size()) {
            throw std::runtime_error(
                "Numbers of probs does not match number of bitstrings"
            );
        }
        if (bitstrings_matrix.shape(1) != (num_qubits + 7) / 8) {
            throw std::runtime_error(
                "Bitstring length in bytes doesn't match number of qubits"
            );
        }

        // Populate vector of bitstrings
        std::cerr << "Experimental bitstrings:";
        std::string str;
        for (std::size_t i = 0; i < bitstring_probs.size(); ++i) {
            const auto bitstring_bytes = xt::view(bitstrings_matrix, i, xt::all());
            boost::dynamic_bitset<std::uint8_t> bitstring(
                bitstring_bytes.cbegin(), bitstring_bytes.cend()
            );
            // We must convert dynamic_bitset<std::uint8_t> to
            // dynamic_bitset<>.  One obvious way is to convert to a string.
            boost::to_string(bitstring, str);
            bitstrings.emplace_back(str);
            std::cerr << "\n"
                      << i << "\t"
                      << (bitstring_bytes.cend() - bitstring_bytes.cbegin()) << "\t"
                      << bitstrings[i] << std::endl;
        }
        std::cerr << std::endl;
    }

    // Broadcast parameters to all workers
    require_mpi_success(
        MPI_Bcast(&params, sizeof(SQDInputParameters), MPI_BYTE, 0, MPI_COMM_WORLD)
    );
    require_mpi_success(MPI_Bcast(
        &num_qubits, sizeof(decltype(num_qubits)), MPI_BYTE, 0, MPI_COMM_WORLD
    ));
    require_mpi_success(MPI_Bcast(
        &num_elec_a, sizeof(decltype(num_elec_a)), MPI_BYTE, 0, MPI_COMM_WORLD
    ));
    require_mpi_success(MPI_Bcast(
        &num_elec_b, sizeof(decltype(num_elec_b)), MPI_BYTE, 0, MPI_COMM_WORLD
    ));

    std::cout << num_qubits << " qubits, " << num_elec_a << " alpha electrons, "
              << num_elec_b << " beta electrons" << std::endl;

    // tmp
    std::size_t max_samples_per_batch = 5000;

    // Postselect and subsample on rank 0
    std::vector<boost::dynamic_bitset<>> batch; // only populated on rank 0 so far
    if (world_rank == 0) {
        auto [postselected_bitstrings, postselected_probs] =
            Qiskit::addon::sqd::postselect_bitstrings(
                bitstrings, bitstring_probs,
                Qiskit::addon::sqd::MatchesRightLeftHamming(num_elec_a, num_elec_b)
            );
        auto samples_per_batch =
            std::min(max_samples_per_batch, postselected_bitstrings.size());
        std::cout << "Drawing " << samples_per_batch
                  << " samples per batch from a population of "
                  << postselected_bitstrings.size() << std::endl;
        Qiskit::addon::sqd::subsample(
            batch, postselected_bitstrings, postselected_probs, samples_per_batch, rng
        );
    }

    // Now we need to call sbd
    std::vector<std::vector<std::size_t>> sbd_bitstrings;
    constexpr auto sbd_bit_length =
        sizeof(std::size_t) * 8; // default to maximum supported bit length
    {
        std::string str;
        for (const auto &bitstring : batch) {
            boost::to_string(bitstring, str);
            sbd_bitstrings.push_back(
                std::move(sbd::from_string(str, sbd_bit_length, num_qubits))
            );
        }
    }

    // Let's just dump all the bitstrings we are using for good measure
    std::cerr << "Selected bitstrings:";
    for (const auto &sbd_bitstring : sbd_bitstrings) {
        std::cerr << "\n" << sbd::makestring(sbd_bitstring, sbd_bit_length, num_qubits);
    }
    std::cerr << std::endl;

    // sbd eigensolver
    std::cout << "Beginning the diagonalization" << std::endl;
    for (int j = 0; j < argc; ++j) {
        std::cout << argv[j] << std::endl;
    }
#if 1
    auto sbd_data = sbd::tpb::generate_sbd_data(0, nullptr);
#else
    // auto sbd_data = sbd::tpb::generate_sbd_data(argc, argv);
    // sbd::tpb::SBD sbd_data;
#endif
    sbd_data.bit_length = sbd_bit_length;
    sbd::FCIDump fcidump;
    std::string loadname, savename("/tmp/wavefunction_data.bin");
    // results
    double energy;
    std::vector<double> density;
    std::vector<std::vector<size_t>> carryover_bitstrings;
    std::vector<std::vector<double>> one_p_rdm, two_p_rdm;
    sbd::tpb::diag(
        MPI_COMM_WORLD, sbd_data, fcidump, sbd_bitstrings, sbd_bitstrings, loadname,
        savename, energy, density, carryover_bitstrings, one_p_rdm, two_p_rdm
    );

    // Show some results
    std::cout << "Energy: " << energy << std::endl;
    std::cout << carryover_bitstrings.size() << " carryover bitstring(s):";
    for (const auto &sbd_bitstring : carryover_bitstrings) {
        std::cerr << "\n" << sbd::makestring(sbd_bitstring, sbd_bit_length, num_qubits);
    }
    std::cerr << std::endl;
}
