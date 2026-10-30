anvil-writer-script 1
# only a quote or a line break is refused
dialect AML
include "a;b.anvl"
include "a'b.anvl"
include "a{b}.anvl"
include "a\\b.anvl"
statement "a" NULL
int 1
finish
