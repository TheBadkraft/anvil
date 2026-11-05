/* v0.14.0-rc probe for the core tarball (run against the EXTRACTED package; key=value lines on stdout).
 * Build: cc probe-core-v014.c -I<pkg>/include -L<pkg>/lib -lanvil -Wl,-rpath,<pkg>/lib
 * Usage: probe-core-v014 <scratch-dir>   (writes root.anvl and inc/a.anvl there)
 * Covers: nested base 4022 at once (no endless walk: the caller runs this under `timeout`), writer/builder code 23,
 * block form output with codes 21 and 22, anvil_error_get_source, and the include error position. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "anvil_flat.h"
#include "anvil_writer.h"
#include "anvil_builder.h"

static void errtext(anvil_error e, char *msg, size_t n, char *src, size_t sn, size_t *line, size_t *col) {
    msg[0] = src[0] = 0; *line = *col = 0;
    if (!e) return;
    anvil_error_get_message(e, msg, n); anvil_error_get_source(e, src, sn);
    *line = anvil_error_get_line(e); *col = anvil_error_get_column(e);
}
static void emit_load(const char *label, const char *doc) {
    anvil_document d = anvil_load_buffer(doc, strlen(doc));
    anvil_error e = d ? anvil_document_get_error(d) : NULL;
    char m[512], s[512]; size_t l, c; errtext(e, m, sizeof m, s, sizeof s, &l, &c);
    printf("%s=%s\n", label, e ? m : "<parsed without error>");
}
static void wr(const char *label, int code) { printf("%s=%d\n", label, code); }
static void writer_code(const char *label, anvil_writer w) { wr(label, (int)anvil_writer_get_error(w)); }

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    /* 1. the endless-walk reproduction fails at load, and so do both forms at depth */
    emit_load("nested1", "#!aml\ns1 := { f3 : s1 { a := 1; }; };\n");
    emit_load("nested2", "#!aml\nb := { a := 1; };\ns1 := { f3 : b := { a := 1; }; };\n");
    emit_load("topbase", "#!aml\nb := { a := 1; };\nd : b := { c := 2; };\n");
    /* 2. writer: base inside an object is code 23; block form needs an object (21); base naming a block statement (22) */
    anvil_writer w = anvil_writer_new(ANVIL_WRITER_AML);
    anvil_writer_statement(w, "o", NULL); anvil_writer_begin_object(w);
    bool ok = anvil_writer_statement(w, "x", "b");
    printf("writer_nested_ok=%d\n", ok); writer_code("writer_nested_code", w); anvil_writer_dispose(w);
    w = anvil_writer_new(ANVIL_WRITER_AML);
    anvil_writer_statement_block(w, "ro", NULL); ok = anvil_writer_int(w, 1);
    printf("writer_block_scalar_ok=%d\n", ok); writer_code("writer_block_scalar_code", w); anvil_writer_dispose(w);
    w = anvil_writer_new(ANVIL_WRITER_AML);
    anvil_writer_statement_block(w, "ro", NULL); anvil_writer_begin_object(w);
    anvil_writer_statement(w, "a", NULL); anvil_writer_int(w, 1); anvil_writer_end_object(w);
    ok = anvil_writer_statement(w, "d", "ro");
    printf("writer_base_block_ok=%d\n", ok); writer_code("writer_base_block_code", w); anvil_writer_dispose(w);
    /* 3. block form output */
    w = anvil_writer_new(ANVIL_WRITER_AML);
    anvil_writer_statement_block(w, "ro", NULL); anvil_writer_begin_object(w);
    anvil_writer_statement(w, "a", NULL); anvil_writer_int(w, 1); anvil_writer_end_object(w);
    anvil_writer_finish(w);
    size_t n = 0; const char *t = anvil_writer_data(w, &n);
    char *esc = malloc(n * 2 + 1), *p = esc;
    for (size_t i = 0; i < n; i++) { if (t[i] == '\n') { *p++ = '\\'; *p++ = 'n'; } else *p++ = t[i]; }
    *p = 0; printf("writer_block_text=%s\n", esc); free(esc);
    anvil_document bd = anvil_load_buffer(t, n);
    printf("writer_block_reloads=%d\n", bd && !anvil_document_get_error(bd)); anvil_writer_dispose(w);
    /* 4. builder: set_base on a nested member is 23 at once; set_block on a scalar is 21 */
    anvil_builder b = anvil_builder_new(ANVIL_WRITER_AML);
    anvil_node o = anvil_builder_object(b);
    anvil_member inner = anvil_builder_add(b, o, "f", anvil_builder_int(b, 1));
    anvil_builder_add(b, NULL, "o", o);
    ok = anvil_builder_set_base(b, inner, "o");
    printf("builder_nested_ok=%d\nbuilder_nested_code=%d\n", ok, (int)anvil_builder_get_error(b)); anvil_builder_dispose(b);
    b = anvil_builder_new(ANVIL_WRITER_AML);
    anvil_member sc = anvil_builder_add(b, NULL, "s", anvil_builder_int(b, 1));
    ok = anvil_builder_set_block(b, sc, true);
    size_t bl = 0; const char *bt = ok ? anvil_builder_emit(b, &bl) : NULL;
    printf("builder_block_scalar_ok=%d\nbuilder_block_scalar_emit_null=%d\nbuilder_block_scalar_code=%d\n", ok, bt == NULL, (int)anvil_builder_get_error(b)); anvil_builder_dispose(b);
    b = anvil_builder_new(ANVIL_WRITER_AML);
    anvil_node ob = anvil_builder_object(b); anvil_builder_add(b, ob, "a", anvil_builder_int(b, 1));
    anvil_member bm = anvil_builder_add(b, NULL, "ro", ob);
    printf("builder_set_block=%d\n", anvil_builder_set_block(b, bm, true));
    anvil_node ob2 = anvil_builder_object(b); anvil_builder_add(b, ob2, "c", anvil_builder_int(b, 2));
    anvil_member dm = anvil_builder_add(b, NULL, "d", ob2);
    anvil_builder_set_base(b, dm, "ro");
    const char *bt2 = anvil_builder_emit(b, &bl);
    printf("builder_base_block_emit_null=%d\nbuilder_base_block_code=%d\n", bt2 == NULL, (int)anvil_builder_get_error(b)); anvil_builder_dispose(b);
    /* 5. source and include position: root.anvl includes inc/a.anvl, which has a missing include on line 3 col 3 */
    char root[1024], inc[1024], cmd[1100];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/inc'", argv[1]); if (system(cmd)) return 2;
    snprintf(inc, sizeof inc, "%s/inc/a.anvl", argv[1]); snprintf(root, sizeof root, "%s/root.anvl", argv[1]);
    FILE *f = fopen(inc, "w"); fputs("#!aml\n\n  include \"missing.anvl\";\n", f); fclose(f);
    f = fopen(root, "w"); fputs("#!aml\ninclude \"inc/a.anvl\";\n", f); fclose(f);
    anvil_document d = anvil_load(root); anvil_error e = d ? anvil_document_get_error(d) : NULL;
    char m[512], s[512]; size_t l, c; errtext(e, m, sizeof m, s, sizeof s, &l, &c);
    printf("inc_message=%s\ninc_source=%s\ninc_line=%zu\ninc_col=%zu\n", m, s, l, c);
    f = fopen(root, "w"); fputs("#!aml\n\n    include \"gone.anvl\";\nx := 1;\n", f); fclose(f);
    d = anvil_load(root); e = d ? anvil_document_get_error(d) : NULL; errtext(e, m, sizeof m, s, sizeof s, &l, &c);
    printf("root_message=%s\nroot_source=[%s]\nroot_line=%zu\nroot_col=%zu\n", m, s, l, c);
    return 0;
}
