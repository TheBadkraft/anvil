anvil-writer-script 1
# blob content is length-delimited
dialect AML
statement "b" NULL
blob "t" "a\x00b"
finish
