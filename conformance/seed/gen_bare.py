#!/usr/bin/env python3
"""Seeds the bare-root reader cases (BR-2610-anvl-004): inputs directly under conformance/cases/.

The corpus runs every input from conformance/cases/ by its relative path, so an input at the top of cases/ has a
root document named with no directory part (a bare file name). That is the case BR-2610-anvl-004 was about: a bare
root and the "./name" its includer composes are one document, so an include cycle through a bare root is found.
Usage: gen_bare.py <repo-root>   (writes the inputs; the dumps are native's, from `make update-conformance`)
"""
import os, sys

out = os.path.join(sys.argv[1], "conformance", "cases")
files = {
    "bare_self.anvl": 'include "bare_self.anvl";\nx := 1;\n',
    "bare_self_dot.anvl": 'include "./bare_self_dot.anvl";\nx := 1;\n',
    "bare_mutual_a.anvl": 'include "bare_mutual_b.anvl";\na := 1;\n',
    "bare_mutual_b.anvl": 'include "bare_mutual_a.anvl";\nb := 1;\n',
    "bare_mutual_dot_a.anvl": 'include "./bare_mutual_dot_b.anvl";\na := 1;\n',
    "bare_mutual_dot_b.anvl": 'include "./bare_mutual_dot_a.anvl";\nb := 1;\n',
    "bare_three_a.anvl": 'include "bare_three_b.anvl";\na := 1;\n',
    "bare_three_b.anvl": 'include "bare_three_c.anvl";\nb := 1;\n',
    "bare_three_c.anvl": 'include "bare_three_a.anvl";\nc := 1;\n',
    "bare_up.anvl": 'include "coverage/../bare_up.anvl";\nx := 1;\n',
    "bare_diamond_root.anvl": 'include "bare_diamond_b.anvl";\ninclude "bare_diamond_c.anvl";\nr := 1;\n',
    "bare_diamond_b.anvl": 'include "bare_diamond_d.anvl";\nb := 1;\n',
    "bare_diamond_c.anvl": 'include "bare_diamond_d.anvl";\nc := 1;\n',
    "bare_diamond_d.anvl": 'd := 1;\n',
}
for name, text in files.items():
    with open(os.path.join(out, name), "wb") as f:
        f.write(text.encode("ascii"))
