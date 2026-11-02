anvil-writer-script 1
# a field may reuse a top-level block name
dialect AML
statement_block "a" NULL
begin_object
statement "x" NULL
int 1
end_object
statement "b" NULL
begin_object
statement "a" NULL
int 2
end_object
finish
