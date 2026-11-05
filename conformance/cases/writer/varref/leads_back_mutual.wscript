anvil-writer-script 1
# two statements that refer to each other are written
dialect AML
statement "a" NULL
begin_object
statement "x" NULL
varref "b"
end_object
statement "b" NULL
begin_object
statement "y" NULL
varref "a"
end_object
finish
