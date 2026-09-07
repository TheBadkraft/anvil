# AnvilScript — Theoretical Sketches

This note collects concrete-but-speculative examples for AnvilScript built-ins, extension methods, host bindings, and compilation formats. Nothing here is committed; it is source material for the design in `notes/anvilscript-design.md`.

## `math.anvs`

```anvs
#!anvs

using "c:anvil.std.math";

abs (x) => {
    return math._abs(x);
};

floor (x) => {
    return math._floor(x);
};

min (a, b) => {
    if (a < b) { return a; }
    return b;
};

max (a, b) => {
    if (a > b) { return a; }
    return b;
};
```

The `_abs` and `_floor` functions are host callbacks registered from the C standard math library. The public functions are ordinary AnvilScript wrappers so arity, error messages, and future behavior live in source.

## `time.anvs`

```anvs
#!anvs

using "c:anvil.std.time";

now () => {
    return time._monotonic_now();
};

elapsed (start) => {
    return time._monotonic_now() - start;
};
```

`time.now` returns a duration-compatible numeric value. A future `time.utc_now` would return an object with calendar fields.

## `io.anvs`

```anvs
#!anvs

using "c:anvil.std.io";

print (value) => {
    io._print(value.as_string());
};

print_line (value) => {
    io._print(value.as_string() + "\n");
};
```

`io.print` is host-dependent; it delegates to a registered callback that knows how to emit output in the current process.

## `list.anvs`

```anvs
#!anvs

using "c:anvil.std.collections";

new () => {
    return list._new();
};

from_array (arr) => {
    var l = list.new();
    var i = 0;
    while (i < array.length(arr)) {
        list.append(l, array.at(arr, i));
        i = i + 1;
    }
    return l;
};

append (l, value) => {
    return list._append(l, value);
};

size (l) => {
    return list._size(l);
};

at (l, index) => {
    return list._at(l, index);
};
```

## `dict.anvs`

```anvs
#!anvs

using "c:anvil.std.collections";

new () => {
    return dict._new();
};

set (d, key, value) => {
    return dict._set(d, key, value);
};

get (d, key) => {
    return dict._get(d, key);
};

has (d, key) => {
    return dict._has(d, key);
};

to_object (d) => {
    // produces an AML object whose keys are the dict keys
    return dict._to_object(d);
};
```

## Extension method example

```anvs
extend array {
    first_where(pred) => {
        var i = 0;
        while (i < array.length(this)) {
            var v = array.at(this, i);
            if (pred(v)) { return v; }
            i = i + 1;
        }
        return null;
    };

    any(pred) => {
        return this.first_where(pred) != null;
    };
}

extend string {
    starts_with(prefix) => {
        if (string.length(prefix) > string.length(this)) {
            return false;
        }
        return string.slice(this, 0, string.length(prefix)) == prefix;
    };
}
```

Usage:

```anvs
var nums = [1, 2, 3, 4];
var has_even = nums.any(x => x % 2 == 0);
var greeting = "hello world";
var is_hello = greeting.starts_with("hello");
```

## Host callback registration (C)

```c
#include <anvil.h>
#include <sigma/math.h>

static anvil_value host_math_abs(anvil_document doc,
                                 anvil_value *args, usize arg_count) {
    if (arg_count != 1) {
        anvil_set_error(doc, ANVL_ERR_ASL_ARITY_MISMATCH, ...);
        return anvil_null();
    }
    double x = anvil_value_as_number(args[0]);
    return anvil_number(fabs(x));
}

void anvil_register_std_math(anvil_document doc) {
    anvil_register_function(doc, "math._abs", host_math_abs);
}
```

## Compiled `.anvlo` sketch

AnvilScript source can be precompiled to `.anvlo` in the same way ANVL documents can. The compiled object contains:

- the module's function registry entries;
- a serialized runtime AST for each function body;
- the namespace bindings produced by `import`/`using`;
- source-span metadata for error reporting.

Loading an `.anvlo` bypasses parsing for the body AST but still requires the runtime to set up the same registry and stack state. Built-in modules can therefore ship either as `.anvs` source or as `.anvlo` compiled objects without changing how callers use them.
