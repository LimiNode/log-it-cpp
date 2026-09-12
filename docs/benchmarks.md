\page benchmarks Performance and benchmarks

# Performance and benchmarks

The benchmark harness lives under `bench/` and is disabled by default. Enable
it with `LOGIT_BENCH_ENABLE=ON`; add `LOGIT_BENCH_WITH_SPDLOG=ON` for the
optional spdlog comparison binaries.

```bash
cmake -S . -B build -DLOGIT_BENCH_ENABLE=ON
cmake --build build --target logit_bench
./build/bench/logit_bench
```

The harness records latency from the logging call until the adapter enters its
sink callback, together with aggregate throughput. This is a **sink-entry
latency** metric: for the file scenario the marker is recorded before the
`ofstream` write, so it is not a completed-write or durability measurement.
It compares synchronous and asynchronous modes, null and file sinks, producer
counts, and message sizes.
Results are appended to `bench/results/latency.csv`; workload size can be
reduced with `LOGIT_BENCH_TOTAL` and `LOGIT_BENCH_WARMUP`.

## Interpreting results

The default adapter measures a **prepared-record / dispatch pipeline**. It
constructs a `LogRecord` whose message is already prepared and calls the
dispatcher directly; it does not exercise the public `LOGIT_INFO(...)` macro
path, argument-name parsing, or `args_array` construction. A separate public
macro benchmark should be treated as a different scenario rather than mixed
into this comparison. The spdlog adapter likewise receives an already prepared
string. Compare implementations within the same mode and configuration; these
numbers are not a universal speed ranking.

The asynchronous measurement includes enqueue, worker wake-up/scheduling, and
sink time. Results are sensitive to queue capacity, overflow policy, worker
count, filesystem cache state, compiler, operating system, and hardware.

## Historical snapshot

The repository includes a comparison snapshot from 2025-12-05 in
`bench/results/latency-2025-12-05-10k.csv`:

- workload: `LOGIT_BENCH_TOTAL=10000`, four producers, 200-byte messages;
- metrics: median (`p50`) latency in nanoseconds and throughput in messages per
  second;
- timestamp: 2025-12-05 03:18 UTC.

This is a historical, reproducible fixture rather than a current performance
claim. Re-run the harness on the target hardware before making deployment or
library-selection decisions.

## Harness details

`bench/LatencyRecorder.hpp` preallocates slots and tracks
`Token {slot, t0_ns, active}` and `Summary {p50, p99, p999}`. It exposes
`recorded()`, `wait_for_all()`, and `finalize()` for end-to-end timing across
producers and consumers. The LogIt adapter stores the benchmark slot in
`LogRecord::line`; sinks call `LatencyRecorder::complete_slot()` when they
observe a non-negative line number.

The current matrix covers 1, 4, 16, and 32 producers. CI intentionally uses a
short Release smoke workload (`LOGIT_BENCH_TOTAL=20000`) for predictable run
time. Larger publication runs (for example, one million messages plus warmup)
belong on fixed or self-hosted hardware, where the results can be reproduced.

The prepared-record/dispatch pipeline and a true public macro benchmark that
calls `LOGIT_INFO(...)` are separate scenarios with different work contracts;
their results must not be presented as one number.

The prepared-record path is also the first target for the logger hot-path
regression checks. Logger strategy lists are published as an immutable
copy-on-write snapshot, so a normal dispatch no longer takes the registry lock
or allocates a temporary vector. `enabled` and `single_mode` are atomic state,
which keeps concurrent configuration changes defined without changing the
existing formatter/backend execution mutex. That mutex remains intentional:
custom formatters and backends are not assumed to be safe for concurrent
invocation. Any future lock-elision experiment must advertise and test an
explicit concurrency contract rather than infer one from a benchmark sink.
