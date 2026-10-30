anvil-writer-script 1
# control byte 0x15 is written raw (only quote, backslash, LF, TAB, CR are escaped)
dialect AML
statement "s" NULL
string "\x15"
finish
