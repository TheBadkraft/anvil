anvil-writer-script 1
# a base inside an object is refused, whatever it names
dialect AML
statement "base" NULL
begin_object
statement "x" NULL
int 1
end_object
statement "o" NULL
begin_object
statement "inner" "base"
