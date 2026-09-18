# Third-party notices

This project is licensed under the Apache License, Version 2.0 (see
[`LICENSE.txt`](LICENSE.txt)).  It also bundles third-party source code under
its own, Apache-compatible license, listed below.  Each bundled file also
retains its upstream license header.

## ankerl::unordered_dense

- **Version:** 5.0.1
- **Upstream:** <https://github.com/martinus/unordered_dense>
- **License:** MIT
- **Bundled at:** `include/qiskit/addon/sqd/internal/vendor/unordered_dense/`
- **Provenance:** vendored (rather than depended on externally) so that the
  header-only library depends only on the C++ standard library while still
  providing a fast hash map by default.  The upstream sources are unmodified
  except that the top-level namespace and include guards are renamed into
  project-specific tokens, to avoid collisions with a downstream project that
  independently uses its own copy of the library.  That rename is applied by
  [`tools/revendor-unordered_dense.sh`](tools/revendor-unordered_dense.sh),
  which also documents how to pull in a new upstream version.

```
MIT License

Copyright (c) 2022 Martin Leitner-Ankerl <martin.ankerl@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
