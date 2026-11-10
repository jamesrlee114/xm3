#!/usr/bin/env python3
"""Count code lines (not blank, not comment-only) and check line budgets.

Usage: tools/loc.py [--budget NAME=LINES ...] DIR ...

Each DIR is a component, named by its last path element. Exit status 1 if
a component is over its budget.
"""

import argparse
import os
import re
import sys

SOURCE = re.compile(r"\.(c|h|S|ld|py)$")


def code_lines(path):
    """Lines with code on them, skipping /* */ and // comments."""
    count, in_block = 0, False
    with open(path, encoding="utf-8") as f:
        for line in f:
            code, i = "", 0
            while i < len(line):
                if in_block:
                    end = line.find("*/", i)
                    if end < 0:
                        break
                    in_block, i = False, end + 2
                elif line.startswith("/*", i):
                    in_block, i = True, i + 2
                elif line.startswith("//", i):
                    break
                else:
                    code += line[i]
                    i += 1
            if code.strip():
                count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--budget", action="append", default=[], metavar="NAME=LINES")
    parser.add_argument("dirs", nargs="+")
    args = parser.parse_args()
    budgets = {k: int(v) for k, v in (b.split("=", 1) for b in args.budget)}

    over = False
    for d in args.dirs:
        name = os.path.basename(os.path.normpath(d))
        files = sorted(os.path.join(root, f) for root, _, fs in os.walk(d)
                       for f in fs if SOURCE.search(f))
        total = sum(code_lines(f) for f in files)
        budget = budgets.get(name)
        note = f" of {budget} budget" if budget else ""
        print(f"{name}: {total} lines in {len(files)} files{note}")
        if budget is not None and total > budget:
            print(f"  over budget by {total - budget} lines", file=sys.stderr)
            over = True
    return 1 if over else 0


if __name__ == "__main__":
    sys.exit(main())
