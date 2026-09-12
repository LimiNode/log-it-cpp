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

The default adapter measures a **prepared-message / direct-dispatch pipeline**.
It constructs a `LogRecord` inside the timed `adapter.log()` call, copies the
message into owned `std::string` storage, and calls the dispatcher directly. It
does not exercise the public `LOGIT_INFO(...)` macro path, argument-name
parsing, or `args_array` construction. The spdlog adapter receives a
`string_view` over the prepared message. This is a deliberate comparison of
these two call contracts, not a claim that they do identical work. A separate
public macro benchmark should be treated as a different scenario.
Compare implementations within the same mode and configuration; these numbers
are not a universal speed ranking.

The asynchronous measurement includes enqueue, worker wake-up/scheduling, and
sink time. Results are sensitive to queue capacity, overflow policy, worker
count, filesystem cache state, compiler, operating system, and hardware.

## Historical snapshot

The repository includes a **legacy** comparison snapshot from 2025-12-05 in
`bench/results/latency-2025-12-05-10k.csv`:

- workload: `LOGIT_BENCH_TOTAL=10000`, four producers, 200-byte messages;
- metrics: median (`p50`) latency in nanoseconds and throughput in messages per
  second;
- timestamp: 2025-12-05 03:18 UTC.

This fixture predates the current benchmark dependency metadata and does not
record the spdlog version, compiler/toolchain, or harness commit. Treat it as
legacy context rather than a reproducible current performance claim. New
published figures must include the dependency versions, toolchain, harness
commit, queue capacity, and flush semantics used for the run.

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
Both adapters receive the same explicit blocking queue capacity, configurable
through `LOGIT_BENCH_QUEUE_CAPACITY` (default: `max(8192, 2 * total)`). In
async mode `adapter.flush()` is a drain barrier: the spdlog adapter waits for a
worker-side flush marker, matching LogIt++'s executor drain. The measured
throughput interval therefore ends only after all recorded messages reached the
sink callback. A queue capacity of `0` is rejected because it would mean an
unlimited LogIt++ queue but a bounded spdlog queue and invalidate the
comparison.

The current CSV schema includes `queue_capacity`. Before appending, the harness
validates the existing `bench/results/latency.csv` header and fails with a
rename/remove instruction when it finds an older schema. Existing result files
are never silently rewritten or mixed with rows from a different schema.

The prepared-message/direct-dispatch pipeline and a true public macro benchmark that
calls `LOGIT_INFO(...)` are separate scenarios with different work contracts;
their results must not be presented as one number.

`logit_public_macro_bench` is the focused public-API smoke benchmark. It invokes
`LOGIT_INFO(...)` from multiple producer threads and therefore includes argument
name parsing, `args_array` construction, and dispatch. Its passthrough formatter
intentionally bypasses formatter work, so this is a public macro
record-construction + dispatch benchmark rather than a formatting benchmark.
Configure it
with `LOGIT_PUBLIC_BENCH_TOTAL` and `LOGIT_PUBLIC_BENCH_PRODUCERS`; its throughput
is reported separately from `latency.csv` and is intended for before/after
hot-path experiments on identical hardware.

`logit_hotpath_bench` and `logit_hotpath_bench_legacy` provide a controlled A/B
measurement for the registry read path. Both run the same prepared `LogRecord`
workload; the legacy target is compiled with `LOGIT_BENCH_LEGACY_REGISTRY` and
uses the pre-optimization mutex-plus-copy path, while the default target uses
the immutable snapshot. Compare their `ns_per_call` output on the same run and
toolchain. This is a measurement harness, not a supported production option.

The prepared-message path is also the first target for the logger hot-path
regression checks. Logger strategy lists are published as an immutable
copy-on-write snapshot, so a normal dispatch no longer takes the registry lock
or allocates a temporary vector. `enabled` and `single_mode` are atomic state,
which keeps concurrent configuration changes defined without changing the
existing formatter/backend execution mutex. That mutex remains intentional:
custom formatters and backends are not assumed to be safe for concurrent
invocation. Any future lock-elision experiment must advertise and test an
explicit concurrency contract rather than infer one from a benchmark sink.

The flush regression target uses an intentionally delayed asynchronous sink and
asserts that `flush()` does not return before every queued message has reached
that sink. `benchmark_validation_test` covers the comparative-protocol guardrails
(`queue_capacity=0` and legacy CSV schema rejection) without relying on packages
installed on the host.
