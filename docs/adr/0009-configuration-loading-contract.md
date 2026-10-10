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
- The semantic model distinguishes three default owners:
  - backend-native fields use defaults from the selected backend's public C++
    `Config` (for example `FileLogger::Config` or `MdbxLogger::Config`);
  - formatter fields use the selected formatter's public `Config` (the v1
    default formatter is `SimpleLogFormatter::Config`);
  - logger-strategy fields use explicit loader/API defaults matching current
    runtime behavior. In particular, an omitted `level` means
    `LogLevel::LOG_LVL_TRACE`.
- A semantic option is exposed at a shared level only when the selected backend
  has a lossless mapping for it. Otherwise it remains backend-specific, or its
  presence is a validation error; values are never silently ignored.
- Backend-specific sections contain only options owned by the selected backend.
  The initial supported mapping will cover the existing public backend
  `Config` structures, including file/unique-file, console, memory, MDBX,
  OTLP, Prometheus, syslog, event log, and Windows debug configurations where
  the target platform and optional feature are available.
- Executor-style `queue_capacity` and `queue_policy` are therefore available
  only for backends whose public contract uses that executor configuration.
  Backend-native queues retain their own fields and semantics, such as MDBX or
  OTLP `max_queue_size` and `drop_on_overflow`; v1 does not pretend these are
  interchangeable with executor queue settings.
- Unknown logger types, unknown fields, duplicate ids, malformed values,
  unsupported combinations, and unavailable optional backends are validation
  errors. They must produce a diagnostic containing the document path and a
  stable error category; v1 does not silently ignore unknown input.
- `level`, `queue_policy`, compression settings, and formatter settings use
  named values defined by the public API. Numeric or frontend-specific aliases
  are not part of the v1 contract unless explicitly added to the schema.
- Formatter configuration is data-only in v1 (for example pattern and JSON
  mode). Callback/function members such as `on_payload`, `on_error`, and
  collection callbacks are not serializable. They require a programmatic
  binding/resolver before logger construction and are never represented as
  executable data in JSON or properties.

An illustrative JSON representation of the semantic model is:

```json
{
  "schema_version": 1,
  "loggers": [
    {
      "id": "application-file",
      "type": "file",
      "strategy": {
        "level": "info",
        "formatter": { "pattern": "%v", "json": false }
      },
      "backend": {
        "directory": "logs",
        "async": true,
        "queue_policy": "block",
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
    -> backend and formatter Config materialization using their defaults
    -> programmatic binding/resolution of callbacks and external resources
    -> logger instantiation
```

The conceptual API has two phases: loading/validation returns a complete plan;
instantiation consumes that plan together with programmatic bindings and
resources. A convenience API may compose the phases, but it must resolve all
required bindings before constructing a logger. A callback-dependent backend
without its required binding is a `missing_programmatic_binding` diagnostic at
the logger path, not a successfully instantiated inert backend. Backends that
explicitly document an inert/no-output mode may opt into that mode instead.

The final operation is atomic with respect to the document: it returns a
complete validated plan/instance set or diagnostics without partially applying
the document. v1 does not define file watching, hot reload, transactional
replacement of live loggers, or environment-variable interpolation.

## Consequences

The contract gives JSON and properties the same behavior while keeping backend,
formatter, and strategy defaults explicit. Strict validation makes deployment
mistakes visible and gives callers actionable paths such as
`loggers[1].backend.queue_policy`.
The model can be tested without making a particular JSON library a core
dependency; a JSON frontend may remain optional or thin.

The initial implementation must maintain an explicit mapping for each backend,
including platform and feature availability checks and any required callback
bindings. Adding a new backend or serializable option requires updating the
schema, mapping, validation, and documentation together. Callbacks and other
executable behavior remain an intentional programmatic boundary.

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
