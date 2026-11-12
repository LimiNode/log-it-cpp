# ADR 0010: Atomic registration of prepared configuration batches

- Status: Proposed
- Date: 2026-11-12

## Context

`prepare_configuration()` produces a complete `ConfigurationPreparedSet`.
Each entry owns an `ILogger` and an `ILogFormatter` through `std::shared_ptr`,
because factories may share those objects and the prepared result is copyable.
The live `Logger` registry currently stores each pair in `LoggerStrategy` as
`std::unique_ptr` and exposes `add_logger()` for one-at-a-time registration.

Converting a shared pointer to a unique pointer through `get()` would create
double ownership or dangling references. Registering entries one by one would
also publish a partially applied configuration if a later entry is invalid.
The existing copy-on-write strategy snapshot and shutdown checks must remain
the publication boundary for concurrent dispatch.

## Decision

Add a separate configuration-registration layer; do not change parsing,
binding resolution, or the existing `Logger::add_logger()` call contract in
this slice.

The future registration operation will accept a complete prepared set and an
explicit registration-options object. It will:

1. validate the entire batch before publication;
2. build all new strategies and candidate registry/ID-map snapshots before
   mutating live state;
3. check `m_shutdown` before preparation and again under the registry write
   lock before publication;
4. publish one immutable strategy-list snapshot with one release-store;
5. return a stable mapping from each configuration `id` to its runtime index.

The operation is append-only in v1. Existing strategies are preserved. New
configuration entries use `single_mode=false` unless an explicit per-entry
registration option says otherwise. The operation rejects duplicate IDs in the
incoming batch and IDs already registered by this configuration-registration
layer. Legacy `add_logger()` entries have no configuration ID and do not
silently acquire one. Repeated registration is therefore an explicit append
operation, not replacement or hot reload; callers that need replacement must
use a separately designed API.

The runtime index of an appended entry is its position in the published
registry after the existing strategies. The returned ID/index mapping is the
only supported way for a configuration caller to address a logger; vector
order and backend type are not inferred from the ID.

All potentially throwing validation, allocation, strategy construction, and
ID-map work must finish before mutating `m_loggers`. Under its write lock, the
implementation prepares a complete candidate list and immutable snapshot,
then uses non-throwing swaps for the mutable registry and ID map before the
single release-store of the snapshot. No failure may leave the mutable registry
and published snapshot describing different batches. An empty batch is a
successful no-op with an empty returned mapping.

Shutdown and batch registration must share a linearization boundary. The
future `shutdown()` path must set `m_shutdown` and capture the registry under
the same write lock used by registration. If shutdown wins, registration
fails without publishing. If registration wins, shutdown sees and shuts down
the entire appended batch. The existing fast-path `m_shutdown` check remains
an early exit, not the sole race guard.

### Ownership boundary

The recommended implementation changes `LoggerStrategy`'s internal owners to
`std::shared_ptr`. The unchanged public `add_logger(std::unique_ptr<...>)` API
will wrap each incoming unique pointer in a shared pointer internally. The new
batch operation can then share the prepared logger and formatter without raw
pointer conversion or an ownership transfer that invalidates the prepared
set. Existing dispatch, per-strategy execution locking, and snapshot lifetime
rules remain unchanged.

The prepared set remains caller-owned after successful registration. Clearing
or destroying it must not invalidate the live registry. A failed registration
must leave both the caller's prepared set and the live registry unchanged.

### Atomicity and side effects

Atomicity covers caller-visible registry publication only. The prepared set is
already materialized, so registration must not invoke factories, resolve
bindings, parse input, or start a backend-specific lifecycle outside the
normal logger construction contract. Backend shutdown remains governed by the
existing `Logger::shutdown()` and strategy lifetime rules.

## Consequences

- Configuration batches become visible to dispatch all at once.
- Existing applications retain the unique-pointer `add_logger()` API.
- Shared ownership removes unsafe pointer conversion and allows prepared-set
  copies without changing logger lifetime semantics.
- The internal `LoggerStrategy` owner type changes, so implementation work must
  cover shutdown, snapshots, `wait()`, clear/read APIs, targeted indices, and
  concurrent dispatch on both C++11 and C++17.
- Registration does not provide replacement, rollback of external backend
  effects, file watching, or hot reload.
- Configuration IDs require a small registry-side metadata map; they are not
  retrofitted onto legacy unnamed registrations.

## Regression matrix for implementation

| Case | Required result |
| --- | --- |
| Two-entry batch succeeds | One snapshot publication; both IDs map to consecutive runtime indices after existing strategies. |
| Invalid second entry or duplicate ID | No new strategy or ID is visible; existing registry and caller output remain unchanged. |
| Repeated batch | Duplicate configured IDs fail; no implicit replacement or idempotent success. |
| Legacy `add_logger()` before and after a batch | Existing API remains valid; unnamed indices remain stable and configured ID mapping remains unambiguous. |
| Prepared set destroyed after registration | Backend and formatter remain alive through registry ownership. |
| `single_mode` option and enabled state | Targeted/general routing and default enabled state match ordinary strategy behavior. |
| Concurrent dispatch during registration | Each dispatch sees either the old or complete new snapshot, never a partial batch. |
| Registration versus shutdown | Exactly one wins the registry lock; no published backend escapes shutdown. |
| Factory/formatter failure before registration | No registration call or live mutation occurs. |
| C++11/C++17 and feature-disabled builds | Public `add_logger()` remains source-compatible; registration uses no newer language features or optional parser dependency. |

## Alternatives considered

### Move prepared pointers into `unique_ptr`

Rejected. It would make `ConfigurationPreparedSet` move-only, break the
existing factory/shared-ownership contract, and make failed publication harder
to preserve safely.

### Convert `shared_ptr` through `get()`

Rejected. Constructing a `unique_ptr` from that raw pointer would either cause
double deletion or require a non-owning deleter, leaving lifetime dependent on
the prepared set.

### Register each entry with the existing `add_logger()`

Rejected for a configuration batch. It exposes partial state and cannot return
one atomic publication point or an unambiguous batch ID/index mapping.

### Replace the registry wholesale on every configuration load

Deferred. Replacement needs explicit semantics for in-flight records,
shutdown, ID collisions, and rollback. It is not implicit hot reload.

## References

- [Configuration runtime preparation](../configuration-loading.md)
- [ADR 0003: Immutable logger registry snapshots](0003-immutable-registry-snapshots.md)
- [Logger registry implementation](../../include/logit_cpp/logit/Logger.hpp)
- [Prepared runtime types](../../include/logit_cpp/logit/configuration_runtime.hpp)
