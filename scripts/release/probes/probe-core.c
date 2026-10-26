/* Release probe for the core tarball: prints version, include-doc error, name-doc value.
 * Build: cc probe-core.c -I<pkg>/include -L<pkg>/lib -lanvil -Wl,-rpath,<pkg>/lib */
#include <stdio.h>
#include <string.h>
#include "anvil_flat.h"

static void probe_error(const char *label, const char *src) {
    anvil_document doc = anvil_load_buffer(src, strlen(src));
    anvil_error err = doc ? anvil_document_get_error(doc) : NULL;
    char msg[512] = "";
    if (err) anvil_error_get_message(err, msg, sizeof msg);
    printf("%s=%s\n", label, doc ? (err ? msg : "<parsed without error>") : "<null document>");
}

int main(void) {
    printf("version=%s\n", anvil_get_version());
    probe_error("include", "#!aml\ninclude \"missing.anvl\";\nx := 1;\n");
    const char *src = "#!aml\nname := \"David\";\n";
    anvil_document doc = anvil_load_buffer(src, strlen(src));
    char text[128] = "<none>";
    if (doc) {
        anvil_statement_iterator it = anvil_document_get_statements(doc);
        anvil_statement st;
        if (it && anvil_statement_iterator_next(it, &st)) {
            anvil_value v = anvil_statement_get_value(st);
            if (v) anvil_value_get_text(v, text, sizeof text);
        }
        if (it) anvil_statement_iterator_dispose(it);
    }
    printf("name=%s\n", text);
    return 0;
}
