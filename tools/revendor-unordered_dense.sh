#!/bin/bash
#
# Re-vendor ankerl::unordered_dense into
# include/qiskit/addon/sqd/internal/vendor/unordered_dense/.
#
# This library is vendored (rather than required as an external dependency) so
# that the header-only qiskit-addon-sqd library depends only on the C++ standard
# library, while still giving every consumer the faster hash map by default.
#
# To avoid namespace and include-guard collisions with a downstream project that
# independently uses its own copy of ankerl::unordered_dense, this script renames
# the library's top-level namespace and include guards into project-specific
# tokens.  The RENAMED headers are what is committed to git; running this script
# is only necessary to pull in a new upstream version.
#
# Usage (from anywhere):
#   tools/revendor-unordered_dense.sh [VERSION]
# e.g.
#   tools/revendor-unordered_dense.sh v5.0.1
#
# The upstream project is MIT-licensed; the license headers in the fetched files
# are preserved.  See the note in the repository README about third-party code
# under Apache-compatible licenses in the vendor directory.

set -euo pipefail

VERSION="${1:-v5.0.1}"
BASE_URL="https://raw.githubusercontent.com/martinus/unordered_dense/${VERSION}/include/ankerl"

# This script lives in tools/; the vendored headers live under include/.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VENDOR_DIR="${REPO_ROOT}/include/qiskit/addon/sqd/internal/vendor/unordered_dense"
cd "${VENDOR_DIR}"

# --- Fetch pristine upstream headers ----------------------------------------
for f in unordered_dense.h stl.h; do
    echo "Fetching ${f} @ ${VERSION}"
    curl -fsSL "${BASE_URL}/${f}" -o "${f}"
done

# --- Apply renames -----------------------------------------------------------
# 1. Top-level namespace: ankerl::unordered_dense -> project-internal namespace.
#    This rewrites the namespace open/close, the out-of-namespace erase_if()
#    qualified references, and (harmlessly) the same string in error messages.
#    The versioned inline sub-namespace and all ANKERL_* feature macros are left
#    intact, so the library's own machinery is untouched.
#
# 2. Include guards: rename ONLY the exact file-guard tokens, anchored with word
#    boundaries so we do not corrupt sibling macros that share the prefix
#    (ANKERL_UNORDERED_DENSE_HAS_SSE, ..._HAS_NEON, ..._HASH_STATICCAST, etc.).

NEW_NS='Qiskit::addon::sqd::internal::vendored::unordered_dense'

sed -i \
    -e "s/ankerl::unordered_dense/${NEW_NS}/g" \
    -e 's/\bANKERL_UNORDERED_DENSE_H\b/QKA_SQD_VENDORED_UNORDERED_DENSE_H/g' \
    unordered_dense.h

sed -i \
    -e 's/\bANKERL_STL_H\b/QKA_SQD_VENDORED_UNORDERED_DENSE_STL_H/g' \
    stl.h

echo "Re-vendored ankerl::unordered_dense ${VERSION} as ${NEW_NS}"
echo "Review the diff, then rebuild and run the test suite."
