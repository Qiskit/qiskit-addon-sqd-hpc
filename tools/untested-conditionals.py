#!/usr/bin/env python3
"""Report conditionals in the library headers that tests never exercise both ways.

Reads a gcovr JSON report and lists source lines where a branch was executed
but one of its outcomes was never taken -- an `if` whose guard never fired, a
loop whose body was never entered, and so on.  These are candidates for
missing tests.

Branch coverage in a header-only template library needs filtering before it
says anything useful, because most branches gcov reports cannot be covered by
any test:

  * Exception landing pads.  GCC emits an edge to the unwinder for anything
    that may throw, so `v.emplace_back(x)` and `return {a, b};` carry a branch
    despite containing no conditional.
  * Statements that never run at all, such as the body of an untested `throw`.
    These are a consequence of the untaken guard above them, not a separate
    finding.
  * Template instantiations.  gcov reports each source line once per
    instantiation, so one line yields several entries with different counts.
  * Branches from inlined library code, which gcov attributes to the calling
    line.  A line holding no conditional of its own -- `++counts[key];`, or a
    bare `}` -- can carry branches from a rehash or a destructor.
  * `assert()`, whose failure edge is reachable only by aborting the process.

Each filter below corresponds to one of those cases.  CONTRIBUTING.md has
the background.

Note that this reports what is untested, which is not the same as what is
wrong.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

# A conditional worth reporting starts with one of these, or contains a
# ternary.  Requiring this drops lines whose only branches come from inlined
# library code attributed here by gcov, which is otherwise hard to distinguish
# from a real conditional.
CONDITIONAL = re.compile(
    r"""^(?: (?:\}\s*)? (?: if | else\s+if | for | while | switch | do )\b
          | .*\bQKA_SQD_IF_[A-Z]+_\s*\(
          | .*\?.*:
       )""",
    re.VERBOSE,
)

# assert() compiles to a branch whose failure edge can only be taken by
# aborting, so it can never be covered.  NDEBUG is off in a coverage build.
ASSERT = re.compile(r"^\s*assert\s*\(")


def load(report: pathlib.Path) -> dict:
    with report.open() as f:
        return json.load(f)


def merge_instantiations(lines: list[dict]) -> dict[int, list[list[int]]]:
    """Sum branch counts per line across template instantiations.

    gcov reports a line once per instantiation, and a branch outcome only counts
    as never taken if it was never taken in *any* instantiation, so counts are
    summed rather than reduced to a single representative instantiation.

    Instantiations may disagree on how many branches a line has, because
    inlining differs between them.  Branch indices are not comparable across
    differing widths, so entries are grouped by width and each group is summed
    independently; merging them positionally would leave a slot that exists in
    only one group looking permanently untaken.
    """
    merged: dict[int, dict[int, list[int]]] = {}
    for line in lines:
        branches = line.get("branches") or []
        if not branches:
            continue
        counts = [b["count"] for b in branches]
        by_width = merged.setdefault(line["line_number"], {})
        existing = by_width.get(len(counts))
        if existing is None:
            by_width[len(counts)] = counts
        else:
            for i, c in enumerate(counts):
                existing[i] += c
    return {number: list(groups.values()) for number, groups in merged.items()}


def findings(data: dict, root: pathlib.Path, prefix: str) -> list[tuple[str, int, int, int, str]]:
    out = []
    for entry in data["files"]:
        name = entry["file"]
        if not name.startswith(prefix):
            continue
        source_path = root / name
        try:
            source = source_path.read_text().splitlines()
        except OSError:
            source = []

        for number, groups in sorted(merge_instantiations(entry["lines"]).items()):
            # A single branch on a line is an exception edge, not a
            # conditional: a real conditional has at least two outcomes.
            groups = [g for g in groups if len(g) >= 2]
            # A line that never executed is the body of some other untaken
            # branch.  Reporting it duplicates the guard that caused it.
            groups = [g for g in groups if any(g)]
            if not groups:
                continue
            # Some inlined copy of this line covered every outcome, so the
            # conditional is exercised both ways somewhere.
            if any(all(g) for g in groups):
                continue
            untaken = max(sum(1 for c in g if c == 0) for g in groups)
            total = max(len(g) for g in groups)
            text = source[number - 1].strip() if number <= len(source) else ""
            if ASSERT.match(text) or not CONDITIONAL.match(text):
                continue
            out.append((name, number, untaken, total, text))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=pathlib.Path, help="gcovr JSON report")
    parser.add_argument(
        "--root",
        type=pathlib.Path,
        default=pathlib.Path.cwd(),
        help="directory the report's paths are relative to (default: cwd)",
    )
    parser.add_argument(
        "--prefix",
        default="include/",
        help="only report files under this path prefix (default: include/)",
    )
    parser.add_argument(
        "--markdown", action="store_true", help="emit GitHub-flavored markdown"
    )
    args = parser.parse_args()

    found = findings(load(args.report), args.root, args.prefix)

    if args.markdown:
        print("### Untested conditionals")
        print()
        if not found:
            print(f"None: every conditional under `{args.prefix}` is exercised both ways.")
            return 0
        print(
            f"{len(found)} conditional(s) under `{args.prefix}` were executed but never "
            "took every outcome.  Each is a candidate for a missing test."
        )
        print()
        print("| location | untaken | source |")
        print("| --- | --- | --- |")
        for name, number, untaken, total, text in found:
            cell = text.replace("|", "\\|")
            print(f"| `{name}:{number}` | {untaken}/{total} | `{cell}` |")
    else:
        if not found:
            print(f"No untested conditionals under {args.prefix}")
            return 0
        for name, number, untaken, total, text in found:
            print(f"{name}:{number}: {untaken}/{total} branches untaken")
            if text:
                print(f"    {text}")
        print()
        print(f"{len(found)} conditional(s) not exercised both ways")
    return 0


if __name__ == "__main__":
    sys.exit(main())
