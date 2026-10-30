#!/bin/sh
# Writes the drift tables to stdout: every enum declared in the public headers (include/*.h) and in
# include/internal/module.h (the dump's node vocabulary), the keywords and grammar tokens of
# include/constants.h, and the known format names. JS (and every other port) compares its own tables with
# this file. Plain text, one entry per line:
#
#   drift-tables 1
#   table <name>
#   <key> <value>        (enums: "NAME number"; keywords: "word group"; tokens: "NAME char-code")
#
# Run from the repo root. The enum values are computed here (an enumerator without "= n" continues from the
# previous one), from the headers with comments removed by the C preprocessor (cpp -fpreprocessed).
set -eu
cd "$(dirname "$0")/.."
cc=${CC:-gcc}

strip() { $cc -E -P -fpreprocessed -dD -x c "$1" 2>/dev/null; }

enums() {
   # prints "table enum:<typedef>" and "NAME value" for every `typedef enum ... { ... } name;` in the file
   strip "$1" | tr '\n' ' ' | awk '
   {
      s = $0
      while ((i = index(s, "typedef enum")) > 0) {
         s = substr(s, i + 12)
         o = index(s, "{"); c = index(s, "}")
         if (o == 0 || c == 0 || c < o) break
         body = substr(s, o + 1, c - o - 1)
         rest = substr(s, c + 1)
         e = index(rest, ";")
         name = substr(rest, 1, e - 1); gsub(/[ \t]/, "", name)
         s = substr(rest, e + 1)
         printf "table enum:%s\n", name
         n = split(body, m, ",")
         next_value = 0
         for (k = 1; k <= n; k++) {
            item = m[k]; gsub(/^[ \t]+|[ \t]+$/, "", item)
            if (item == "") continue
            if (index(item, "=") > 0) {
               split(item, kv, "=")
               id = kv[1]; val = kv[2]
               gsub(/[ \t]/, "", id); gsub(/[ \t]/, "", val)
               next_value = val + 0
            } else { id = item; gsub(/[ \t]/, "", id) }
            printf "%s %d\n", id, next_value
            next_value++
         }
      }
   }'
}

echo "drift-tables 1"
for h in include/errors.h include/types.h include/anvil_types.h include/anvil_schema.h include/anvil_type_registry.h \
         include/anvil_writer_types.h include/anvil_builder_types.h include/internal/module.h; do
   [ -f "$h" ] && enums "$h"
done

# keywords: the reserved words (illegal as an identifier and as a bare value) and the value words (illegal as an
# identifier, classify a value); the group is the one include/constants.h documents.
echo "table keywords"
strip include/constants.h | sed -n 's/^#define ANVL_KEYWORD_\([A-Z]*\) "\(.*\)"$/\1 \2/p' | grep -v '_LEN ' | while read -r id word; do
   case $id in TRUE|FALSE|NULL) group=value;; *) group=reserved;; esac
   echo "$word $group"
done

echo "table shebangs"
strip include/constants.h | sed -n 's/^#define ANVL_SHEBANG_\(AML\|AMP\|ASL\) "\(.*\)"$/\2 \1/p'
echo "table extensions"
strip include/constants.h | sed -n 's/^#define ANVL_EXT_\(AML\|AMP\|ASL\|ANVL\) "\(.*\)"$/\2 \1/p'

# grammar tokens that are a single character: "NAME code" (decimal byte value)
echo "table tokens"
strip include/constants.h | sed -n "s/^#define ANVL_TOK_\([A-Z_]*\) '\(.*\)'\$/\1 \2/p" | while IFS= read -r line; do
   id=${line%% *}; ch=${line#* }
   case $ch in
      '\\') code=92;; '\n') code=10;; '\r') code=13;; '\t') code=9;;
      *) code=$(printf '%s' "$ch" | od -An -tu1 | tr -d ' ');;
   esac
   echo "$id $code"
done
echo "table token-strings"
strip include/constants.h | sed -n 's/^#define ANVL_TOK_\([A-Z_]*\) "\(.*\)"$/\1 \2/p' | grep -v '_LEN '

# format names: the closed set anvil_format_known accepts (src/anvil_types.c), which include/anvil_type_registry.h documents
echo "table formats"
fmts=$(sed -n '/^bool anvil_format_known/,/^}/p' src/anvil_types.c | grep -o 'strcmp(name, "[a-z0-9_]*")' | sed 's/.*"\(.*\)".*/\1/')
doc=$(grep -o 'lexical formats: [^)]*' include/anvil_type_registry.h | head -1 | grep -o '`[a-z0-9_]*`' | tr -d '`')
[ "$(echo $fmts)" = "$(echo $doc)" ] || { echo "gen-drift-tables: format names differ between src/anvil_types.c ($fmts) and include/anvil_type_registry.h ($doc)" >&2; exit 1; }
for f in $fmts; do echo "$f"; done
