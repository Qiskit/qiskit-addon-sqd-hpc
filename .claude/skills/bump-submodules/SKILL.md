---
name: bump-submodules
description: Check the vendored git submodules under deps/ for newer upstream releases and bump them deliberately, one concern at a time, to release-tagged commits only. Use when asked to update, check, or bump the submodules / vendored dependencies (doctest, nanobench, Boost, bitset2).
---

# Bumping the vendored submodules

This project vendors its C++ dependencies as git submodules under `deps/`.
Dependabot does **not** track them (its `gitsubmodule` updater proposed
downgrades and moves to untagged commits — see
dependabot/dependabot-core#14513), so they are bumped by hand with this skill.

## Principles

- **Release tags only.** Move a submodule pin only to a commit that carries an
  upstream **release tag** — never to an arbitrary branch commit. This is the
  single rule the automated updater kept violating.
- **Only move forward.** Confirm the new tag is newer than the current pin.
  Never propose a downgrade.
- **Boost moves in lockstep.** All `deps/boost/*` libraries must sit on the
  **same** `boost-X.YY.0` release. Bump them together, never a subset.
- **Verify by building**, not by reasoning. A submodule bump can silently
  change or relocate headers; the build is the proof.
- **One concern per PR.** e.g. one PR for nanobench, one for the Boost set.
  Follow the usual workflow: a worktree, a brief branch name, a **draft** PR.

## Steps

### 1. Survey what is behind

For each submodule, resolve its current pin to a tag and compare against the
latest upstream **release** tag (skip betas/rcs). Use absolute paths with
`git -C <abs-path>` (the shell cwd can reset); do not `cd` into submodules.

```
git -C deps/<sub> fetch --tags origin
git -C deps/<sub> describe --tags HEAD           # current
git -C deps/<sub> tag --sort=-v:refname | head   # latest tags (boost-*: grep '^boost-')
```

Confirm each candidate bump is a clean fast-forward:

```
git -C deps/<sub> merge-base --is-ancestor <current-commit> <new-tag>^{commit} && echo FF
```

Notes on individual submodules:
- **doctest**, **nanobench** — `vX.Y.Z` tags.
- **deps/boost/\*** — `boost-X.YY.0` tags; released together, bump as a set.
- **bitset2** — publishes **no release tags** and tracks `master`. There is
  nothing to bump it *to*; leave it, and just note its state.

### 2. Bump the pins

Per submodule, on the bump branch (checkout the tag by name so the intent is
recorded):

```
git -C deps/<sub> fetch --tags origin
git -C deps/<sub> checkout <release-tag>
```

`git describe` may show a nearer older tag when the library is unchanged across
releases; confirm with `git -C deps/<sub> rev-parse HEAD` against
`<new-tag>^{commit}` rather than trusting `describe`.

### 3. Watch for relocated / removed headers

Upstream sometimes **merges one library into another** and deletes its headers
(e.g. Boost.StaticAssert merged into Boost.Config at 1.92.0, so
`deps/boost/static_assert` became a stub and `boost/static_assert.hpp` now
ships from `deps/boost/config`). When that happens:

- Check whether anything still includes the moved header:
  `grep -rl "boost/<name>.hpp" deps/boost/*/include include test benchmark`
- Confirm the new provider (already vendored) supplies it.
- If a submodule is now a vestigial stub that nothing needs, **remove it**:
  `git rm deps/boost/<name>`, drop its `${...}/deps/boost/<name>/include` line
  from `CMakeLists.txt`, and let `git rm` update `.gitmodules`.

### 4. Verify — build and test in every CI configuration

Adapt from `dev-loop.sh`. All three must pass:

```
# C++17 (default)
rm -rf build && cmake -S . -B build -G Ninja && cmake --build build -j8 && ./build/sqd_tests

# C++20
rm -rf build && cmake -S . -B build -G Ninja -DCMAKE_CXX_STANDARD=20 -DCMAKE_CXX_FLAGS="" \
  && cmake --build build -j8 && ./build/sqd_tests

# no exceptions / no RTTI
rm -rf build && cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_FLAGS="-fno-rtti -fno-exceptions -DQKA_SQD_DISABLE_EXCEPTIONS=1 -DDOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS=1" \
  && cmake --build build -j8 --target sqd_tests && ./build/sqd_tests
```

If nanobench changed, also build and run `sqd_benchmarks`.

### 5. Open the PR

Commit (submodule pointer changes plus any CMake/`.gitmodules` edits), push,
and open a **draft** PR. State the old and new versions, note any removed or
relocated header, and confirm which build configurations were verified.
