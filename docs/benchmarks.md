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
published figures must include the fixture metadata, dependency versions,
toolchain, harness commit, build and hardware identity, queue settings, and
separate latency-completion and flush-barrier semantics used for the run.

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

`logit_exec_mx_bench` and `logit_exec_mx_bench_concurrent` are a guarded
lock-elision experiment. They use the same prepared `LogRecord` and a small
thread-safe counting backend/formatter pair; the first target keeps the default
serialized path, while the second explicitly opts into the concurrency
capabilities. Both report 1, 4, 16, and 32 producer runs. These binaries are
research tools, not a recommendation to opt in arbitrary backends. A backend
or formatter must satisfy the complete lifecycle contract in
[`ADR 0006`](adr/0006-concurrent-dispatch-capability.md) before returning the
capability flag.

`logit_public_macro_bench` and `logit_public_macro_formatted_bench` are focused
public-API smoke benchmarks. Both invoke `LOGIT_INFO(...)` from multiple
producer threads and therefore include argument-name parsing, `args_array`
construction, and dispatch. Producers are created before the timed interval,
wait on a ready barrier, and are released together; thread creation and the
startup skew are not part of the producer-scaling measurement. The optional
`LOGIT_PUBLIC_BENCH_WARMUP` run is executed before the measured workload.
The first target uses a passthrough formatter and intentionally bypasses
formatter work; the second uses `SimpleLogFormatter` and includes the formatter
path. Their throughputs are separate scenarios and must not be presented as one
number. Configure either with `LOGIT_PUBLIC_BENCH_TOTAL`,
`LOGIT_PUBLIC_BENCH_PRODUCERS`, and `LOGIT_PUBLIC_BENCH_WARMUP`; results are
reported separately from `latency.csv` and are intended for before/after
experiments on identical hardware. Both targets print the same fixture metadata
line as `logit_bench`, using `not-applicable` for queue settings and explicit
`backend-count` / `logger-wait` completion semantics.

The checked-in [`benchmark-fixture-v1.json`](https://github.com/LimiNode/log-it-cpp/blob/main/bench/results/benchmark-fixture-v1.json)
defines the required metadata and workload contract. All publication-capable
benchmark binaries print a versioned `benchmark-fixture` metadata line for
each run, including source commit, compiler/version, toolchain, C++ standard,
platform, build type, architecture, machine identity, CPU model, queue
settings, latency completion, and flush barrier. The commit defaults to
`LOGIT_BENCH_COMMIT` or `GITHUB_SHA`; machine identity and CPU model can be
provided through `LOGIT_BENCH_MACHINE_ID` and `LOGIT_BENCH_CPU_MODEL`.
For a comparable/publication run, set `LOGIT_BENCH_REQUIRE_COMPARABLE=1` and
provide all required metadata; public-macro runs may explicitly use
`not-applicable` queue settings. Smoke runs may leave unavailable values as
`unknown`. The fixture separates metadata that must be present from metadata
that must match between runs: `source_commit` is required provenance and is
expected to differ in before/after comparisons, while the fields listed in
`metadata_must_match` (compiler, toolchain, platform, build, hardware, queue,
and completion semantics) must be identical. `LOGIT_BENCH_REQUIRE_COMPARABLE=1`
checks metadata completeness and known values; it does not enforce the
canonical fixture workload values such as total messages, warmup, or producer
matrix.

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
that sink. The fixture records latency completion and the flush barrier as
separate semantics: the latency benchmark completes at sink entry, while the
flush barrier drains all prior work. `benchmark_validation_test` covers the comparative-protocol guardrails
(`queue_capacity=0` and legacy CSV schema rejection) without relying on packages
installed on the host.
