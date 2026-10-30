anvil-writer-script 1
# a field may reuse an outer name
dialect AML
statement "x" NULL
begin_object
statement "x" NULL
begin_object
statement "x" NULL
int 1
end_object
end_object
finish
