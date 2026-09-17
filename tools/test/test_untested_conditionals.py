#!/usr/bin/env python3
"""Behavioral tests for tools/untested-conditionals.py.

Each case is a minimal hand-written gcovr report exercising one filter, so the
distinction between a real conditional and a branch no test could cover stays
verified.  Run with `python3 tools/test/test_untested_conditionals.py`.
"""

import json
import pathlib
import subprocess
import sys
import tempfile

SCRIPT = pathlib.Path(__file__).resolve().parent.parent / "untested-conditionals.py"

def run(files, source_map, extra=()):
    with tempfile.TemporaryDirectory() as d:
        d = pathlib.Path(d)
        for name, text in source_map.items():
            p = d / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text)
        rep = d / "r.json"
        rep.write_text(json.dumps({"files": files}))
        r = subprocess.run([sys.executable, str(SCRIPT), str(rep), "--root", str(d), *extra],
                           capture_output=True, text=True)
        return r.stdout

def line(n, count, branches):
    return {"line_number": n, "count": count,
            "branches": [{"count": c} for c in branches]}

# throw-statement line: never executed, single branch -> excluded
f = [{"file": "include/a.hpp", "lines": [line(2, 0, [0])]}]
out = run(f, {"include/a.hpp": "x\nQKA_SQD_THROW_INVALID_ARGUMENT_(\"x\");\n"})
assert "a.hpp:2" not in out, out
print("PASS: throw statement excluded")

# emplace_back: executed, single branch (exception edge) -> excluded
f = [{"file": "include/a.hpp", "lines": [line(1, 5, [5])]}]
out = run(f, {"include/a.hpp": "v.emplace_back(x);\n"})
assert "a.hpp:1" not in out, out
print("PASS: single exception edge excluded")

# real if with untaken guard -> reported
f = [{"file": "include/a.hpp", "lines": [line(1, 9, [0, 9])]}]
out = run(f, {"include/a.hpp": "if (x != y) {\n"})
assert "a.hpp:1" in out, out
print("PASS: untaken guard reported")

# fully covered if -> not reported
f = [{"file": "include/a.hpp", "lines": [line(1, 9, [4, 5])]}]
out = run(f, {"include/a.hpp": "if (x != y) {\n"})
assert "a.hpp:1" not in out, out
print("PASS: covered conditional not reported")

# differing widths: one instantiation covers both ways -> not reported
f = [{"file": "include/a.hpp", "lines": [line(1, 14, [14, 14, 0]), line(1, 240, [195, 45])]}]
out = run(f, {"include/a.hpp": "if (a == b) {\n"})
assert "a.hpp:1" not in out, out
print("PASS: width groups merged independently")

# assert -> excluded
f = [{"file": "include/a.hpp", "lines": [line(1, 9, [0, 9])]}]
out = run(f, {"include/a.hpp": "assert(n <= size);\n"})
assert "a.hpp:1" not in out, out
print("PASS: assert excluded")

# summing across same-width instantiations
f = [{"file": "include/a.hpp", "lines": [line(1, 5, [0, 5]), line(1, 5, [3, 2])]}]
out = run(f, {"include/a.hpp": "if (x) {\n"})
assert "a.hpp:1" not in out, out
print("PASS: same-width instantiations summed")

# statement carrying inlined branches -> excluded
f = [{"file": "include/a.hpp", "lines": [line(1, 9, [9, 0, 0])]}]
out = run(f, {"include/a.hpp": "++counts[left_ci];\n"})
assert "a.hpp:1" not in out, out
print("PASS: inlined-callee branches excluded")

# prefix filter
f = [{"file": "test/t.cpp", "lines": [line(1, 9, [0, 9])]}]
out = run(f, {"test/t.cpp": "if (x) {\n"})
assert "t.cpp" not in out, out
print("PASS: prefix filter excludes test/")

print()
print("all filter cases pass")
