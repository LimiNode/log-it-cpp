# ADR 0002: Shared and dedicated asynchronous executors

- Status: Accepted
- Date: 2026-09-14

## Context

Using one shared asynchronous executor is efficient for the default case, but
a slow backend can delay unrelated backends. A separate worker per backend
improves isolation at the cost of a thread and queue per opted-in backend.

## Decision

The shared `TaskExecutor` remains the default. Backends that support it expose
`Config::use_dedicated_executor`; when enabled for an asynchronous backend, it
owns a `SingleThreadExecutor` with its own queue, policy, wait, and shutdown
lifecycle. Platform-specific backends that cannot provide this behavior keep
their documented limitations.

## Consequences

Applications can choose throughput/resource sharing or isolation per backend.
The feature is already tested for lifecycle, mixed shared/dedicated operation,
and macro configuration. Changing the default is a separate compatibility and
resource-usage decision.
