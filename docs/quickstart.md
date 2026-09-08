\page quickstart Quick start and documentation map

# Quick start

LogIt++ is a header-only, macro-first C++ logging library. The core and most
native backends support C++11. OTLP, the Prometheus HTTP server, and MDBX use
C++17 through their CMake feature options.

## Minimal application

```cpp
#include <logit.hpp>

int main() {
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_INFO("service started", 42);
    LOGIT_WAIT();
}
```

Installation choices and complete CMake examples are collected in the
[`Installation guide`](installation.html). Optional features and their
dependency requirements are described in the [`Backend matrix`](backends.html).

## Documentation map

- **Installation and CMake** — [`Installation guide`](installation.html),
  including vendored, submodule, installed-package, and package-manager flows.
- **Macros and formatting** — use the public `<logit.hpp>` entry point; the
  macro and pattern references are available from the API index.
- **Backends** — [`Backend matrix`](backends.html),
  [`OTLP exporters`](otlp_http_logger.html), and
  [`Prometheus`](prometheus_logger.html).
- **Stored logs and context** — `MemoryLogger`, `MdbxLogger`, `ILogReader`,
  `ILogSubscriber`, and MDC/NDC are documented in the generated class reference
  and examples.
- **Asynchronous delivery** — [`TaskExecutor`](task_executor.html) and
  [`Queue back-pressure`](backpressure.html).
- **Performance** — [`Benchmarks`](benchmarks.html), including methodology and
  the historical snapshot disclaimer.
- **Choosing a logging library** — [`Comparison with other C++ logging libraries`](comparison.html),
  including trade-offs and the scope of the comparison.
- **Examples** — browse the `examples/` directory in the source repository;
  each optional example states the feature macro it requires.

## Public API boundary

Application code should include the public umbrella headers and use the
`LOGIT_*` macros. The `logit::detail` namespace and implementation headers are
internal and carry no source or API compatibility guarantee.
