anvil-writer-script 1
# inside a quoted attribute only the quote and line breaks are refused
dialect AML
statement "a" NULL
attribute_string "note" "a, b] c = d \\ e"
int 1
finish
