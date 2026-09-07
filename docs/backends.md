\page backends Backend matrix

# Backend matrix

Choose a backend by delivery model, platform, and dependency requirements.
Every backend implements `ILogger`; stored-log backends may additionally
implement `ILogReader` and `ILogSubscriber`.

| Backend | Enablement | Standard | Extra dependency | Platform and packaging notes |
|---|---|---:|---|---|
| Console, file, unique file, memory, crash | built in | C++11 | TimeShield | Native and documented Emscripten stubs |
| `WindowsDebugLogger` | built in | C++11 | Windows API | `OutputDebugStringW` on Windows; stderr fallback elsewhere |
| `SyslogLogger` | `LOGIT_WITH_SYSLOG=ON` | C++11 | POSIX syslog | Unix-like platforms |
| `EventLogLogger` | `LOGIT_WITH_WIN_EVENT_LOG=ON` | C++11 | Windows SDK | Windows only |
| `OtlpHttpLogger` | `LOGIT_WITH_OTLP=ON` | C++17 | kurlyk | Outbound OTLP/HTTP; installed exports need external kurlyk |
| `OtlpPayloadLogger` | `LOGIT_WITH_OTLP=ON` | C++17 | None for callback | JSON payload callback; application owns transport |
| `PrometheusPayloadLogger` | `LOGIT_WITH_PROMETHEUS=ON` | C++11 | None | Text payload callback; no HTTP client |
| `PrometheusHttpServerLogger` | `LOGIT_WITH_PROMETHEUS_SERVER=ON` | C++17 | Simple-Web-Server/Asio | Build-tree only; install currently rejected |
| `MdbxLogger` | `LOGIT_WITH_MDBX=ON` | C++17 | mdbx-containers | Not supported on Emscripten or MSVC |

## Delivery models

General-purpose native backends use the shared asynchronous `TaskExecutor` by
default. A backend configured with `use_dedicated_executor=true` owns a
per-backend `SingleThreadExecutor`. Crash and callback payload backends are
synchronous unless their own configuration says otherwise; OTLP HTTP and
payload exporters own their queues and workers. Emscripten builds without
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
