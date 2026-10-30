anvil-writer-script 1
dialect AML
statement "base" NULL
begin_object
statement "x" NULL
int 1
end_object
statement "child" "base"
attribute "k" "v"
begin_object
statement "y" NULL
int 2
end_object
finish
