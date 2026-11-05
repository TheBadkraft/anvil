anvil-writer-script 1
# s1 := { a := $s1; }; is written: only the reader and the builder refuse it
dialect AML
statement "s1" NULL
begin_object
statement "a" NULL
varref "s1"
end_object
finish
