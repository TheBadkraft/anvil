#include <node_api.h>

#include "anvil.h"
#include "context.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static napi_value make_null(napi_env env) {
    napi_value v;
    napi_get_null(env, &v);
    return v;
}

static napi_value make_string(napi_env env, const char *s) {
    napi_value out;
    napi_create_string_utf8(env, s ? s : "", NAPI_AUTO_LENGTH, &out);
    return out;
}

static void set_named_string(napi_env env, napi_value obj, const char *key, const char *value) {
    napi_value v = make_string(env, value);
    napi_set_named_property(env, obj, key, v);
}

static void set_named_int64(napi_env env, napi_value obj, const char *key, int64_t value) {
    napi_value v;
    napi_create_int64(env, value, &v);
    napi_set_named_property(env, obj, key, v);
}

static napi_value js_get_version(napi_env env, napi_callback_info info) {
    (void)info;
    return make_string(env, Anvil.get_version());
}

static napi_value js_last_error(napi_env env, napi_callback_info info) {
    (void)info;
    if (!Anvil.error_is_set()) {
        return make_null(env);
    }

    const anvl_error_state *e = Anvil.error_get();
    napi_value obj;
    napi_create_object(env, &obj);
    set_named_string(env, obj, "message", e && e->message ? e->message : "");
    set_named_int64(env, obj, "code", e ? (int64_t)e->code : 0);
    set_named_int64(env, obj, "line", e ? (int64_t)e->line : 0);
    set_named_int64(env, obj, "column", e ? (int64_t)e->column : 0);
    return obj;
}

/*
 * Minimal scaffold parse: validates source parseability and returns null.
 * Full AST-to-JS mapping will be implemented by the binding team.
 */
static napi_value js_parse(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1];
    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (argc < 1) {
        return make_null(env);
    }

    size_t len = 0;
    napi_get_value_string_utf8(env, argv[0], NULL, 0, &len);
    char *src = (char *)malloc(len + 1);
    if (!src) {
        return make_null(env);
    }

    size_t out_len = 0;
    napi_get_value_string_utf8(env, argv[0], src, len + 1, &out_len);

    ctx_builder b = Context.get_builder();
    b->set_source(b, src, out_len);
    context ctx = b->build(b);

    napi_value ret = make_null(env);
    if (ctx && Context.parse(ctx)) {
        ret = make_null(env);
    }

    if (ctx) {
        Context.dispose(ctx);
    }
    free(src);
    return ret;
}

static napi_value js_load(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1];
    napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (argc < 1) {
        return make_null(env);
    }

    size_t len = 0;
    napi_get_value_string_utf8(env, argv[0], NULL, 0, &len);
    char *path = (char *)malloc(len + 1);
    if (!path) {
        return make_null(env);
    }

    size_t out_len = 0;
    napi_get_value_string_utf8(env, argv[0], path, len + 1, &out_len);
    (void)out_len;

    ctx_builder b = Context.get_builder();
    bool loaded = b->load_file(b, path);
    context ctx = loaded ? b->build(b) : NULL;

    napi_value ret = make_null(env);
    if (ctx && Context.parse(ctx)) {
        ret = make_null(env);
    }

    if (ctx) {
        Context.dispose(ctx);
    }
    free(path);
    return ret;
}

static napi_value init(napi_env env, napi_value exports) {
    napi_value fn;

    napi_create_function(env, "getVersion", NAPI_AUTO_LENGTH, js_get_version, NULL, &fn);
    napi_set_named_property(env, exports, "getVersion", fn);

    napi_create_function(env, "parse", NAPI_AUTO_LENGTH, js_parse, NULL, &fn);
    napi_set_named_property(env, exports, "parse", fn);

    napi_create_function(env, "load", NAPI_AUTO_LENGTH, js_load, NULL, &fn);
    napi_set_named_property(env, exports, "load", fn);

    napi_create_function(env, "lastError", NAPI_AUTO_LENGTH, js_last_error, NULL, &fn);
    napi_set_named_property(env, exports, "lastError", fn);

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
