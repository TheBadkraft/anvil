anvil-writer-script 1
# a nested block may not have a base; this used to be written
dialect AML
statement "base" NULL
begin_object
statement "x" NULL
int 1
end_object
statement "o" NULL
begin_object
statement_block "inner" "base"
