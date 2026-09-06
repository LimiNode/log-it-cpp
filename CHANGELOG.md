# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

Target release: **v1.0.2**

### Added

- Raw and section logging macros for unformatted diagnostic snapshots, plus in-memory and persisted snapshot/file access APIs.
- POSIX syslog, Windows Event Log, POSIX/Windows crash logger, Prometheus payload/registry/HTTP server, OTLP/HTTP, and MDBX logger backends.
- Structured OTLP attributes, callback-based exporting, payload splitting, compression, export counters, and MDC/NDC context support.
- Compile-time and runtime log-level controls, conditional/frequency/tagging helpers, stream/printf/fmt/scope macro families, and configurable console output.
- File rotation by size and timestamp with retention policies and gzip/zstd/external-command compression.
- Configurable asynchronous backpressure, queue policies, lock-free MPSC execution, hot queue resizing, and dedicated executor controls.
- Emscripten support, CMake/pkg-config package metadata, vcpkg integration, install-consumer coverage, and latency benchmarks.

### Changed

- Reorganized public umbrella headers and moved implementation helpers under `detail` while preserving header-only ODR safety.
- Made logger configuration, shutdown, queue resizing, and single-thread executor lifecycle behavior explicit and consistent across backends.

### Fixed

- Corrected file rotation ordering/error handling, async shutdown and flushing, queue drop accounting, MPSC lifecycle handling, and benchmark synchronization.
- Fixed platform portability issues, crash-filter naming collisions, disabled-fmt macro behavior, scope duration logging, and missing/self-contained header dependencies.

### Packaging / Build

- Refreshed bundled dependency revisions and canonical repository URLs.
- Added fail-closed installation checks for unsupported bundled optional dependencies and source-tree-only Prometheus server headers.

### CI / Testing

- Expanded regression coverage for sanitizers, TSAN, ODR, Emscripten, optional compression/backends, package consumers, and platform-specific behavior.

### Documentation

- Refreshed README, README-RU, Doxygen, examples, macro references, task-executor guidance, and scoped `AGENTS.md` instructions.

## [v1.0.1] - 2025-08-05
- Added initial CMake integration for building, installing, and consuming the header-only package.
- Added Emscripten build support with browser console output and runtime export wiring.
- Added minimum log-level control macros and related logger configuration coverage.
- Improved Doxygen mainpage and macro documentation, including version tag injection during documentation publishing.
- Added docs grouping support and updated publish workflow paths.
- Cleaned up public include dependencies and redundant includes while preserving compatibility with existing entry headers.
- Restored and updated the TimeShield submodule reference used by the release.
- Fixed path utility comments, Doxygen comments, `argument_utils` unused parameter warnings, formatter fallthrough warnings, and `VariableValue` overload ambiguity for `bool`.

## [v1.0.0] - 2025-07-17
- Initial public release of the header-only LogIt++ logging library.
- Added console, file, and unique-file logger backends with asynchronous task execution.
- Added configurable log formatting with pattern compiler support and formatter interfaces.
- Added logging macros for trace, debug, info, warn, error, and fatal levels.
- Added stream-style logging and variable argument formatting helpers.
- Added path, encoding, formatting, and argument parsing utilities.
- Added examples for basic logging, custom backends, customized settings, and short macro names.
- Added README, README-RU, Doxygen configuration, generated documentation styling, and MIT license metadata.
