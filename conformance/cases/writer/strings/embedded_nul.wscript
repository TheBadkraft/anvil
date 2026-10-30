anvil-writer-script 1
# a NUL byte is data: strings are length-delimited
dialect AML
statement "s" NULL
string "a\x00b"
finish
