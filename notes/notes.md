1. letting `document` API allocate source via `Source.create` so, `from_buffer` and `from_file` expect `out_src` to be allocated
2. added a `length` member to `anvl_source` object (we will add supporting vtable interface functions later if needed)
3. added several invariants to `SRC04` test cases
4. anywhere we are using `size_t` (unsigned long) we should be using Sigma's type `usize`. the `u` is immediate reminder _`unsigned`_
5. `source_from_file` will cache the file's dialect hint from file extension in the source struct