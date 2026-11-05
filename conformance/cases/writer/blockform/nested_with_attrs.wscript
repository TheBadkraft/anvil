anvil-writer-script 1
# a nested block may have attributes, but no base (only a top-level statement has a base)
dialect AML
statement "base" NULL
begin_object
statement "z" NULL
int 0
end_object
statement "o" NULL
begin_object
statement_block "inner" NULL
attribute "k" "v"
begin_object
statement "x" NULL
int 1
end_object
end_object
finish
