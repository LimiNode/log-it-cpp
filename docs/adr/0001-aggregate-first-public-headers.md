# ADR 0001: Aggregate-first public headers and API boundary

- Status: Accepted
- Date: 2026-09-14

## Context

The library has many historical leaf headers, including
`logit/log_macros.hpp`. Treating every header as independently includable made
include order accidental and made internal implementation details look public.

## Decision

`<logit.hpp>` is the supported application entry point. The module umbrellas
`<logit/utils.hpp>`, `<logit/formatter.hpp>`, and `<logit/loggers.hpp>` are
supported focused entry points. Leaf headers are standalone only when their
documentation and include-contract test explicitly say so. `logit/log_macros.hpp`
is an aggregate-owned implementation header and has no standalone-inclusion
guarantee. Public aliases, including `logit::QueuePolicy`, are consumed through
the umbrella or the relevant module umbrella.

## Consequences

The include contract is predictable and can evolve without promising every
historical file as an API boundary. Documentation and tests must use the
nearest supported umbrella, while implementation headers remain free to change.

## Alternatives considered

Making every leaf header self-contained would increase compile cost and freeze
implementation structure without adding value for normal application code.
