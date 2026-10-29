# libanvil v@VERSION@ — Linux x86_64

Prebuilt static (`libanvil.a`) and shared (`libanvil.so`) release builds of Anvil Native: the core
parser (AML/AMP), the opt-in type registry, AnvilSchema, and the streaming writer and document
builder. Built with `-O2`, stripped of debug symbols. Reports its version as `@VERSION@+@BUILD@`
(`anvil_get_version()`); `BUILD_INFO.json` records the source commit (`@COMMIT@`).

## Using it

```sh
gcc your_program.c -I include -L lib -lanvil -o your_program        # static or shared
# or, to force static linking:
gcc your_program.c -I include lib/libanvil.a -o your_program
```

## What's included

- `lib/libanvil.a`, `lib/libanvil.so`
- `include/anvil_flat.h`: the primary public API (plain function calls)
- `include/anvil_vtable.h`: the same API as a vtable-style struct of function pointers
- `include/anvil_types.h`: public ABI types (opaque handles, stable error categories)
- `include/anvil_type_registry.h`: the opt-in type registry (`@[types]`)
- `include/anvil_schema.h`: AnvilSchema (`@[schema]`)
- `include/anvil_writer*.h`, `include/anvil_builder*.h`: the streaming writer and document builder
- `BUILD_INFO.json`: version, build number and source commit

Only Linux x86_64 is built today. macOS/Windows: build from source (`make lib` at the repo root).
