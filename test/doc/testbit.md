## TestBit

> `#include "testbit.h"`

## Runner

---
> `void run(const char *name, void (*fn)(void))`
- Runs one named test function.

---
> `void run_ex(const char *name, void (*setup)(void), void (*fn)(void), void (*teardown)(void))`
- Runs one named test with optional setup and teardown hooks.
- `setup` and `teardown` may be `NULL`.

## Assertions

---
> `void is_true(bool actual, const char *msg)`
- Passes when `actual` is `true`.

---
> `void is_false(bool actual, const char *msg)`
- Passes when `actual` is `false`.

---
> `void is_null(const void *ptr, const char *msg)`
- Passes when `ptr` is `NULL`.

---
> `void is_not_null(const void *ptr, const char *msg)`
- Passes when `ptr` is not `NULL`.

---
> `void is_equal_int(long long expected, long long actual, const char *msg)`
- Passes when `expected == actual`.
- Convention: pass expected first, actual second.

---
> `void is_equal_str(const char *expected, const char *actual, const char *msg)`
- Passes when both strings are equal.
- Also passes when both pointers are `NULL`.

---
> `void is_equal_str_ci(const char *expected, const char *actual, const char *msg)`
- Passes when strings are equal ignoring ASCII case.

---
> `void float_within(float value, float min, float max, const char *msg)`
- Passes when `value` is in the inclusive range `[min, max]`.

## Explicit Control

---
> `void fail(const char *msg)`
- Marks the current test as failed immediately.

---
> `void skip(const char *msg)`
- Marks the current test as skipped.

## Reporting

---
> `int report(void)`
- Prints summary output.
- Returns `0` when no tests failed.
- Returns `1` when one or more tests failed.

## Notes

- Keep assertion message prefixes aligned to your test IDs.
- Keep intentional failure tests separate from pass-only CI runs.

## Behavior Demo

```c
static void tbn01_fail(void) {
  TestBit.fail("TBN01: explicit fail should fail test");
}

static void tbn02_expected_assert_failures(void) {
  TestBit.is_true(0, "TBN02: intentional fail");
  TestBit.is_equal_int(1, 2, "TBN02: intentional fail");
  TestBit.is_equal_str("a", "b", "TBN02: intentional fail");
}

static void tbn03_skip(void) {
  TestBit.skip("TBN03: intentional skip");
}

int main(void) {
  TestBit.run("TBN01_fail", tbn01_fail);
  TestBit.run("TBN02_expected_assert_failures", tbn02_expected_assert_failures);
  TestBit.run("TBN03_skip", tbn03_skip);
  return TestBit.report();
}
```