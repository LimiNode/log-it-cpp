# ADR 0005: Benchmark evidence and comparison methodology

- Status: Accepted
- Date: 2026-09-14

## Context

Prepared records, the public macro path, formatted output, and external
library adapters measure different work. Mixing them into one number produces
misleading claims, and short CI runners are not a substitute for a fixed
publication machine.

## Decision

Keep separate scenarios for prepared-record dispatch, the real public
`LOGIT_INFO(...)` path, formatting, and external-library comparisons. The
public macro smoke benchmark may use a passthrough formatter when it is
explicitly documented as record-construction/dispatch coverage. Producer
scaling measurements create all workers before the timed interval and release
them through a shared start barrier; an optional warmup is separate from the
measured run. Report absolute timings only with compiler, platform, commit,
build type, architecture, machine/CPU identity, queue settings, and separate
latency-completion and flush-barrier semantics. Treat CI runs as regression
smoke unless the environment is fixed.

## Consequences

Benchmark documentation remains comparable and honest across changes. New
scenarios require their own workload contract and should not silently replace
historical measurements.
