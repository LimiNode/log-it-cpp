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

The harness records end-to-end latency from the logging call until delivery to
the sink, together with aggregate throughput. It compares synchronous and
asynchronous modes, null and file sinks, producer counts, and message sizes.
Results are appended to `bench/results/latency.csv`; workload size can be
reduced with `LOGIT_BENCH_TOTAL` and `LOGIT_BENCH_WARMUP`.

## Interpreting results

The benchmark measures the complete path, not just formatter throughput. A
LogIt++ call may parse argument names, build `args_array`, and optionally
format values, while another logger may receive an already formatted string.
Compare implementations within the same mode and configuration; these numbers
are not a universal speed ranking.

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
