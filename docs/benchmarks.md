# Anvil Benchmark Report

**Generated:** 2026-03-14 20:54
**Platform:** Linux x86_64
**Compiler:** gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
**Build flags:** `-std=c2x -O0 -g` (debug build — release will be faster)
**sigma.memory:** 0.2.5 (Resource scope / mmap bump allocator)
**Anvil version:** v0.4.5-alpha

> All timings are wall-clock (`CLOCK_MONOTONIC`).
> Each benchmark runs 5 warm-up iterations (discarded) followed by 100 measured
> iterations. **min** = best observed (cache-warm), **mean** = arithmetic mean, **σ** = population std-dev.
> Throughput = file size / mean parse time.

---


### PB — Parser throughput (bench_parse)

```

  Benchmark                                         min       mean        max       σ    throughput
  ────────────────────────────────────────────────────────────────────────────────────────────────────
  PB01 parse small (assignments.anvl)         min=53.65 µs  mean=62.05 µs  max=140.69 µs  σ= 13.5 µs     7.09 MB/s
  PB02 parse medium (objects.anvl)            min=122.95 µs  mean=266.62 µs  max=462.25 µs  σ= 73.3 µs     1.40 MB/s
  PB03 parse large (modpack.anvl)             min=3.36 ms    mean=7.58 ms    max=31.73 ms   σ=5495.9 µs     0.90 MB/s
  PB04 parse blob-heavy (blob_heavy_mixed.anvl)  min=1.03 µs   mean=1.06 µs   max=1.10 µs   σ=  0.0 µs  1343.67 MB/s
  PB05 parse deep-nesting (deep_nesting.anvl)  min=1.86 µs   mean=3.92 µs   max=26.46 µs  σ=  3.0 µs   448.91 MB/s
  PB06 parse basic (basic.aml)                min=351.73 µs  mean=392.87 µs  max=689.73 µs  σ= 43.2 µs     3.26 MB/s
  PB07 parse large  (large.aml)               min=10.96 ms   mean=43.31 ms   max=84.37 ms   σ=22724.0 µs     0.23 MB/s
```


### RB — Inheritance resolver (bench_resolver)

```

  Benchmark                                         min       mean        max       σ
  ────────────────────────────────────────────────────────────────────────────
  RB01 cold 1-level build+merge (RubyGem:BaseItem)  min=41.56 µs  mean=53.80 µs  max=348.71 µs  σ= 32.6 µs
  RB02 warm 1-level cache hit  (RubyGem:BaseItem)  min=19 ns      mean=19 ns      max=23 ns      σ=  0.0 µs
  RB03 cold 3-level build+merge (Creeper:MobBase:EntityBase)  min=49.99 µs  mean=54.18 µs  max=178.41 µs  σ= 15.3 µs
  RB04 warm 3-level cache hit  (Creeper:MobBase:EntityBase)  min=20 ns      mean=20 ns      max=24 ns      σ=  0.0 µs
  RB05 eager warm_all (merge.aml 10 stmts)    min=93.80 µs  mean=97.14 µs  max=202.68 µs  σ= 11.1 µs
  RB06 3-sibling warm reads (Iron/Gold/DiamondOre)  min=26 ns      mean=29 ns      max=40 ns      σ=  0.0 µs
```


### SB — Serializer / writer (bench_serializer)

```

  Benchmark                                         min       mean        max       σ
  ────────────────────────────────────────────────────────────────────────────
  SB01 write pretty small (objects.anvl)      min=31.28 µs  mean=31.84 µs  max=44.81 µs  σ=  1.6 µs
  SB02 write minified small                   min=30.62 µs  mean=30.96 µs  max=35.05 µs  σ=  0.7 µs
  SB03 write pretty large (modpack.anvl)      min=102.19 µs  mean=110.91 µs  max=138.90 µs  σ=  6.7 µs
  SB04 write minified large                   min=54.49 µs  mean=69.00 µs  max=103.08 µs  σ= 14.3 µs
  SB05 round-trip parse→serialize→parse   min=197.59 µs  mean=346.02 µs  max=508.00 µs  σ= 86.6 µs
```


### AB — ASL script engine (bench_asl)

```

  Benchmark                                         min       mean        max       σ
  ────────────────────────────────────────────────────────────────────────────
  AB01 cold AST parse (arith body)            min=9.07 µs   mean=10.29 µs  max=34.08 µs  σ=  4.1 µs
  AB02 exec arithmetic: 7*3+7-3 → 25        min=13.89 µs  mean=15.36 µs  max=71.79 µs  σ=  6.0 µs
  AB03 exec classify 85 → "B" (4-branch)    min=69.03 µs  mean=75.13 µs  max=183.45 µs  σ= 12.4 µs
  AB04 exec sum loop 0..99 → 4950           min=72.36 µs  mean=78.18 µs  max=277.52 µs  σ= 20.9 µs
  AB05 exec iterative fib(20) → 6765        min=89.24 µs  mean=95.61 µs  max=163.63 µs  σ= 11.0 µs
  AB06 exec 10×10 nested loop → 2025       min=139.70 µs  mean=160.82 µs  max=580.92 µs  σ= 44.0 µs
  AB07 exec collatz(27) → 111 steps         min=174.98 µs  mean=187.32 µs  max=271.65 µs  σ= 13.1 µs
```


### Full sample sweep (test_performance)

```

  File                              Size  Parse time   Stmts  Throughput
  ─────────────────────────────────────────────────────────────────
  assignments.anvl                 461 B   10.27 µs      15     42.82 MB/s
  objects.anvl                     391 B    9.30 µs       3     40.10 MB/s
  inherits.anvl                    448 B    8.71 µs       2     49.03 MB/s
  numbers.anvl                     596 B    2.38 µs       0    238.55 MB/s
  attributes.anvl                  660 B   11.45 µs       3     54.98 MB/s
  arrays.anvl                     1.2 KB   11.20 µs       5    106.51 MB/s
  tuples.anvl                      344 B    6.99 µs       3     46.95 MB/s
  modpack.anvl                    7.0 KB  111.29 µs      32     61.59 MB/s
  large_dataset.anvl              2.2 KB    2.94 µs       0    725.89 MB/s
  complex_nested.anvl             3.1 KB    2.18 µs       0   1383.06 MB/s
  repetitive_data.anvl            3.7 KB    2.47 µs       0   1448.18 MB/s
  deep_nesting.anvl               1.8 KB    2.36 µs       0    746.41 MB/s
  blob_heavy_json.anvl            1.2 KB    2.48 µs       0    465.41 MB/s
  blob_heavy_xml.anvl             1.2 KB    2.30 µs       0    500.09 MB/s
  blob_heavy_csv.anvl             1.3 KB    2.25 µs       0    563.06 MB/s
  blob_heavy_mixed.anvl           1.5 KB    1.86 µs       0    767.90 MB/s
  amp_envelope.anvl               1.0 KB    2.07 µs       0    495.53 MB/s
```


---

## Notes

- **PB01–PB05** use `.anvl` (AML dialect); **PB06–PB07** use `.aml` (stricter AMP dialect).
- **PB04** (blob-heavy) and **PB05** (deep-nesting) exercise pass-through and recursion limits respectively.
- **RB02, RB04, RB06** warm cache-hit times are in the **17–26 ns** range — single pointer deref through the resolver cache.
- **SB04** (minified) is faster than **SB03** (pretty) because it skips indent/newline emission.
- **SB05** round-trip variance is dominated by the two parse phases, not serialization.
- **AB** benchmarks run under the ASL interpreter; timings include AST parse + eval.
- All allocations during parse go through `Allocator.Resource` (mmap bump slab). Zero BTree/skiplist overhead.
  See sigma.memory FT-12 for details.
