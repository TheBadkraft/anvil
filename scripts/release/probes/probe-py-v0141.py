"""v0.14.1-rc probe for anvil-py: argv[1] = extracted package dir. Prints PASS/FAIL lines."""
import sys, os
root = sys.argv[1]
sys.path.insert(0, os.path.join(root, "src") if os.path.isdir(os.path.join(root, "src")) else root)
from anvil import AnvilDocument, AnvilBuilder, AnvilWriter, AnvilDialect, WriterErrorCode, ErrorCode
from anvil.writer_error import AnvilWriterError
bad = 0
def t(name, fn):
    global bad
    try:
        fn(); print("PASS py-v0141: " + name)
    except Exception as e:
        bad = 1; print(f"FAIL py-v0141: {name}: {e!r}")
def refused(src, pos):
    d = AnvilDocument.load_buffer(src)
    assert d is not None and d.has_errors, "must not load clean"
    e = d.error
    assert e.category == ErrorCode.RESOLVE, e.category
    assert e.message == "A reference leads back to a value that contains it", e.message
    assert f"{e.line}:{e.column}" == pos, f"{e.line}:{e.column}"
def clean(src):
    d = AnvilDocument.load_buffer(src); assert d is not None and not d.has_errors
def code(fn):
    try: fn(); return 0
    except AnvilWriterError as e: return int(e.code)
t("s1 := { a := $s1; }; is refused (resolve) at 2:14", lambda: refused("#!aml\ns1 := { a := $s1; };\n", "2:14"))
t("a nested reference at 2:21", lambda: refused("#!aml\ns1 := { a := { b := $s1; }; };\n", "2:21"))
t("an array holding a reference to itself at 2:8", lambda: refused("#!aml\na := [ $a ];\n", "2:8"))
t("two objects that refer to each other at 2:13", lambda: refused("#!aml\na := { x := $b; };\nb := { y := $a; };\n", "2:13"))
t("a cycle closed through inheritance at 2:13", lambda: refused("#!aml\na := { x := $b; };\nb : a := { z := 1; };\n", "2:13"))
t("a reference to a scalar loads", lambda: clean("#!aml\nv := 1;\ns1 := { a := $v; };\n"))
t("the plain chain cycle still loads", lambda: clean("#!aml\na := $b;\nb := $a;\n"))
def cyc():
    b = AnvilBuilder.create(AnvilDialect.AML); o = b.object(); o.add("a", b.varref("s1")); b.add("s1", o); return b
def code24():
    assert WriterErrorCode.REFERENCE_CYCLE == 24
    assert code(lambda: cyc().emit()) == 24
t("builder: emit is writer code 24", code24)
t("builder: write is writer code 24", lambda: (_ for _ in ()).throw(AssertionError(code(lambda: cyc().write(AnvilWriter.create(AnvilDialect.AML))))) if code(lambda: cyc().write(AnvilWriter.create(AnvilDialect.AML))) != 24 else None)
def scalar_ok():
    b = AnvilBuilder.create(AnvilDialect.AML); b.add("v", b.value(1)); o = b.object(); o.add("a", b.varref("v")); b.add("s1", o)
    assert "$v" in b.emit()
t("builder: a reference to a scalar emits", scalar_ok)
sys.exit(bad)
