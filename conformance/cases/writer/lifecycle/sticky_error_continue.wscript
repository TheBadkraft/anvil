anvil-writer-script 1
# the first error is sticky; later calls fail with it
dialect AML
option on-error continue
statement "a" NULL
statement "b" NULL
int 1
bool true
get_error
finish
data
