anvil-writer-script 1
# a blob is verbatim: nothing is escaped
dialect AML
statement "b" NULL
blob "t" "say \"x\" \\n \\\\ "
finish
