anvil-writer-script 1
# a nested base is refused before its target is looked at
dialect AML
statement_block "blk" NULL
begin_object
statement "f" NULL
int 1
end_object
statement "o" NULL
begin_object
statement "d" "blk"
