# ADR 0003: Immutable logger registry snapshots

- Status: Accepted
- Date: 2026-09-14

## Context

Copying the mutable logger registry under a read lock on every log call made
the normal dispatch path pay synchronization and allocation costs. Logger
registry mutations and reconfiguration are infrequent compared with logging.

## Decision

Publish an immutable copy-on-write strategy-list snapshot on registry changes.
The hot path reads it with an atomic `shared_ptr` load. Mutable `enabled` and
`single_mode` state is atomic. The per-strategy execution mutex remains around
formatter/backend invocation because custom formatters and backends do not
have a universal concurrent-invocation contract.

## Consequences

Normal dispatch avoids the registry mutex and temporary vector copy while
preserving safe lifetime and mutation semantics. A future lock-elision path
requires an explicit capability contract and separate regression coverage; a
benchmark sink alone is not evidence that arbitrary backends are thread-safe.
