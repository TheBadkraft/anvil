anvil-writer-script 1
# the reader rejects an include after a module attribute (4201), so the writer refuses it
dialect AML
attribute "x" "1"
include "inc.anvl"
