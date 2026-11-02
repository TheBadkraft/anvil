anvil-writer-script 1
# the reader rejects a block as a base (4052)
dialect AML
statement_block "blk" NULL
begin_object
statement "f" NULL
int 1
end_object
statement "d" "blk"
