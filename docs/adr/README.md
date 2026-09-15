# Architecture Decision Records

Architecture Decision Records (ADRs) capture decisions that should remain
discoverable after the implementation or the original discussion changes.
They complement the roadmap: the roadmap says **what is next**, while an ADR
explains **why the current boundary or trade-off exists**.

## Statuses

- **Proposed** — under discussion; do not treat it as a public contract.
- **Accepted** — the current implementation and documentation should follow it.
- **Superseded** — replaced by a newer ADR; retain it for historical context.
- **Rejected** — considered and intentionally not adopted.

## Creating an ADR

1. Copy [`0000-template.md`](0000-template.md) to the next zero-padded number.
2. Use a short, stable title and set the status/date.
3. Describe the context, decision, consequences, and rejected alternatives.
4. Update the implementation and user-facing documentation in the same change
   when the decision changes a public contract.
5. Link a superseding ADR instead of rewriting an accepted historical record.

## Accepted decisions

- [0001 — Aggregate-first public headers and API boundary](0001-aggregate-first-public-headers.md)
- [0002 — Shared and dedicated asynchronous executors](0002-async-executor-model.md)
- [0003 — Immutable logger registry snapshots](0003-immutable-registry-snapshots.md)
- [0004 — TimeShield compatibility and dependency reuse](0004-timeshield-compatibility.md)
- [0005 — Benchmark evidence and comparison methodology](0005-benchmark-methodology.md)
- [0006 — Explicit capability for concurrent dispatch](0006-concurrent-dispatch-capability.md)
