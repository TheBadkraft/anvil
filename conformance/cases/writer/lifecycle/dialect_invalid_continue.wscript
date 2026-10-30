anvil-writer-script 1
# a NULL writer: every call fails with INVALID_ARGUMENT
option on-error continue
dialect 7
get_error
statement "a" NULL
int 1
finish
data
