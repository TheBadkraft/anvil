{
  "targets": [
    {
      "target_name": "anvil_binding",
      "sources": [
        "src/binder.c",
        "../../src/core/anvil.c",
        "../../src/core/context.c",
        "../../src/core/errors.c",
        "../../src/core/memory.c",
        "../../src/core/operators.c",
        "../../src/core/parser.c",
        "../../src/core/strings.c",
        "../../src/core/symbols.c",
        "../../src/core/types.c",
        "../../src/import/import.c",
        "../../src/resolver/resolver.c",
        "../../src/schema/schema.c",
        "../../src/utilities/utils.c",
        "../../src/vars/vars.c"
      ],
      "include_dirs": [
        "../../include",
        "../../src/core"
      ],
      "cflags": [
        "-std=c2x",
        "-Wall",
        "-Wextra",
        "-D_POSIX_C_SOURCE=200809L"
      ],
      "defines": [
        "ANVL_BUILD=0"
      ]
    }
  ]
}
