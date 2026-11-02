anvil-writer-script 1
# a nested block may have a base and attributes; the base is looked up among the top-level names
dialect AML
statement "base" NULL
begin_object
statement "z" NULL
int 0
end_object
statement "o" NULL
begin_object
statement_block "inner" "base"
attribute "k" "v"
begin_object
statement "x" NULL
int 1
end_object
end_object
finish
