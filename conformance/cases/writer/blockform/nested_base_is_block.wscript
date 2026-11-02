anvil-writer-script 1
# a base is looked up among the top-level names, at any depth
dialect AML
statement_block "blk" NULL
begin_object
statement "f" NULL
int 1
end_object
statement "o" NULL
begin_object
statement "d" "blk"
