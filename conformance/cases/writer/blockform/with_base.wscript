anvil-writer-script 1
# d : base { x := 1; };
dialect AML
statement "base" NULL
begin_object
statement "z" NULL
int 0
end_object
statement_block "d" "base"
begin_object
statement "x" NULL
int 1
end_object
finish
