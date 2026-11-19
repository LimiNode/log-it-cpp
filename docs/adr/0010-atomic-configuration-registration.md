# ADR 0010: Atomic registration of prepared configuration batches

- Status: Proposed
- Date: 2026-11-12
- Last revised: 2026-11-20

## Context

`prepare_configuration()` produces a complete `ConfigurationPreparedSet`.
Each entry owns an `ILogger` and an `ILogFormatter` through `std::shared_ptr`,
and the prepared result is copyable. Shared lifetime does not imply that these
objects support concurrent use or registration in multiple strategies.
The live `Logger` registry currently stores each pair in `LoggerStrategy` as
`std::unique_ptr` and exposes `add_logger()` for one-at-a-time registration.

Converting a shared pointer to a unique pointer through `get()` would create
double ownership or dangling references. Registering entries one by one would
also publish a partially applied configuration if a later entry is invalid.
Each `LoggerStrategy` has its own `exec_mx`. Registering the same backend or
formatter in two strategies could bypass serialization for a non-concurrent
object and call the same backend's `shutdown()` twice. The existing
copy-on-write strategy snapshot must remain the dispatch publication boundary.

The existing `add_logger()` also appends to `m_loggers` before allocating its
dispatch snapshot. Allocation failure can therefore leave the mutable registry
ahead of the snapshot. The implementation follow-up must fix this existing
exception-safety defect as part of a consistent registry publication contract.

## Decision

This ADR specifies future implementation, not an API delivered by this PR.
Add a separate configuration-registration layer with a complete prepared set,
explicit registration options, diagnostics, and a caller-visible result. Keep
parsing, binding resolution, and broad dispatcher refactoring outside this
slice. Preserve the existing `Logger::add_logger()` signature and observable
successful append behavior.

### Append-only registration and identity

Registration preserves existing strategies, their indices, flags, and backend
state. It appends the whole new batch in prepared-entry order with one
immutable dispatch snapshot publication. There is no replacement or hot reload.
An already registered configuration ID is rejected, so repeating the same
batch fails rather than becoming idempotent or appending duplicate loggers.
A later batch with fresh IDs and fresh runtime objects may append normally.

Legacy `add_logger()` entries remain unnamed and do not acquire configuration
IDs. The result maps each incoming configuration ID to its appended runtime
index after all existing strategies. Callers use this mapping for the existing
index-based APIs; IDs are not inferred from backend types. Later appends do not
change any previously returned index.

### Validate every prepared batch

`ConfigurationPreparedSet` is publicly constructible and mutable. Registration
must not assume it was returned unchanged by `prepare_configuration()`. Before
any live mutation, validate:

- supported `schema_version` (`1` in v1), even for an empty batch;
- non-empty IDs, unique within the batch and absent from the live configured-ID
  map;
- non-null backend and formatter for every entry;
- absence of object aliases as defined below, within the batch and against
  existing strategies;
- a representable final registry count and public logger indices;
- valid per-entry registration options, keyed only by IDs in this batch.

The v1 options contain only Boolean `enabled` and `single_mode` flags, defaulting
to `true` and `false` respectively. Unknown option IDs are errors, not ignored
settings for future or existing entries; no other option fields are supported.
Prepared `level` metadata must be a declared `LogLevel` value. Registration
does not reapply levels, rebuild formatters, or otherwise mutate the shared
objects: applying schema strategy remains `prepare_configuration()`'s job.
Callers supplying objects manually are responsible for preparing their state.

For minimal compatibility, cap the final registry **count** at `INT_MAX`, not
`INT_MAX + 1`. Existing readers compare an index with `static_cast<int>(size)`;
the supported index range is therefore `0` through `INT_MAX - 1`. Compute the
capacity check as `batch_size <= limit - current_size` after validating
`current_size <= limit`, before addition or narrowing. Apply the same limit
to legacy appends. Allowing index `INT_MAX` would require auditing and changing
those readers and is outside v1. Test this boundary with a private count seam,
not a registry containing billions of loggers.

A valid empty batch is a successful no-op with an empty result mapping and no
snapshot publication, provided shutdown has not won. Unknown option IDs,
invalid schema, and shutdown are still failures for an empty batch.

### Ownership boundary

The recommended implementation changes `LoggerStrategy`'s internal owners to
`std::shared_ptr`. The unchanged public `add_logger(std::unique_ptr<...>)` API
will construct each internal shared pointer by moving the unique pointer.
The new batch operation can share the prepared logger and formatter without raw
pointer conversion or an ownership transfer that invalidates the prepared
set. This is a lifetime mechanism only; it does not add object-level locks.

Reject the entire batch if a backend or formatter object would belong to more
than one strategy. Check both the incoming entries and all live strategies,
including prior configuration batches and legacy unnamed entries. Reject such
aliases regardless of concurrent-dispatch capability, preventing ambiguous
state mutation and repeated backend shutdown as well as concurrent invocation.

Use object identity, not `shared_ptr::use_count()` or control-block ownership.
For these polymorphic interfaces, the most-derived address obtained via
`dynamic_cast<const void*>(pointer.get())` identifies the complete object even
when interface subobject addresses differ. One complete object must not appear
in different strategies, including across backend/formatter roles. Distinct
objects sharing one control block are not aliases merely for that reason.
Within one strategy, a combined backend/formatter object still has only one
execution mutex and must satisfy both interface contracts. If both interfaces
refer to the same complete object, their shared pointers must also share one
ownership control block, checked in C++11 by symmetric `owner_before()`:
`!backend.owner_before(formatter) && !formatter.owner_before(backend)`.
Independent control blocks for that same object are rejected before publication.
Distinct complete objects sharing one control block remain valid. No global
object-lock registry or new hot-path synchronization is introduced in v1.

The prepared set remains caller-owned after successful registration. Clearing
or destroying it must not invalidate the live registry. A failed registration
must leave both the caller's prepared set and the live registry unchanged.
Externally retained pointers preserve lifetime but do not authorize
unsynchronized external invocation, mutation, or shutdown. Use `Logger`'s
registered-object APIs; direct use must satisfy the existing interface and
lifecycle contracts and be synchronized with registry-mediated operations by
the caller. Callers must not concurrently modify the prepared set, options, or
result slot during registration. Holding an external shared owner does not
prevent `Logger::shutdown()` from shutting down the object.

### Strong exception safety and result publication

All registry membership writers, including legacy `add_logger()`, must use a
complete candidate registry and preconstructed immutable dispatch snapshot.
Do not `push_back()` into the live `m_loggers` and then allocate its snapshot.
The batch path also prepares a candidate configured-ID map and a complete result
mapping before commit. Existing strategy objects are shared into the candidate,
not mutated or reconstructed.

Use a caller-owned `std::shared_ptr<const RegistrationResult>` result slot
(conceptual type name) for the new operation. The immutable result contains
the fully allocated ID-to-`int`-index map for this batch. Prepare its object and
control block before commit, then hand it off with `shared_ptr::swap`, which
is non-throwing in C++11. Do not build or copy a map after registry publication
or rely on optional return-value optimization to make a by-value map safe.
The previous caller result remains unchanged on any pre-commit failure.

While holding the registry write lock, the commit sequence is:

1. Finish all validation and candidate allocations, including the snapshot,
   live configured-ID map, and immutable caller result.
2. Swap the candidate registry and ID map into live storage using proven
   non-throwing operations (default allocators/comparators; no user hooks).
3. Release-store the prepared `shared_ptr<const StrategyList>` snapshot once.
   This is the successful registration's linearization point for dispatch.
4. Swap the prepared result handle into the caller's result slot, release the
   registry lock, and return success with no further throwing work.

Lock-based readers cannot observe the intermediate swaps; lock-free dispatch
retains the old snapshot until the release-store and then can see the entire
batch. There must be no allocation, diagnostic formatting, callback, or
throwing result construction after the first live swap. Keep old snapshot and
old result owners alive through commit; release displaced owners and failed
candidates after unlocking so user destructors/deleters cannot run under the
registry mutex. Custom destructors/deleters must be non-throwing.

On validation failure, diagnostics may change but the registry, configured-ID
map, dispatch snapshot, prepared input, and previous result stay unchanged.
Allocation exceptions may propagate before commit rather than trying to
allocate an error diagnostic during `std::bad_alloc`; they must have the same
strong preservation guarantee. No path may publish successfully and then
report failure due to a result allocation or copy.

Legacy `void add_logger()` keeps its unique-pointer parameters, defaults,
successful append behavior, and no-op behavior after shutdown. Its ownership
arguments remain consumed by value; a failure need not return them to the
caller. A pre-commit allocation failure must leave its registry and dispatch
snapshot unchanged. Exhausting the index domain must fail before mutation
(for the legacy void API, a `std::length_error`, not a truncated index).

### Registration/shutdown linearization and locks

Build new strategy objects, pointer identities, and capability flags before
the registry lock; capability queries are virtual calls and must not run under
that lock. They retain ADR 0006's existing meaning and are cached per strategy.
No capability exempts objects from alias rejection.

Registration then takes `m_loggers_mx` in write mode and rechecks shutdown,
duplicate IDs, object aliases against the current registry, and final index
capacity. Only this locked view determines the appended indices. Construct
the remaining candidates under that lock without invoking user code, then
perform the non-throwing commit above. Concurrent legacy and batch writers
are serialized by this same lock. Registration never takes `exec_mx`.

Shutdown takes the registry write lock, checks whether shutdown already won,
retains the already-published immutable snapshot by shared-pointer copy
(without allocating a vector), and sets `m_shutdown` while still locked.
The transition to shutdown is its linearization point. Release the registry
lock before taking any strategy `exec_mx`, invoking backend `shutdown()`, or
shutting down the shared executor. Shutdown holds at most one strategy mutex
at a time; it never reacquires the registry lock while holding that mutex.
Repeated shutdown calls remain no-ops under the existing contract. This
specifies the registration/shutdown paths, not new general callback reentrancy
guarantees for other lifecycle/control APIs.

If registration publishes first, the shutdown snapshot contains every member
of the committed batch. If shutdown sets the flag first, registration fails
without publication or result replacement. Early atomic `m_shutdown` checks
are optimizations, not substitutes for these locked checks. In-flight
dispatch may retain an older snapshot, as in ADR 0003. Serialized dispatch
continues to use `exec_mx`; opted-in concurrent pairs retain ADR 0006's
requirements for safe concurrent lifecycle calls. Do not change those
contracts or promise synchronous cancellation of in-flight calls.

### Atomicity and side effects

Atomicity covers caller-visible registry publication only. The prepared set is
already materialized, so registration must not invoke factories, resolve
bindings, parse input, or start a backend-specific lifecycle outside the
normal logger construction contract. Factory side effects are not transactional.
No implementation of the registration or shutdown changes is part of this PR.

## Consequences

- Configuration batches become visible to dispatch all at once.
- Existing applications retain the unique-pointer `add_logger()` API.
- Shared ownership preserves lifetime, while alias rejection prevents multiple
  strategy locks and shutdown paths from independently controlling one object.
- The internal `LoggerStrategy` owner type changes, so implementation work must
  cover shutdown, snapshots, `wait()`, clear/read APIs, targeted indices, and
  concurrent dispatch on both C++11 and C++17.
- Candidate construction also repairs legacy `add_logger()` exception safety;
  it adds only cold-path work, not a dispatch object-lock registry.
- Registration does not provide replacement, rollback of external backend
  effects, file watching, or hot reload.
- Configuration IDs require a small registry-side metadata map; they are not
  retrofitted onto legacy unnamed registrations.

## Regression matrix for implementation

| Case | Required result |
| --- | --- |
| Two-entry batch succeeds | One snapshot publication; both IDs map to consecutive runtime indices after existing strategies. |
| Invalid second entry or duplicate ID | No new strategy or ID is visible; existing registry and caller output remain unchanged. |
| Shared backend pointer under two different IDs | Reject the whole batch, with concurrent capability either enabled or disabled. |
| Shared formatter pointer under two different IDs | Reject the whole batch; no log/format invocation or object state change during validation. |
| Runtime object reused in a later batch | Reject even with a fresh ID; include aliasing against legacy entries and different interface subobject addresses. |
| Distinct objects sharing one control block | Do not falsely reject solely because shared ownership has the same control block. |
| Combined backend/formatter object in one strategy | Accept only with one shared ownership control block; reject independent control blocks for the same complete object without publication. |
| Manually constructed prepared set | Reject unsupported schema, empty/duplicate IDs, null objects, invalid level/options, and unknown option IDs without publication. |
| Snapshot allocation failure in batch registration | Mutable registry, ID map, dispatch snapshot, prepared input, and old result remain unchanged. |
| Returned ID/index result allocation failure | No publication or partial mapping; old caller result remains unchanged. |
| Legacy `add_logger()` snapshot allocation failure | No mutable registry/snapshot divergence; previous entries remain usable. |
| Maximum valid index and overflow | Private count seam accepts final count `INT_MAX`/last index `INT_MAX - 1` and rejects the next append without narrowing or arithmetic overflow. |
| Repeated batch | Duplicate configured IDs fail; no implicit replacement or idempotent success. |
| Legacy `add_logger()` before and after a batch | Existing API remains valid; unnamed indices remain stable and configured ID mapping remains unambiguous. |
| Externally retained or destroyed prepared set | Registry retains the objects; external owners obey invocation/mutation/shutdown restrictions and do not receive a second shutdown authority. |
| `single_mode` option and enabled state | Targeted/general routing and default enabled state match ordinary strategy behavior. |
| Concurrent dispatch during registration | Each dispatch sees either the old or complete new snapshot, never a partial batch. |
| Registration versus shutdown | Deterministic barriers exercise both lock orders: either all entries are in the shutdown snapshot or registration fails unchanged. No sleeps or production-singleton restart. |
| Shutdown callback queries `logger_count()` while a registration competes | No registry mutex is held during backend shutdown; registration never waits on `exec_mx`. |
| Valid empty batch | Empty result, no publication; invalid schema/options or prior shutdown still fail. |
| Factory/formatter failure before registration | No registration call or live mutation occurs. |
| C++11/C++17 and feature-disabled builds | Public `add_logger()` remains source-compatible; registration uses no newer language features or optional parser dependency. |

Use focused allocation-failure seams for snapshot and result construction,
and controlled barriers for the shutdown race. Run singleton shutdown cases
in process-isolated regression executables or through a suitable private test
seam. Do not add a production singleton reset/restart API. Checks must remain
active with and without `NDEBUG`.

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

### Share one object across strategies with a global lock registry

Rejected for v1. It would add object-level synchronization and lifecycle
coordination across strategies and complicate the concurrent-dispatch path.
Rejecting aliases is a smaller, explicit cold-path restriction.

### Replace the registry wholesale on every configuration load

Deferred. Replacement needs explicit semantics for in-flight records,
shutdown, ID collisions, and rollback. It is not implicit hot reload.

## References

- [ADR 0009: Versioned configuration loading contract](0009-configuration-loading-contract.md)
- [ADR 0003: Immutable logger registry snapshots](0003-immutable-registry-snapshots.md)
- [ADR 0006: Explicit capability for concurrent dispatch](0006-concurrent-dispatch-capability.md)
- [Logger registry implementation](../../include/logit_cpp/logit/Logger.hpp)
- [Prepared runtime types](../../include/logit_cpp/logit/configuration_runtime.hpp)
