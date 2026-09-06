# Logger backend instructions

Each logger backend must honor the common `ILogger` lifecycle contract while
remaining independently usable through `loggers.hpp`.

- `log()` must become a no-op after shutdown and must not access backend state
  after the logger's worker or callback resources are released.
- Serialize backend-specific writes with the backend mutex/executor only; do
  not hold the global `Logger` registry lock while invoking user callbacks or
  doing I/O.
- Snapshot callback/subscriber lists before invocation. Callbacks may remove
  themselves, add another callback, throw, or call back into the logger.
- Keep synchronous and asynchronous modes behaviorally equivalent for ordering,
  error reporting, and shutdown. Test both modes when changing dispatch.
- Optional backends must remain behind their feature macros and must include
  only the dependency umbrella supplied by CMake.
- File and MDBX backends own durable resources. Close/commit them before
  destruction and report failures through the configured error callback without
  allowing callback exceptions to escape a `noexcept` path.

Update the backend-specific documentation and tests with any lifecycle or
configuration change. Do not duplicate serialization or retry logic when a
shared helper already owns that behavior.
