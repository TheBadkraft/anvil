anvil-writer-script 1
# once an object element is written, later elements go on their own lines
dialect AML
statement "a" NULL
begin_array
int 1
int 2
begin_object
statement "x" NULL
int 1
end_object
int 3
string "y"
end_array
finish
