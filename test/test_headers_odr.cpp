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

// ODR guard for the header-only public API: this translation unit includes
// every public header, and the other test translation units already include
// them too.  If any public header (directly or transitively) defines a free
// function or variable with external linkage that is not inline, linking this
// TU alongside the others fails with a multiple-definition error.  It carries
// no test cases of its own; building and linking is the check.

#include "qiskit/addon/sqd/bitset_full.hpp"
#include "qiskit/addon/sqd/configuration_recovery.hpp"
#include "qiskit/addon/sqd/fermion.hpp"
#include "qiskit/addon/sqd/postselection.hpp"
#include "qiskit/addon/sqd/subsampling.hpp"
#include "qiskit/addon/sqd/support/boost_dynamic_bitset.hpp"
