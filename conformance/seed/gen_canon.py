#!/usr/bin/env python3
"""Seeds the canonical-identity reader cases (BR-2611-anvl-include-001): inputs under conformance/cases/canon/.

An include whose file is the file of a document still being loaded (the root included) is a cycle, by whatever path
or symbolic link it was reached. A document that reaches itself by any spelling of its own path is an include cycle
(4206), at the
position of the include keyword, exactly as the plain spelling reports. The corpus runs every input from
conformance/cases by its relative path (a root `canon/x.anvl`, or `x.anvl` at the top), so the spellings here are
the ones a relative corpus can hold: bare, ./, ../ (up and back down by the directory's name), a/../ and doubled
slashes, for a root and for a child. An absolute spelling depends on where the corpus is checked out, and a symbolic
link is not a regular file, so those are covered by native's unit tests (ANV86-ANV91), not by corpus files.
Usage: gen_canon.py <repo-root>   (writes the inputs; the dumps are native's, from `make update-conformance`)
"""
import os, sys

out = os.path.join(sys.argv[1], "conformance", "cases")
files = {
    # the root reaches itself (cwd is conformance/cases; the root is named `canon_root_*.anvl`, a bare name)
    "canon_root_up_down.anvl": 'include "../cases/canon_root_up_down.anvl";\nx := 1;\n',
    "canon_root_via_dir.anvl": 'include "canon/../canon_root_via_dir.anvl";\nx := 1;\n',
    "canon_root_slashes.anvl": 'include ".//canon_root_slashes.anvl";\nx := 1;\n',
    "canon_root_line3.anvl": '#!aml\n\ninclude "./../cases/canon_root_line3.anvl";\nx := 1;\n',
    # the root is below a directory
    "canon/root_self.anvl": 'include "root_self.anvl";\nx := 1;\n',
    "canon/root_dot.anvl": 'include "./root_dot.anvl";\nx := 1;\n',
    "canon/root_up_down.anvl": 'include "../canon/root_up_down.anvl";\nx := 1;\n',
    "canon/root_out_and_in.anvl": 'include "../canon/../canon/root_out_and_in.anvl";\nx := 1;\n',
    # a child reaches itself, or the root reaches it by a different spelling than the child's own
    "canon_child_root.anvl": 'include "canon/child_self.anvl";\nr := 1;\n',
    "canon/child_self.anvl": 'include "child_self.anvl";\nc := 1;\n',
    "canon_child_dot_root.anvl": 'include "./canon/child_dot.anvl";\nr := 1;\n',
    "canon/child_dot.anvl": 'include "./child_dot.anvl";\nc := 1;\n',
    "canon_child_up_root.anvl": 'include "canon/child_up.anvl";\nr := 1;\n',
    "canon/child_up.anvl": 'include "../canon/child_up.anvl";\nc := 1;\n',
    # the child includes the root by another spelling than the root's own name
    "canon_back_root.anvl": 'include "canon/back.anvl";\nr := 1;\n',
    "canon/back.anvl": 'include "../canon_back_root.anvl";\nb := 1;\n',
    "canon_back_up_root.anvl": 'include "canon/back_up.anvl";\nr := 1;\n',
    "canon/back_up.anvl": 'include "../canon/../canon_back_up_root.anvl";\nb := 1;\n',
    # no false cycle: a diamond whose branches spell the shared document differently
    "canon_diamond_root.anvl": 'include "canon/dia_b.anvl";\ninclude "./canon/../canon/dia_c.anvl";\nr := 1;\n',
    "canon/dia_b.anvl": 'include "dia_d.anvl";\nb := 1;\n',
    "canon/dia_c.anvl": 'include "../canon/./dia_d.anvl";\nc := 1;\n',
    "canon/dia_d.anvl": 'd := 1;\n',
}
os.makedirs(os.path.join(out, "canon"), exist_ok=True)
for name, text in files.items():
    with open(os.path.join(out, name), "wb") as f:
        f.write(text.encode("ascii"))
