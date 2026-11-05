#!/usr/bin/env python3
"""Seeds conformance/cases/refcycle/ (BR-2611-anvl-resolver-002): a `$` reference that leads back to a container
holding it is the resolver error 4054 (ANVL_ERR_RESOLVER_REFERENCE_CYCLE), at the `$` of the first reference on a cycle.

The rule the cases pin (the dumps are native's, from `make update-conformance`):
  A walk follows (1) a container's own members, (2) fields inherited from a base (aliased, after the merge) and
  (3) a resolved `$` reference to its target statement's value. If any value can reach a value it is inside, the load
  fails. The position is the `$` of the first reference that lies on a cycle, in source order: the documents in load order
  (the dump's `doc` numbers), then by byte offset inside a document.
  A reference to a scalar never loops. A plain chain cycle (`a := $b; b := $a;`) stays unresolved and valid.

  err/       the shapes that must fail, one statement of the position rule each
  inc/       the same across included documents (the root includes the others)
  ok/        what stays valid: scalar targets, no way back, chain cycles, overrides, diamonds, deep acyclic fans
  scale/     rings and fans of a few hundred statements

Deterministic; the output is committed and not regenerated at check time (like the other seed scripts).
Usage: gen_refcycle.py <repo-root> [--force]     Refuses to run over an existing cases/refcycle unless --force.
"""
import os, shutil, sys

args = [a for a in sys.argv[1:] if a != "--force"]
root = args[0]
out = os.path.join(root, "conformance", "cases", "refcycle")
if os.path.exists(out):
    if "--force" not in sys.argv:
        sys.exit("cases/refcycle exists; pass --force to wipe and regenerate")
    shutil.rmtree(out)

def put(rel, data):
    path = os.path.join(out, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if isinstance(data, str):
        data = data.encode("utf-8")
    with open(path, "wb") as f:
        f.write(data)

# ---------------------------------------------------------------------------------------------------- err/
E = "err/"
# a container that holds a reference to itself
put(E + "self_object.anvl", "s1 := { a := $s1; };\n")
put(E + "self_object_deeper.anvl", "s1 := { a := { b := $s1; }; };\n")
put(E + "self_array.anvl", "a := [ $a ];\n")
put(E + "self_tuple.anvl", "a := ( 1, $a );\n")
put(E + "self_block_form.anvl", "a { x := $a; };\n")
put(E + "self_many_levels.anvl", "a := [ [ { x := ( 1, [ $a ] ); } ] ];\n")
put(E + "self_third_line.anvl", "s1 := {\n   a := 1;\n   b := $s1;\n};\n")
put(E + "self_explicit_shebang.anvl", "#!aml\ns1 := { a := $s1; };\n")
put(E + "self_comments.anvl", "// a comment\ns1 := { /* inline */ a := $s1; };\n")
put(E + "self_crlf.anvl", "s1 := {\r\n   a := 1;\r\n   b := $s1;\r\n};\r\n")
put(E + "self_tab.anvl", "s1 := {\n\ta := $s1;\n};\n")
put(E + "self_multibyte_before.anvl", 's1 := { a := "é€\U0001F600"; b := $s1; };\n')
put(E + "self_after_other_statements.anvl", "x := 1;\ny := [ 1, 2 ];\ns1 := { a := $s1; };\n")
put(E + "self_attributes.anvl", "s1 @[k=1] := { a := $s1; };\n")
put(E + "self_two_references_first_reported.anvl", "s1 := { a := $s1; b := $s1; };\n")
# statements that refer to each other
put(E + "mutual_objects.anvl", "a := { x := $b; };\nb := { y := $a; };\n")
put(E + "mutual_objects_reversed.anvl", "b := { y := $a; };\na := { x := $b; };\n")
put(E + "mutual_one_line.anvl", "a := { x := $b; }; b := { y := $a; };\n")
put(E + "mutual_ring_three.anvl", "c := { x := $a; };\na := { y := $b; };\nb := { z := $c; };\n")
put(E + "mutual_block_form.anvl", "a { x := $b; };\nb { y := $a; };\n")
put(E + "mutual_arrays.anvl", "a := [ $b ];\nb := [ $a ];\n")
put(E + "mutual_through_chain.anvl", "a := $b;\nb := { c := $a; };\n")
put(E + "mutual_through_longer_chain.anvl", "a := $b;\nb := $c;\nc := [ $a ];\n")
put(E + "mutual_chain_target_not_container.anvl", "a := $b;\nb := $c;\nc := { x := $a; };\n")
put(E + "mutual_scalar_beside.anvl", "r := { p := $a; q := $b; };\na := { x := $r; };\nb := 1;\n")
put(E + "mutual_reference_to_the_chain_head.anvl", "h := $t;\nt := { x := $h; y := 2; };\n")
# closed through inheritance
put(E + "inherit_field_refers_to_deriving.anvl", "a := { x := $b; };\nb : a := { z := 1; };\n")
put(E + "inherit_two_statements.anvl", "a := { x := $c; };\nb : a := { y := 1; };\nc := { z := $b; };\n")
put(E + "inherit_base_refers_to_leaf.anvl", "base := { r := $leaf; };\nleaf : base := { q := 1; };\n")
put(E + "inherit_three_levels.anvl", "a := { x := $d; };\nb : a := { m := 1; };\nc : b := { n := 2; };\nd : c := { k := 3; };\n")
put(E + "inherit_own_reference_no_alias.anvl", "a := { x := 1; };\nb : a := { y := $c; };\nc := { z := $b; };\n")
put(E + "inherit_block_form_deriving.anvl", "a := { x := $b; };\nb : a { z := 1; };\n")
put(E + "inherit_nested_reference.anvl", "a := { x := { y := [ $b ]; }; };\nb : a := { z := 1; };\n")
put(E + "inherit_reference_through_array_of_objects.anvl", "a := { x := [ { y := $b; }, { y := 2; } ]; };\nb : a := { z := 1; };\n")
put(E + "inherit_override_then_another_cycle_reported.anvl", "a := { x := $c; w := 1; };\nb : a := { x := 5; };\nc : b := { w := 2; };\nd : a := { v := $d; };\n")
put(E + "inherit_cycle_in_aliased_field_of_two_derived.anvl", "base := { r := $one; };\none : base := { p := 1; };\ntwo : base := { q := 2; };\n")
# which reference is reported
put(E + "pos_second_statement.anvl", "ok := 1;\nb := [ $b ];\na := [ $a ];\n")
put(E + "pos_first_in_source_not_where_a_walk_starts.anvl", "c := { x := $a; };\na := { y := $b; };\nb := { z := $c; };\n")
put(E + "pos_earlier_reference_not_on_cycle.anvl", "u := $r;\nr := [ $r ];\n")
put(E + "pos_two_separate_cycles.anvl", "a := [ $a ];\nb := [ $b ];\n")
put(E + "pos_later_cycle_after_valid_references.anvl", "n := 1;\nt := { v := $n; };\nw := { z := $t; };\nr := [ $r ];\n")
put(E + "pos_reference_before_cycle_to_same_target.anvl", "r := [ $r ];\nu := { x := $r; };\n")
put(E + "pos_reference_after_cycle_to_same_target.anvl", "u := { x := $r; };\nr := [ $r ];\n")
put(E + "pos_second_reference_in_same_statement_is_on_cycle.anvl", "t := { v := 1; };\na := { x := $t; y := $a; };\n")
put(E + "pos_inherited_reference_precedes_in_source.anvl", "a := { x := $c; };\nb : a := { y := 1; };\nc := { z := $b; };\nd := [ $d ];\n")
put(E + "pos_chain_cycle_reference_ignored.anvl", "p := $q;\nq := $p;\nr := [ $r ];\n")

# ---------------------------------------------------------------------------------------------------- inc/
I = "inc/"
put(I + "pair_root.anvl", 'include "pair_child.anvl";\n\nroot := { x := $child; };\n')
put(I + "pair_child.anvl", "child := { y := $root; };\n")
put(I + "child_self_root.anvl", 'include "child_self.anvl";\nuse := 1;\n')
put(I + "child_self.anvl", "a := [ $a ];\n")
put(I + "skip_root.anvl", 'include "skip_child.anvl";\nu := $r;\n')
put(I + "skip_child.anvl", "r := [ $r ];\n")
put(I + "base_root.anvl", 'include "base_child.anvl";\nleaf : base := { q := 1; };\n')
put(I + "base_child.anvl", "base := { r := $leaf; };\n")
put(I + "base_in_root_leaf_in_child_root.anvl", 'include "leaf_child.anvl";\nbase := { r := $leaf; };\n')
put(I + "leaf_child.anvl", "leaf : base := { q := 1; };\n")
put(I + "ok_root.anvl", 'include "ok_child.anvl";\nuse := { x := $t; };\n')
put(I + "ok_child.anvl", "t := { v := 1; };\n")
put(I + "order_b_then_c_root.anvl", 'include "order_b.anvl";\ninclude "order_c.anvl";\nr := 1;\n')
put(I + "order_c_then_b_root.anvl", 'include "order_c.anvl";\ninclude "order_b.anvl";\nr := 1;\n')
put(I + "order_b.anvl", "b := { x := $c; };\n")
put(I + "order_c.anvl", "c := { y := $b; };\n")
put(I + "diamond_root.anvl", 'include "diamond_l.anvl";\ninclude "diamond_r.anvl";\nr := 1;\n')
put(I + "diamond_l.anvl", 'include "diamond_d.anvl";\nl := { x := $d; };\n')
put(I + "diamond_r.anvl", 'include "diamond_d.anvl";\nr2 := { x := $d; };\n')
put(I + "diamond_d.anvl", "d := [ $d ];\n")
put(I + "three_docs_root.anvl", 'include "three_a.anvl";\ninclude "three_b.anvl";\nroot := { x := $a; };\n')
put(I + "three_a.anvl", "a := { y := $b; };\n")
put(I + "three_b.anvl", "b := { z := $root; };\n")
put(I + "nested_include_root.anvl", 'include "nested_mid.anvl";\nr := { x := $tail; };\n')
put(I + "nested_mid.anvl", 'include "nested_tail.anvl";\nm := 1;\n')
put(I + "nested_tail.anvl", "tail := { y := $r; };\n")
put(I + "ok_two_way_scalars_root.anvl", 'include "ok_two_way_scalars_child.anvl";\nn := 1;\nroot := { x := $c; y := $n; };\n')
put(I + "ok_two_way_scalars_child.anvl", "c := { v := 2; };\n")

# ---------------------------------------------------------------------------------------------------- ok/
O = "ok/"
valid = [
    ("chain_cycle", "a := $b;\nb := $a;\n"),
    ("chain_self", "a := $a;\n"),
    ("chain_cycle_then_reference", "a := $b;\nb := $a;\nc := [ $a ];\n"),
    ("chain_three_cycle", "a := $b;\nb := $c;\nc := $a;\n"),
    ("chain_into_chain_cycle", "x := { y := $a; };\na := $b;\nb := $a;\n"),
    ("scalar_targets", "n := 1;\na := { x := $n; y := [ $n, $n ]; };\n"),
    ("string_target", 's := "text";\na := [ $s, $s ];\n'),
    ("blob_target", "b := @t`data`;\na := { x := $b; };\n"),
    ("null_target", "n := null;\na := { x := $n; };\n"),
    ("no_way_back", "a := { x := $b; };\nb := { y := 1; };\n"),
    ("no_way_back_chain", "a := { x := $b; };\nb := $c;\nc := { y := 1; };\n"),
    ("no_way_back_block_form", "a { x := 1; };\nb := { y := $a; };\n"),
    ("several_users_of_one_target", "a := { x := $b; y := $c; };\nb := { z := $c; };\nc := { w := 1; };\n"),
    ("array_of_references", "a := [ $b, $c ];\nb := { p := 1; };\nc := ( 1, $b );\n"),
    ("missing_target", "a := [ $nothing ];\n"),
    ("override_breaks_the_cycle", "a := { x := $b; };\nb : a := { x := 1; };\n"),
    ("override_two_levels", "a := { x := $c; };\nb : a := { x := 1; };\nc : b := { w := 2; };\n"),
    ("override_in_block_form", "a := { x := $b; };\nb : a { x := 1; };\n"),
    ("diamond_of_aliased_fields", "base := { v := [ 1, 2 ]; };\na : base := { p := 1; };\nb : base := { q := 2; };\nc := { x := $a; y := $b; };\n"),
    ("inherited_reference_to_unrelated", "t := { v := 1; };\na := { x := $t; };\nb : a := { y := 2; };\nc : b := { z := $b; };\n"),
    ("derived_refers_to_its_base", "a := { x := 1; };\nb : a := { y := $a; };\n"),
    ("base_refers_to_unrelated_object", "t := { v := 1; };\nbase := { r := $t; };\nleaf : base := { q := 1; };\n"),
    ("self_name_in_a_string", 's1 := { a := "$s1"; };\n'),
    ("self_name_as_bare_word", "s1 := { a := s1; };\n"),
    ("reference_to_a_field_name", "a := { b := 1; };\nc := { d := $b; };\n"),
    ("reference_to_nested_name_is_missing", "a := { b := { c := 1; }; };\nd := [ $c ];\n"),
    ("dollar_in_comment", "// $a\na := [ 1 ]; /* $a */\n"),
    ("amp_document", "#!amp\na := [ 1, 2 ];\n"),
]
for name, text in valid:
    put(O + name + ".anvl", text)

# a deep acyclic chain of arrays, a binary tree of references (2^12 paths), a ladder, a wide fan
n = 300
put(O + "deep_chain_300.anvl", "".join("s%d := [ $s%d ];\n" % (i, i + 1) for i in range(n - 1)) + "s%d := [ 1 ];\n" % (n - 1))
depth = 12
put(O + "binary_fan_12.anvl", "".join("x%d := [ $x%d, $x%d ];\n" % (i, i + 1, i + 1) for i in range(depth)) + "x%d := [ 1 ];\n" % depth)
m = 20
put(O + "ladder_20.anvl", "".join("l%d := [ $l%d, $l%d ];\n" % (i, i + 1, i + 2) for i in range(m - 2)) + "l%d := [ $l%d, 1 ];\nl%d := [ 1 ];\n" % (m - 2, m - 1, m - 1))
k = 300
put(O + "wide_fan_300.anvl", "t := { v := 1; };\nhub := {\n" + "".join("   f%d := $t;\n" % i for i in range(k)) + "};\n")
put(O + "wide_fan_of_containers_300.anvl", "".join("t%d := { v := %d; };\n" % (i, i) for i in range(k)) + "hub := [\n" + "".join("   $t%d,\n" % i for i in range(k)) + "];\n")
put(O + "chain_of_references_300.anvl", "".join("c%d := $c%d;\n" % (i, i + 1) for i in range(n - 1)) + "c%d := { v := 1; };\n" % (n - 1))
put(O + "chain_of_references_into_container_without_way_back.anvl", "start := { x := $c0; };\n" + "".join("c%d := $c%d;\n" % (i, i + 1) for i in range(50)) + "c50 := { v := 1; };\n")
put(O + "inheritance_chain_100.anvl", "b0 := { v := 1; };\n" + "".join("b%d : b%d := { f%d := %d; };\n" % (i, i - 1, i, i) for i in range(1, 100)) + "r := { x := $b99; };\n")
put(O + "references_to_every_level_of_an_inheritance_chain.anvl", "b0 := { v := 1; };\n" + "".join("b%d : b%d := { f%d := %d; };\n" % (i, i - 1, i, i) for i in range(1, 30)) + "r := [ " + ", ".join("$b%d" % i for i in range(30)) + " ];\n")

# ---------------------------------------------------------------------------------------------------- scale/
S = "scale/"
put(S + "ring_300.anvl", "".join("s%d := [ $s%d ];\n" % (i, (i + 1) % n) for i in range(n)))
put(S + "ring_300_defined_backwards.anvl", "".join("s%d := [ $s%d ];\n" % (i, (i + 1) % n) for i in reversed(range(n))))
put(S + "chain_300_closed_at_the_end.anvl", "".join("s%d := [ $s%d ];\n" % (i, i + 1) for i in range(n - 1)) + "s%d := [ $s%d ];\n" % (n - 1, n // 2))
put(S + "ladder_ring_200.anvl", "".join("s%d := [ $s%d, $s%d ];\n" % (i, (i + 1) % 200, (i + 2) % 200) for i in range(200)))
put(S + "hub_cycle_300.anvl", "hub := {\n" + "".join("   f%d := $t%d;\n" % (i, i) for i in range(k)) + "};\n" + "".join("t%d := { v := %d; };\n" % (i, i) for i in range(k - 1)) + "t%d := { v := $hub; };\n" % (k - 1))
put(S + "inheritance_chain_closed_by_a_reference.anvl", "b0 := { v := $b99; };\n" + "".join("b%d : b%d := { f%d := %d; };\n" % (i, i - 1, i, i) for i in range(1, 100)))
put(S + "deep_nesting_300_closed.anvl", "a := " + "[ " * 300 + "$a" + " ]" * 300 + ";\n")
put(S + "deep_nesting_300_open.anvl", "b := { v := 1; };\na := " + "[ " * 300 + "$b" + " ]" * 300 + ";\n")
