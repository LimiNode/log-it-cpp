\page comparison Comparison with other C++ logging libraries

# LogIt++ compared with other C++ logging libraries

This page is a design and feature comparison, not a universal ranking. It
answers a practical question: **when does LogIt++ fit better than a smaller
formatted-string logger, a general event framework, or a diagnostic macro
utility?**

The comparison was checked on **2026-09-10** against these upstream releases:

| Project | Release checked | Primary documentation |
| --- | --- | --- |
| LogIt++ | `1.0.2-dev` (`main`) | [project documentation](https://liminode.github.io/log-it-cpp/) |
| spdlog | [`v1.17.0`](https://github.com/gabime/spdlog/releases/tag/v1.17.0) | [README](https://github.com/gabime/spdlog), [async logging](https://github.com/gabime/spdlog/wiki/Asynchronous-logging) |
| Quill | [`v13.0.0`](https://github.com/odygrd/quill/releases/tag/v13.0.0) | [project documentation](https://quillcpp.readthedocs.io/) |
| Boost.Log | [Boost `1.92.0`](https://www.boost.org/users/history/version_1_92_0.html) | [Boost.Log documentation](https://www.boost.org/doc/libs/1_92_0/libs/log/doc/html/) |
| glog | [`v0.7.1`](https://github.com/google/glog/releases/tag/v0.7.1) | [glog documentation](https://google.github.io/glog/) |
| IceCream-Cpp | [`v1.0.0`](https://github.com/renatoGarcia/icecream-cpp/releases/tag/v1.0.0) | [project README](https://github.com/renatoGarcia/icecream-cpp) |

Upstream projects evolve independently. Re-check the release and linked
documentation before using this page as a procurement or performance claim.

## What LogIt++ optimizes for

LogIt++ is designed around a diagnostic record rather than only a finished
string. A normal macro call can keep values and argument names available to
formatters, memory/storage backends, callbacks, and telemetry exporters. The
same macro-first surface also covers conditional logging, throttling, scope
timers, tags, MDC/NDC context, and targeted logger routing.

This makes LogIt++ a good fit when logging is part of an application's
diagnostic data flow. It is a less obvious fit when the only requirement is the
smallest possible formatted-string call on a hot path.

## Feature comparison

The table describes the primary distribution and documented API of each
project, not everything that can be built through third-party adapters. `Built
in` means the checked release documents the capability directly. `Different
model` means that a similar outcome exists, but the abstraction is not
equivalent. `Extension` means that an adapter, custom sink, or application
code is normally required. A dash means that the capability is not a primary
goal of the checked project, not that it is impossible to implement.

| Capability | LogIt++ | spdlog | Quill | Boost.Log | glog | IceCream-Cpp |
| --- | --- | --- | --- | --- | --- | --- |
| Macro-first instrumentation | Built in | Built-in severity macros | Built in macros | Built-in macros over a record/stream model | Built in macros | Core feature |
| Structured records / attributes | Built in `LogRecord` and values | Formatted messages plus MDC (synchronous logging only); different record model | Built-in named-value logging, JSON output, tags, and MDC; different record model | Built in attribute/event model | Extension / message-centric | Diagnostic values, not a sink record |
| Capture source argument names | Built in | — | Built in via `LOGV_*` | — | — | Core feature |
| `{fmt}`-style formatting | Built in when `LOGIT_WITH_FMT=ON` | Built in | Built in | Extension / pre-formatting | Extension / pre-formatting | — |
| `printf`-style logging | Built in | Extension / pre-formatting | Extension / pre-formatting | Extension / pre-formatting | Low-level `RAW_LOG` only (stderr-only) | — |
| Stream-style logging | Built in | Extension / pre-formatting | Extension / pre-formatting | Built in | Built in | Human-readable diagnostic output |
| Conditional logging helpers | Built in | No dedicated helper; use an application condition | No dedicated helper; use an application condition | Filters and predicates; no equivalent call-site macro | Built in via `LOG_IF` | Configuration helpers, not a logging framework |
| Rate-limited logging helpers | Built in | No dedicated macro family | Built in via `LOG_*_LIMIT` and `LOGV_*_LIMIT` | Extension / custom filter | Built in via `LOG_EVERY_N`, `LOG_FIRST_N`, and related macros | — |
| Asynchronous queue | Built in | Built in | Core design | Built in via asynchronous sink frontends | Not the primary model | — |
| Configurable overflow/backpressure | Built in (`Block`, `DropNewest`, `DropOldest`) | Built in overflow policies | Built-in bounded/unbounded and blocking/dropping queue modes | Built in via bounded async sink queue strategies (`drop_on_overflow`, `block_on_overflow`) | — | — |
| Rotating file sink | Built in | Built in | Built in via `RotatingFileSink` | Built in sink types | Built-in file rollover/cleanup; different model | — |
| In-memory history and read-back | Built in for selected backends | Built-in backtrace buffer; no equivalent read-back API | Built-in backtrace logging; no equivalent read-back API | Extension / custom sink | — | — |
| Live log subscriptions | Built in for selected backends | Built-in callback sink; different model | Extension / custom sink | Extension / custom sink | — | — |
| Embedded queryable structured storage | Built in via optional MDBX backend | Extension | Extension | Extension | — | — |
| OTLP / OpenTelemetry log export | Built in optional backend | Extension / adapter | Extension / custom sink | Extension / adapter | Extension / adapter | — |
| Prometheus metrics | Built in optional backends | Extension / adapter | Built in via `PrometheusSink` | Extension / adapter | Extension / adapter | — |
| Header-only integration | Built in | Supported mode | CMake/library integration | Compiled Boost component | CMake/library integration | Header-oriented utility |

The word “built in” is intentionally narrow. For example, all of these
projects can be connected to other sinks or telemetry systems by application
code; that does not make the integration part of the primary distribution.

## Developer ergonomics

LogIt++ deliberately provides a large, regular macro surface:

```cpp
#include <logit.hpp>

LOGIT_INFO("Connected", host, port);
LOGIT_WARN_ONCE("configuration missing");
LOGIT_INFO_EVERY_N(100, "processed", count);
LOGIT_ERROR_THROTTLE(1000, "connection still unavailable");
LOGIT_SCOPE_INFO("load_database");
LOGIT_INFO_TAG(({{"order_id", order_id}, {"symbol", symbol}}), "order sent");
```

The first call can retain the names and values of `host` and `port`, depending
on the selected macro and build options. That is different from passing one
already formatted string to a sink. It is convenient for diagnostics, but it
also does more work than a minimal formatted-string call.

spdlog, Quill, and glog also provide useful macro families, but their primary
abstractions remain logger calls, formatted messages, or severity/check
macros. Boost.Log provides logging macros over its record, attribute, filter,
and sink model rather than a single broad macro facade. IceCream-Cpp is the
closest comparison as a dedicated introspection utility; Quill also captures
source variable names through its `LOGV_*` logging macros.

## Why IceCream-Cpp is a different comparison

IceCream-Cpp is a diagnostic printing/introspection utility. It is valuable
when a developer wants a short call that displays an expression and its value:

```cpp
// IceCream-Cpp-style diagnostic output
IC(x, user_id);
```

It does not aim to provide LogIt++-style sinks, asynchronous queues, file
rotation, retention/read-back, subscriptions, OTLP, or Prometheus backends.
The meaningful comparison is therefore the ergonomics of source-level
diagnostics and argument-name capture, not “which logger has more sinks”.
IceCream-Cpp can also be used alongside a real logger.

## Async delivery and queue semantics

LogIt++ offers synchronous and asynchronous modes, global or dedicated
executors, queue capacity, and explicit overflow policies. The deque and MPSC
implementations intentionally document a semantic difference: in MPSC mode
`DropOldest` rejects the incoming task to preserve the order of already
accepted work.

spdlog and Quill are closer comparisons for asynchronous throughput and queue
configuration. Their options, worker topology, and overflow semantics are
not interchangeable with LogIt++, so a benchmark must hold the workload and
delivery contract constant. Boost.Log provides asynchronous sink frontends,
including bounded FIFO queues with drop-on-overflow and block-on-overflow
strategies, through a more general sink configuration model. glog and
IceCream-Cpp are not primary comparisons for this queue design.

## Structured data, storage, and telemetry

Choose LogIt++ when the log record itself is useful after the logging call:

- `MemoryLogger` supports snapshots, readers, and subscribers;
- `FileLogger` supports persisted-file enumeration and text read-back;
- `MdbxLogger` retains structured records for later querying;
- OTLP exporters preserve selected structured attributes and context;
- Prometheus backends expose application and built-in metrics;
- MDC/NDC, tags, and argument values can flow through the same record model.

Boost.Log is the closest architectural comparison for an extensible record,
attribute, filter, and sink pipeline. spdlog is usually simpler to adopt for
formatted messages and high-throughput sinks. Quill combines that performance
focus with named-value macros, JSON output, tags, and MDC through its own
record model. glog is focused on application diagnostics and severity/check
macros. These are trade-offs in scope, not a claim that one design is
universally better.

## Performance snapshot

The repository currently has a reproducible **prepared-message/direct-dispatch
pipeline** adapter for LogIt++ and spdlog, not for every project in the feature
table. The historical fixture below is therefore a **legacy LogIt++/spdlog
pipeline snapshot**, not a ranking of all six projects. It does not measure the
full public `LOGIT_INFO(...)` macro path; in particular, it omits argument-name
parsing and `args_array` construction. The LogIt++ adapter constructs its
`LogRecord` and copies the message into `std::string` during the timed call,
while spdlog receives a prepared `string_view`; those call contracts are
intentionally documented rather than presented as identical.

| Mode | Sink | LogIt++ p50 | LogIt++ throughput | spdlog p50 | spdlog throughput |
| --- | --- | ---: | ---: | ---: | ---: |
| Sync | Null | 119 ns | 2,127,704 msg/s | 86 ns | 5,803,783 msg/s |
| Sync | File | 130 ns | 1,035,690 msg/s | 87 ns | 1,593,987 msg/s |
| Async | Null | 20,916 ns | 1,846,272 msg/s | 1,248,779 ns | 1,303,573 msg/s |
| Async | File | 255,323 ns | 651,384 msg/s | 5,001,140 ns | 1,153,976 msg/s |

Snapshot conditions: Release build, four producers, 200-byte messages,
`LOGIT_BENCH_TOTAL=10000`, and the fixture recorded on 2025-12-05. The timed
LogIt++ adapter constructs a `LogRecord` and owns a message copy, while the
spdlog adapter receives a prepared `string_view`. Async values also include
enqueue, worker wake-up/scheduling, and sink work.

The CSV is a legacy fixture: its spdlog version, compiler/toolchain, harness
commit, queue capacity, and async drain protocol were not recorded. Do not use
it as a current numeric comparison; regenerate a versioned fixture after the
benchmark protocol changes.

See [`docs/benchmarks.md`](benchmarks.html) for the methodology and
[`bench/results/latency-2025-12-05-10k.csv`](https://github.com/LimiNode/log-it-cpp/blob/main/bench/results/latency-2025-12-05-10k.csv)
for the complete fixture. The harness does not currently measure allocations
per message, binary size, compile time, or comparable Quill/Boost.Log/glog
scenarios; those numbers are intentionally not invented here.

## Trade-offs

Choose LogIt++ with its trade-offs in mind:

- the macro surface is intentionally broad;
- argument-name capture and structured records can cost more than a minimal
  preformatted-string call;
- optional OTLP, Prometheus HTTP server, and MDBX integrations use C++17;
- optional dependencies and installed-package composition need the documented
  CMake setup;
- the public API uses aggregate-first umbrella headers rather than promising
  that every leaf header is independently includable;
- the project favors diagnostic richness, storage, and telemetry integration
  over the smallest possible logging-call overhead.

## Which library should I choose?

**Choose LogIt++ when:**

- one macro-first API should cover console, file, memory, storage, and
  telemetry backends;
- structured values, argument-name capture, tags, MDC/NDC, or read-back are
  important;
- queue capacity, overflow policy, dedicated executors, and per-backend
  delivery behavior matter;
- a header-only C++11 core with optional C++17 integrations fits the project.

**Consider spdlog when:**

- the main requirement is a focused, fast formatted-message pipeline;
- its existing sinks, async queue design, or ecosystem match the application;
- retaining structured diagnostic records is not necessary.

**Consider Quill when:**

- an asynchronous, low-latency logging pipeline is the main requirement;
- its named-value macros, JSON output, tags, MDC, and Prometheus metrics fit
  the structured-data requirements;
- LogIt++-style persistent structured storage, programmatic read-back, and
  live subscriptions are not required.

**Consider Boost.Log when:**

- a general attribute/filter/sink framework is more important than a compact
  macro facade;
- the project already depends on Boost and wants its extensibility model.

**Consider glog when:**

- Google-style severity, check, and diagnostic macros are the primary need;
- a narrower application logging model is preferable to storage and telemetry
  backends.

The [upstream `google/glog` repository](https://github.com/google/glog) was
archived and made read-only as of the 2026-09-10 check; account for that
lifecycle status before adopting it as a new dependency.

**Consider IceCream-Cpp when:**

- the goal is lightweight source-expression introspection during development;
- a logger/sink/retention framework is not required. It can complement, rather
  than replace, one of the logging libraries above.

## Sources and reproducibility

The version and source links at the top of this page are the authoritative
starting points for the feature comparison. For LogIt++ behavior, prefer the
linked project guides, generated API reference, examples, and benchmark
fixture in this repository. Re-run benchmarks on the target compiler,
operating system, hardware, sink, queue configuration, and workload before
making a performance decision.
