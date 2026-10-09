# ADR 0009: Versioned configuration loading contract

- Status: Proposed
- Date: 2026-10-10

## Context

LogIt++ exposes several backend-specific `Config` structures. Applications can
construct those structures directly today, but there is no stable file or
properties contract for selecting multiple loggers and applying their options.
Adding a parser before defining that contract would create a second set of
defaults and validation rules, and could make JSON and properties inputs behave
differently.

The first configuration API must describe static construction only. Watching a
file and hot reload introduce lifecycle, replacement, and failure semantics
that should be designed after the initial load model is stable.

## Decision

Configuration loading v1 will use a parser-neutral semantic model. JSON and
properties frontends, whether built in or optional, must map into that same
model before validation and logger construction. No frontend is allowed to
instantiate a backend directly with frontend-specific defaults.

The model has these top-level requirements:

- `schema_version` is required and selects a versioned contract. A loader must
  reject unsupported versions rather than silently applying a different schema.
- `loggers` is a required array. Each entry has a stable `id`, a `type`, and
  backend options. Logger ids must be unique within one document.
- Common options are represented once: `level`, `async`,
  `use_dedicated_executor`, `queue_capacity`, `queue_policy`, and `formatter`.
  A backend may reject an option that it cannot support; it must not silently
  ignore it.
- Backend-specific sections contain only options owned by the selected backend.
  The initial supported mapping will cover the existing public backend
  `Config` structures, including file/unique-file, console, memory, MDBX,
  OTLP, Prometheus, syslog, event log, and Windows debug configurations where
  the target platform and optional feature are available.
- When a field is missing, the loader uses the default of the corresponding
  C++ `Config`. Defaults are therefore owned by the C++ configuration types
  and must not be duplicated as an independent parser default table.
- Unknown logger types, unknown fields, duplicate ids, malformed values,
  unsupported combinations, and unavailable optional backends are validation
  errors. They must produce a diagnostic containing the document path and a
  stable error category; v1 does not silently ignore unknown input.
- `level`, `queue_policy`, compression settings, and formatter settings use
  named values defined by the public API. Numeric or frontend-specific aliases
  are not part of the v1 contract unless explicitly added to the schema.
- Formatter configuration is data-only in v1 (for example pattern and JSON
  mode). Callback/function members such as `on_payload`, `on_error`, and
  collection callbacks are not serializable. They require programmatic binding
  after loading and are never represented as executable data in JSON or
  properties.

An illustrative JSON representation of the semantic model is:

```json
{
  "schema_version": 1,
  "loggers": [
    {
      "id": "application-file",
      "type": "file",
      "level": "info",
      "async": true,
      "queue_policy": "block",
      "formatter": { "pattern": "%v", "json": false },
      "backend": {
        "directory": "logs",
        "compress_level": 1
      }
    }
  ]
}
```

The exact frontend syntax may differ for properties, but it must produce the
same semantic values, defaults, validation results, and diagnostics as this
model. The example does not add fields to any backend `Config` by itself.

The load pipeline is:

```text
source (JSON/properties)
    -> parser-neutral semantic model
    -> schema and value validation
    -> backend Config construction using C++ defaults
    -> logger instantiation
```

Loading is an explicit operation that either returns a complete validated
configuration/instance set or returns diagnostics without partially applying
the document. v1 does not define file watching, hot reload, transactional
replacement of live loggers, or environment-variable interpolation.

## Consequences

The contract gives JSON and properties the same behavior and keeps existing C++
`Config` defaults authoritative. Strict validation makes deployment mistakes
visible and gives callers actionable paths such as `loggers[1].queue_policy`.
The model can be tested without making a particular JSON library a core
dependency; a JSON frontend may remain optional or thin.

The initial implementation must maintain an explicit mapping for each backend,
including platform and feature availability checks. Adding a new backend or
serializable option requires updating the schema, mapping, validation, and
documentation together. Callbacks and other executable behavior remain an
intentional programmatic boundary.

## Alternatives considered

### Bind JSON directly to each `Config`

Rejected for v1. It duplicates parser behavior across backends, makes defaults
drift from C++, and leaves no shared semantics for a properties frontend.

### Permissive unknown-field handling

Rejected for v1. Ignoring a misspelled backend or option can produce a logger
that appears configured while silently using an unintended default. Forward
compatibility is handled by `schema_version` and future schema revisions.

### Make `nlohmann::json` a required core dependency

Rejected at the contract stage. The semantic model must stay independent of a
parser library so the header-only core and optional-feature builds do not gain
an unnecessary mandatory dependency.

### Include hot reload in the first API

Rejected. Reload requires decisions about ownership, in-flight records,
failure rollback, callback rebinding, and concurrent readers. Those semantics
should follow a stable static-load contract rather than constrain it in advance.

## References

- [Roadmap: Configuration loading](../future-plans.md)
- [ADR 0002: Shared and dedicated asynchronous executors](0002-async-executor-model.md)
- [ADR 0008: Binary log record format research prototype](0008-binary-log-record-format-research.md)
