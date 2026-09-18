# ADR 0006: Explicit capability for concurrent dispatch

- Status: Accepted
- Date: 2026-09-15

## Context

`Logger::log()` historically serialized formatter and backend calls with one
execution mutex per strategy. This protects mutable formatter state, backend
state, and lifecycle operations, but it also makes independent producer calls
contend on that mutex. A benchmark result from one sink cannot establish a
thread-safety contract for every custom formatter or backend.

## Decision

`ILogFormatter::supports_concurrent_format()` and
`ILogger::supports_concurrent_log()` are explicit opt-in capability hooks. Both
must return `true` before `Logger` elides a strategy's `exec_mx` during dispatch.
The default is `false`, preserving serialized behavior for all existing and
custom implementations.

An opting-in formatter must support concurrent `format()` calls and concurrent
configuration through `set_timestamp_offset()`. An opting-in backend must make
`log()`, `get_log_level()`, `set_log_level()`, `wait()`, and `shutdown()` safe
when called concurrently, and must keep its resources alive until those calls
finish. The backend remains responsible for its own output ordering and
serialization where required.

The capability is stored when a logger strategy is registered, so the normal
dispatch path does not perform virtual capability checks. The execution mutex
remains in the fallback path and is still used by lifecycle/configuration APIs.
Built-in backends do not opt in until each backend's complete lifecycle and
formatter contract has been audited.

## Consequences

Safe custom pairs can avoid the per-strategy execution mutex under producer
contention. Existing behavior and safety guarantees are unchanged by default.
The benchmark and regression test compare serialized and explicitly opted-in
pairs across 1, 4, 16, and 32 producers; their measurements are machine
specific and are not evidence that arbitrary backends may opt in.
