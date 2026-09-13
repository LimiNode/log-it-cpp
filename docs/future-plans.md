# Roadmap and delivery checklist — LogIt++

This file is the maintained roadmap, not a changelog. It deliberately separates
delivered capabilities from planned work and from ideas that are intentionally
delegated to user-defined `ILogger` implementations.

Legend:

- `[x]` delivered and covered by code/docs/tests;
- `[~]` available, but opt-in or platform-limited;
- `[ ]` planned work;
- `[-]` intentionally deferred, with no current commitment to implement it in
  the core library.

## Delivered

- [x] Public aggregate-first header policy: `<logit.hpp>` is the supported
  application entry point; module umbrellas and leaf-header contracts are
  documented.
- [x] Public `logit::QueuePolicy` API and queue policy macros.
- [x] Immutable copy-on-write logger registry snapshots on the production hot
  path, with atomic logger state and the existing execution mutex retained for
  formatter/backend safety.
- [~] Dedicated per-backend worker threads via
  `Config::use_dedicated_executor=true`. The default remains the shared
  `TaskExecutor`; unsupported platform backends keep their documented
  limitations.
- [x] Queue capacity, overflow policies, drop counters, resize behavior, and
  flush/shutdown coverage.
- [x] MDC/NDC context, structured records, stored-log readers/subscribers, and
  runtime log-level controls.
- [x] TimeShield `2.0.x` compatibility (minimum `2.0.0`) and dependency-target
  reuse for build-tree and installed-package consumers.
- [x] Benchmark methodology and regression infrastructure: prepared-record hot
  path A/B, public `LOGIT_INFO(...)` smoke benchmark, queue/CSV validation,
  delayed-sink flush regression, and CI coverage.
- [x] Doxygen generation, GitHub Pages publication, header/layout smoke tests,
  and the comparison guide for spdlog, Quill, Boost.Log, glog, and IceCream-Cpp.

## Next planned work

- [ ] **Release 1.0.2** — prepare changelog/release notes, verify package
  overlays, generate the non-`-dev` documentation, tag the release, and verify
  the published Pages site.
- [ ] **Binary logging research** — choose a versioned binary record format,
  define compatibility/versioning rules, and prototype a reader before adding
  a production backend.
- [ ] **Transparent compressed-file reads** — make `read_log_file()` and
  `read_log_files()` read `.gz`/`.zst` entries when the corresponding feature is
  enabled, with platform-specific tests.
- [ ] **Configuration loading** — design a versioned JSON/properties mapping to
  the existing backend configuration. Treat file watching/hot reload as a
  follow-up, not part of the first configuration API.
- [ ] **Extended filtering** — evaluate source/file, message, tag/MDC, and
  range filters; define their cost and ordering before adding public API.
- [ ] **Benchmark follow-up** — add a genuinely formatted public-macro scenario,
  a versioned fixture containing compiler/toolchain/commit/queue/flush metadata,
  and a broader 1/4/16/32-producer matrix. Keep publication numbers tied to a
  fixed machine and toolchain.
- [ ] **Concurrency fast-path research** — only after documenting a formal
  thread-safety capability for formatters/backends. Do not remove `exec_mx`
  based on benchmark results alone.

## Intentionally deferred

- [-] **Database/SQLite appender** — currently better implemented as a custom
  `ILogger`; revisit if a common portable schema and maintenance owner emerge.
- [-] **Telnet/TCP appender** — application-specific transport and access
  policy; implement as a custom backend unless a supported protocol is agreed.
- [-] **Main-thread GUI appender** — event-loop integration is framework and
  platform specific; revisit when LogIt++ ships a GUI viewer.

## How to update this document

Move an item to `[x]` only when the implementation, tests, and user-facing
documentation agree. Use an ADR in [`docs/adr`](https://github.com/LimiNode/log-it-cpp/tree/main/docs/adr) for architectural
decisions or compatibility changes; keep this file focused on status and next
steps. Record measurements with their environment instead of turning one local
run into a universal performance claim.

Last reviewed: 2026-09-14.
