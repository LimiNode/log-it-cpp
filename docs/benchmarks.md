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

For a contract-matched asynchronous null-sink comparison, build
`logit_bench_async_contract` and run it with
an explicit producer count and message size when comparing the two libraries.
This target is structurally limited to `async=1`, `sink=null` and defaults to the separate
`bench/results/latency-async-contract.csv` output; `LOGIT_BENCH_OUTPUT` can
select another path when needed. In this target LogIt++ carries the full
message through its async queue, matching spdlog's async payload contract.

The harness records latency from the logging call until the adapter enters its
sink callback, together with aggregate throughput. This is a **sink-entry
latency** metric: for the file scenario the marker is recorded before the
`ofstream` write, so it is not a completed-write or durability measurement.
It compares synchronous and asynchronous modes, null and file sinks, producer
counts, and message sizes.
Results are appended to `bench/results/latency.csv`; workload size can be
reduced with `LOGIT_BENCH_TOTAL` and `LOGIT_BENCH_WARMUP`. Set
`LOGIT_BENCH_OUTPUT` to keep a separate workload contract from being appended
to an existing CSV.

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
The default LogIt++ async/null adapter transports only a slot marker because
the null sink does not consume the message. That path is intentionally not a
cross-library payload-cost comparison: spdlog transports the message through
its async queue. Use `logit_bench_async_contract` for a matched full-message
async contract.

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

The current CSV schema includes `queue_capacity` and `workload_contract`.
Before appending, the harness validates the selected output CSV header and
fails with a rename/remove instruction when it finds an older schema. Existing
result files are never silently rewritten or mixed with rows from a different
schema. Each CSV row retains its comparison provenance.

The prepared-message/direct-dispatch pipeline, the matched full-message async
target, and a true public macro benchmark that calls `LOGIT_INFO(...)` are
separate scenarios with different work contracts; their results must not be
presented as one number.

`logit_bench_async_contract` uses workload contract
`prepared-message/async-full-message`. For async/null it copies the same
message payload into the LogIt++ worker task that spdlog carries in its async
queue. Its CSV uses a separate output path (the default is
`bench/results/latency-async-contract.csv`, overridable with
`LOGIT_BENCH_OUTPUT`) and should be compared only with runs carrying the same
workload contract. This matches the queued payload contract, not every
instruction or allocation performed by the two libraries; LogIt++ still builds
and dispatches its `LogRecord` before the sink task is queued.

`logit_bench_async_payload_contract_test` is a functional regression test, not
a performance measurement. It configures the matched null-sink path with a
200-byte payload and verifies the exact payload observed by the worker-side
sink callback.

## Async pipeline research target

`logit_bench_pipeline_research` is a separate, benchmark-only instrumented
target for decomposing the matched `async/null/full-message` pipeline. Build it
with `LOGIT_BENCH_ENABLE=ON` and `LOGIT_BENCH_WITH_SPDLOG=ON`. The default
matrix uses 200-byte messages, 200,000 measured messages, 4,096 warmup
messages, producers `1,2,4,8`, queue capacities `1024,8192,65536,400000`,
five repeats, and alternates library order on each matched key point. A
rate-controlled run is selected with `LOGIT_BENCH_RESEARCH_MODE=rate`; its
default target rates are 100k, 250k, 500k, 750k, and 1M messages/second and it uses a
400,000-entry queue.

For example, a short exploratory run is:

```powershell
$env:LOGIT_BENCH_TOTAL = "200000"
$env:LOGIT_BENCH_WARMUP = "4096"
$env:LOGIT_BENCH_REPEATS = "5"
$env:LOGIT_BENCH_PRODUCERS = "1,2,4,8"
$env:LOGIT_BENCH_QUEUE_CAPACITIES = "1024,8192,65536,400000"
./build/logit_bench_pipeline_research
```

The target writes individual rows to `pipeline-research.csv` and JSONL
receipts to `pipeline-research.jsonl`, plus a median-per-key-point
`pipeline-research-aggregate.csv`. Paths can be overridden with
`LOGIT_BENCH_RESEARCH_CSV`, `LOGIT_BENCH_RESEARCH_JSONL`, and
`LOGIT_BENCH_RESEARCH_AGGREGATE`. Each row retains fixture metadata and the
deterministic `run_order`; the aggregate also retains producer/sink p50/p99,
producer phase, drain tail, total wall time, realized rate, schedule lag, and
both benchmark-outstanding summaries.

The metrics are deliberately library-neutral. `producer_p50/p99/p999_ns`
measure only the `adapter.log()` call; rate limiting is outside that timed
region. Both producer and sink recorders use the same benchmark call-start
timestamp. This gives the two metrics a common origin, but the research
measurement still includes the small recorder reservation and telemetry
overhead between that timestamp and entering `adapter.log()`. Therefore
`sink_p50/p99/p999_ns` is an **instrumented sink-entry metric** and must not be
compared as an identical absolute quantity with older matched-benchmark runs.
`producer_phase_ns` runs from the shared producer release barrier to the last
producer return, `drain_tail_ns` runs from the last producer return to the
final sink entry, and `total_wall_ns` ends after the drain barrier.
`throughput` is measured messages/second over that total interval.

In rate mode, `target_rate` is a pacing schedule, not a guaranteed external
open-loop arrival rate: a finite producer can receive its next ticket only
after its previous blocking `adapter.log()` returns. `realized_submission_rate`
is the issued count divided by the producer phase. `schedule_lag_p50/p99/max_ns`
records how far actual call start falls behind its scheduled start; growing lag
means the finite producer set cannot keep up with the target schedule.

`issued - sink_completed` is named **benchmark outstanding**. It is a common
benchmark counter, not a direct queue-depth measurement: it can include calls
in admission/backpressure and must not be presented as either library's
private queue size. `outstanding_high_water` and
`outstanding_at_producer_done` use this benchmark-issued semantics.

These results describe admission, backpressure, worker scheduling, and queue
backlog under the selected workload. They must not be reported as intrinsic
library latency or as a universal speed ranking. In particular, a lower
sink-entry p50 can coexist with lower throughput when a producer-side path
applies stronger admission pressure.

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

The checked-in [`benchmark-fixture-v2.json`](https://github.com/LimiNode/log-it-cpp/blob/main/bench/results/benchmark-fixture-v2.json)
defines the required metadata and workload contract. All publication-capable
benchmark binaries print a versioned `benchmark-fixture` metadata line for
each run, including source commit, compiler/version, toolchain, C++ standard,
platform, build type, architecture, machine identity, CPU model, queue
settings, latency completion, flush barrier, and workload contract. The commit defaults to
`LOGIT_BENCH_COMMIT` or `GITHUB_SHA`; machine identity and CPU model can be
provided through `LOGIT_BENCH_MACHINE_ID` and `LOGIT_BENCH_CPU_MODEL`.
For a comparable/publication run, set `LOGIT_BENCH_REQUIRE_COMPARABLE=1` and
provide all required metadata; public-macro runs may explicitly use
`not-applicable` queue settings. Smoke runs may leave unavailable values as
`unknown`. The fixture separates metadata that must be present from metadata
that must match between runs: `source_commit` is required provenance and is
expected to differ in before/after comparisons, while the fields listed in
`metadata_must_match` (compiler, toolchain, platform, build, hardware, queue,
completion semantics, and workload contract) must be identical.
`LOGIT_BENCH_REQUIRE_COMPARABLE=1`
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

## Producer path profiling

`logit_producer_profile` is a benchmark-only set of overlapping attribution
probes for the prepared LogIt++ producer path. It does not change library code
and does not compare against another logging library. The cases intentionally
overlap rather than form an additive decomposition:

- `string_copy_only` measures copying the prepared 200-byte payload;
- `logrecord_construct` measures construction of the same `LogRecord` shape
  used by `LogItAdapter`;
- `task_object_marker_only` measures local `std::function` construction and
  invocation without a payload;
- `task_object_full_message` adds the 200-byte payload ownership and capture;
- `taskexecutor_enqueue_prebuilt_noop` measures admission of a prebuilt
  no-op task; the source callable is constructed once, but each `add_task`
  call still includes by-value `std::function` copy/ownership transfer;
- `taskexecutor_enqueue_full_message` combines payload capture with queue
  admission;
- `taskexecutor_enqueue_noop` retains the original shared-state capture probe;
  it is intentionally not a pure queue-admission measurement;
- `logger_log_sync_null` measures dispatch of a prepared record to a synchronous
  counting sink;
- `logger_log_construct_and_invoke_full` measures dispatch plus full task
  construction and inline task-body invocation instead of enqueueing it;
  the inline body is part of this timed producer-side probe and is not present
  in the async producer timing region;
- `logger_log_async_full_prepared` measures prepared-record dispatch plus full
  message task admission;
- `prepared_record_plus_logger_async_full` adds per-call `LogRecord`
  construction to the previous asynchronous path.

The reported `ns_per_call` values are medians over independent repeats. Async
cases time only the producer loop; their drain barrier runs after the timed
region and verifies that every task was consumed. These are attribution probes,
not intrinsic latency claims: cases differ in allocation, ownership, allocator,
worker, and queue state. Do not sum the rows or treat subtraction between rows
as an exact component cost; differences are exploratory signals only. The
direct queue case is not a public API contract. The header reports
`queue_backend=mpsc_ring` or `queue_backend=mutex_deque`, depending on the
compile-time executor configuration. Configure the run with
`LOGIT_PRODUCER_PROFILE_TOTAL`, `LOGIT_PRODUCER_PROFILE_WARMUP`, and
`LOGIT_PRODUCER_PROFILE_REPEATS`.

For example:

```powershell
$env:LOGIT_PRODUCER_PROFILE_TOTAL = "200000"
$env:LOGIT_PRODUCER_PROFILE_WARMUP = "10000"
$env:LOGIT_PRODUCER_PROFILE_REPEATS = "5"
./build/logit_producer_profile
```

`logit_task_ownership_profile` is a narrower follow-up experiment for the
async ownership path. It compares local `std::function` construction/copy,
batched versus one-at-a-time `TaskExecutor` admission, and (when
`LOGIT_USE_MPSC_RING` is enabled) direct `MpscRingAny` publication for both a
`std::function<void()>` payload and a `uint64_t` control payload. The
one-at-a-time case includes worker completion and wakeup, so it is a round-trip
signal rather than a producer-only admission cost. Direct ring cases omit
`TaskExecutor` condition-variable notification and are not production API
measurements. Every queue case validates that all submitted tasks were
consumed; the output records the queue backend.

This target is an A/B attribution tool, not a replacement for the prepared
producer benchmark. Its rows still have different ownership and scheduling
contracts and must not be added or subtracted as exact component costs.
Configure it with `LOGIT_TASK_OWNERSHIP_TOTAL`,
`LOGIT_TASK_OWNERSHIP_WARMUP`, and `LOGIT_TASK_OWNERSHIP_REPEATS`. The
round-trip case defaults to at most 1,000 samples because each sample waits
for worker completion; use `LOGIT_TASK_OWNERSHIP_ROUNDTRIP_TOTAL` to override
that independent sample count.

The flush regression target uses an intentionally delayed asynchronous sink and
asserts that `flush()` does not return before every queued message has reached
that sink. The fixture records latency completion and the flush barrier as
separate semantics: the latency benchmark completes at sink entry, while the
flush barrier drains all prior work. `benchmark_validation_test` covers the comparative-protocol guardrails
(`queue_capacity=0` and legacy CSV schema rejection) without relying on packages
installed on the host.
