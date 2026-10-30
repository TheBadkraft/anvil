anvil-writer-script 1
dialect AML
statement "base" NULL
begin_object
statement "x" NULL
int 1
end_object
statement "o" NULL
begin_object
statement "inner" "base"
begin_object
statement "y" NULL
int 2
end_object
end_object
finish
