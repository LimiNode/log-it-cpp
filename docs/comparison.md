\page comparison Comparison with other C++ logging libraries

# LogIt++ compared with other C++ logging libraries

This page is a design and feature comparison, not a universal ranking. It
answers a practical question: **when does LogIt++ fit better than a smaller
formatted-string logger, a general event framework, or a diagnostic macro
utility?**

The comparison was checked on **2026-09-08** against these upstream releases:

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
| Macro-first instrumentation | Built in | Built in macros | Built in macros | Different model | Built in macros | Core feature |
| Structured records / attributes | Built in `LogRecord` and values | Different model: formatted message and formatter arguments | Different model: typed async log messages | Built in attribute/event model | Extension / message-centric | Diagnostic values, not a sink record |
| Capture source argument names | Built in | — | — | — | — | Built in |
| `printf`-style and stream APIs | Built in | Built in | Built in / fmt-oriented | Extension / different model | Stream and printf-like macros | Human-readable display |
| Conditional and rate-limited macros | Built in | Built in macro families | API-dependent | Filters and predicates | Built in severity/condition macros | Limited diagnostic helpers |
| Asynchronous queue | Built in | Built in | Core design | Sink-dependent | Not the primary model | — |
| Configurable overflow/backpressure | Built in (`Block`, `DropNewest`, `DropOldest`) | Built in overflow policies | Queue policies documented by Quill | Sink/configuration dependent | — | — |
| Rotating file sink | Built in | Built in | Built in handlers | Built in sink types | Basic file logging; rotation is a different concern | — |
| In-memory history and read-back | Built in for selected backends | Extension | Extension | Extension / custom sink | — | — |
| Live log subscriptions | Built in for selected backends | Extension | Extension | Extension | — | — |
| Persistent structured storage | Built in optional MDBX backend | Extension | Extension | Extension | — | — |
| OTLP and Prometheus integrations | Built in optional backends | Extension / adapter | Extension / adapter | Extension / adapter | Extension / adapter | — |
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
macros. Boost.Log emphasizes records, attributes, filters, and sinks rather
than a single macro facade. IceCream-Cpp is the closest comparison for
argument-name display, but it deliberately stops short of being a logger
backend framework.

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
delivery contract constant. Boost.Log can express asynchronous pipelines
through sinks and backends, but its configuration model is more general. glog
and IceCream-Cpp are not primary comparisons for this queue design.

## Structured data, storage, and telemetry

Choose LogIt++ when the log record itself is useful after the logging call:

- `MemoryLogger` supports snapshots, readers, and subscribers;
- file and MDBX backends can retain structured records for later access;
- OTLP exporters preserve selected structured attributes and context;
- Prometheus backends expose application and built-in metrics;
- MDC/NDC, tags, and argument values can flow through the same record model.

Boost.Log is the closest architectural comparison for an extensible record,
attribute, filter, and sink pipeline. spdlog and Quill are usually simpler to
adopt for formatted messages and high-throughput sinks. glog is intentionally
focused on application diagnostics and severity/check macros. These are
trade-offs in scope, not a claim that one design is universally better.

## Performance snapshot

The repository currently has a reproducible adapter for LogIt++ and spdlog,
not for every project in the feature table. The historical fixture below is
therefore a **LogIt++/spdlog pipeline snapshot**, not a ranking of all six
projects.

| Mode | Sink | LogIt++ p50 | LogIt++ throughput | spdlog p50 | spdlog throughput |
| --- | --- | ---: | ---: | ---: | ---: |
| Sync | Null | 119 ns | 2,127,704 msg/s | 86 ns | 5,803,783 msg/s |
| Sync | File | 130 ns | 1,035,690 msg/s | 87 ns | 1,593,987 msg/s |
| Async | Null | 20,916 ns | 1,846,272 msg/s | 1,248,779 ns | 1,303,573 msg/s |
| Async | File | 255,323 ns | 651,384 msg/s | 5,001,140 ns | 1,153,976 msg/s |

Snapshot conditions: Release build, four producers, 200-byte messages,
`LOGIT_BENCH_TOTAL=10000`, and the fixture recorded on 2025-12-05. The LogIt++
path may include argument-name extraction and structured value packing, while
the spdlog adapter receives a prepared string. Async values also include
enqueue, worker wake-up/scheduling, and sink work.

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

**Consider spdlog or Quill when:**

- the main requirement is a focused, fast formatted-message pipeline;
- their existing sinks, queue design, or ecosystem match the application;
- retaining structured diagnostic records is not necessary.

**Consider Boost.Log when:**

- a general attribute/filter/sink framework is more important than a compact
  macro facade;
- the project already depends on Boost and wants its extensibility model.

**Consider glog when:**

- Google-style severity, check, and diagnostic macros are the primary need;
- a narrower application logging model is preferable to storage and telemetry
  backends.

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
