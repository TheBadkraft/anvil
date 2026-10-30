# Differential fuzz harness

`fuzz.py` compares two implementations of the reader on generated and mutated inputs. Each side is any command
that reads the same input and prints the dump format of `conformance/FORMAT.md`. Today the sides are native's
`anvil-dump` and itself (and a deliberately altered copy, see `prove.sh`); when anvil.js exists, side `b` is its
dump command.

```
python3 conformance/fuzz/fuzz.py --a build/tools/anvil-dump --b "node path/to/dump.js" --cases 1000000 --seed 1 --jobs 8
```

- **Inputs.** Per case, from a seed (case `i` of seed `s` is always the same bytes): a grammar-aware generator
  (documents and value fragments: dialect headers, module attributes, every statement form, every value kind,
  numbers of every spelling, strings with escapes, blobs with tags, attributes, nested collections, `$name`
  references, comments; about half the cases stay inside the grammar, the rest inject errors), a byte-level
  mutator over the conformance corpus (flip, replace, insert grammar tokens, delete, duplicate, swap, truncate,
  splice between inputs), and the corpus itself.
- **Running.** In batches: `CMD --batch` with one path per line on stdin, output `@@ <path>` then that input's
  dump, per input. `--no-batch` runs `CMD [--fragment] <path>` per input for a command without batch mode. A
  `.frag` input is a value fragment.
- **Reporting.** The first difference (or `--max-diffs N`) is minimised by delta debugging over bytes, printed
  with the input, the two dumps' unified diff, and saved under `--out` as `diff-NNNN.<ext>` with both dumps. A
  minimised difference is a candidate corpus case: put it in `conformance/cases/` and run
  `make update-conformance` alongside the fix. A crash, a timeout or stderr output counts as a difference even
  when both sides fail alike.
- **Exit status.** 0 no difference, 1 a difference, 2 harness error.
- **Speed.** About 20,000 cases per second per job on native alone; a million cases is under a minute with
  `--jobs 4`.

`prove.sh` is the proof that the tooling can fail. In a scratch copy it alters one number rule of the parser
(`E` stops being an exponent marker), then requires `check-conformance` to go RED on that build, the fuzzer to
find nothing comparing native with itself, and the fuzzer to find and minimise the difference against the
altered build. Nothing is committed from the scratch copy. `make fuzz-prove` runs it.
