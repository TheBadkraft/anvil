anvil-writer-script 1
# any call after finish is STATE (and the output is gone with the sticky error)
dialect AML
statement "a" NULL
int 1
finish
int 2
data
