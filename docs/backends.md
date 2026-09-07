\page backends Backend matrix

# Backend matrix

Choose a backend by delivery model, platform, and dependency requirements.
Every backend implements `ILogger`; stored-log backends may additionally
implement `ILogReader` and `ILogSubscriber`.

All LogIt++ builds require **TimeShield 1.0.6 or newer**. The dependency column
below lists only feature-specific dependencies.

| Backend | Enablement | Standard | Feature-specific dependency | Platform and packaging notes |
|---|---|---:|---|---|
| `ConsoleLogger` | built in | C++11 | None | Native and documented Emscripten behavior |
| `FileLogger` / `UniqueFileLogger` | built in | C++11 | zlib or zstd only when compression is enabled | Native and documented Emscripten stubs |
| `MemoryLogger` | built in | C++11 | None | Supports snapshots and subscriber callbacks |
| `CrashLogger` | built in | C++11 | Platform crash facilities | Synchronous crash path |
| `WindowsDebugLogger` | built in | C++11 | Windows API | `OutputDebugStringW` on Windows; stderr fallback elsewhere |
| `SyslogLogger` | `LOGIT_WITH_SYSLOG=ON` | C++11 | POSIX syslog | Unix-like platforms |
| `EventLogLogger` | `LOGIT_WITH_WIN_EVENT_LOG=ON` | C++11 | Windows SDK | Windows only |
| `SystemLogger` | built in alias | C++11 | Platform system API | Alias for `SyslogLogger` or `EventLogLogger` |
| `OtlpHttpLogger` | `LOGIT_WITH_OTLP=ON` | C++17 | kurlyk (required by the current option) | Outbound OTLP/HTTP; installed exports need external kurlyk |
| `OtlpPayloadLogger` | `LOGIT_WITH_OTLP=ON` | C++17 | kurlyk (required by the shared option; not used for callback transport) | JSON payload callback; application owns transport; async by default |
| `PrometheusPayloadLogger` | `LOGIT_WITH_PROMETHEUS=ON` | C++11 | None | Text payload callback; no HTTP client |
| `PrometheusHttpServerLogger` | `LOGIT_WITH_PROMETHEUS_SERVER=ON` | C++17 | Simple-Web-Server/Asio | Build-tree only; install currently rejected |
| `MdbxLogger` | `LOGIT_WITH_MDBX=ON` | C++17 | mdbx-containers | Not supported on Emscripten or MSVC |

## Delivery models

General-purpose native backends use the shared asynchronous `TaskExecutor` by
default. A backend configured with `use_dedicated_executor=true` owns a
per-backend `SingleThreadExecutor`. Crash backends and
`PrometheusPayloadLogger` are synchronous. OTLP HTTP and OTLP payload
exporters own their queues and workers and can be configured for synchronous or
asynchronous delivery. Emscripten builds without
pthreads use cooperative queues instead of OS worker threads.

## Packaging

The source/build-tree configuration supports all feature combinations subject
to their platform restrictions. Installed CMake exports require optional
dependencies to be provided as installed/imported targets. Bundled optional
dependencies and `LOGIT_WITH_PROMETHEUS_SERVER=ON` are intentionally rejected
by `cmake --install` until their dependency trees can be exported reliably.

See the feature-specific guides for details:

- [`OTLP exporters`](otlp_http_logger.html)
- [`Prometheus`](prometheus_logger.html)
- [`TaskExecutor`](task_executor.html)
- [`Queue back-pressure`](backpressure.html)
