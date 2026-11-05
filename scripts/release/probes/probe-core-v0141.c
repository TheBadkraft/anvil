/* v0.14.1-rc probe for the core tarball (run against the EXTRACTED package; key=value lines on stdout).
 * Build: cc probe-core-v0141.c -I<pkg>/include -L<pkg>/lib -lanvil -Wl,-rpath,<pkg>/lib
 * Covers: a $ reference that leads back to a container holding it is refused at load (4054, category RESOLVE, position of
 * the first on-cycle reference) and never loops; the plain chain cycle still loads; the builder refuses the same graph
 * with writer code 24 at emit and at write. The caller runs this under `timeout`. */
#include <stdio.h>
#include <string.h>
#include "anvil_flat.h"
#include "anvil_writer.h"
#include "anvil_builder.h"

static void load(const char *label, const char *doc) {
    anvil_document d = anvil_load_buffer(doc, strlen(doc));
    anvil_error e = d ? anvil_document_get_error(d) : NULL;
    char m[512] = "";
    if (e) anvil_error_get_message(e, m, sizeof m);
    printf("%s_msg=%s\n%s_cat=%d\n%s_pos=%zu:%zu\n", label, e ? m : "<parsed without error>", label, e ? (int)anvil_error_get_category(e) : -1,
           label, e ? anvil_error_get_line(e) : 0, e ? anvil_error_get_column(e) : 0);
}
int main(void) {
    printf("resolve_category=%d\n", (int)ANVIL_ERR_RESOLVE);
    load("self", "#!aml\ns1 := { a := $s1; };\n");
    load("deep", "#!aml\ns1 := { a := { b := $s1; }; };\n");
    load("array", "#!aml\na := [ $a ];\n");
    load("pair", "#!aml\na := { x := $b; };\nb := { y := $a; };\n");
    load("viabase", "#!aml\na := { x := $b; };\nb : a := { z := 1; };\n");
    load("scalar", "#!aml\nv := 1;\ns1 := { a := $v; };\n");
    load("chain", "#!aml\na := $b;\nb := $a;\n");
    /* builder: a member whose object holds a reference to that member */
    anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
    anvil_node o = anvil_builder_object(b);
    anvil_builder_add(b, o, "a", anvil_builder_varref(b, "s1"));
    anvil_builder_add(b, NULL, "s1", o);
    size_t n = 0; const char *t = anvil_builder_emit(b, &n);
    printf("builder_emit_null=%d\nbuilder_emit_code=%d\n", t == NULL, (int)anvil_builder_get_error(b)); anvil_builder_dispose(b);
    b = anvil_builder_new(ANVIL_WRITER_AML);
    o = anvil_builder_object(b);
    anvil_builder_add(b, o, "a", anvil_builder_varref(b, "s1"));
    anvil_builder_add(b, NULL, "s1", o);
    anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
    bool ok = anvil_builder_write(b, w);
    printf("builder_write_ok=%d\nbuilder_write_code=%d\n", ok, (int)anvil_builder_get_error(b)); anvil_writer_dispose(w); anvil_builder_dispose(b);
    /* a reference to a scalar and a reference with no way back still build */
    b = anvil_builder_new(ANVIL_WRITER_AML);
    anvil_builder_add(b, NULL, "v", anvil_builder_int(b, 1));
    o = anvil_builder_object(b); anvil_builder_add(b, o, "a", anvil_builder_varref(b, "v"));
    anvil_builder_add(b, NULL, "s1", o);
    t = anvil_builder_emit(b, &n);
    printf("builder_ok_emit=%d\nbuilder_ok_code=%d\n", t != NULL, (int)anvil_builder_get_error(b)); anvil_builder_dispose(b);
    /* the streaming writer holds no document and does not see it */
    w = anvil_writer_new(ANVIL_WRITER_AML);
    anvil_writer_statement(w, "s1", NULL); anvil_writer_begin_object(w); anvil_writer_statement(w, "a", NULL); anvil_writer_varref(w, "s1"); anvil_writer_end_object(w);
    printf("streaming_writer_code=%d\n", (int)anvil_writer_get_error(w)); anvil_writer_dispose(w);
    return 0;
}
