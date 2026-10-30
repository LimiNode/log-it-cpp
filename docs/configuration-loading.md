# Configuration loading

LogIt++ configuration is split into explicit phases:

```text
JSON value or text
    -> ConfigurationDocument
    -> validation and materialization
    -> programmatic binding resolution
    -> runtime logger construction
```

The core semantic model and runtime layers do not depend on a JSON library.
The JSON frontend is optional and uses `nlohmann::json` only when
`LOGIT_WITH_JSON_CONFIG=ON` is selected.

## Enabling the JSON frontend

Install `nlohmann_json` or allow the source-tree fallback:

```bash
cmake -S . -B build \
  -DLOGIT_WITH_JSON_CONFIG=ON \
  -DLOGIT_USE_SUBMODULES=ON
```

Installed packages configured with this option use
`find_dependency(nlohmann_json 3.11 CONFIG)`. Installing a build that uses the
bundled fallback is intentionally rejected; install `nlohmann_json` separately
for an exportable package.

## Parsing

With the option enabled, `<logit.hpp>` exposes overloads for JSON text and an
already parsed `nlohmann::json` value:

```cpp
#include <logit.hpp>

const std::string text = R"json({
  "schema_version": 1,
  "loggers": [{
    "id": "application-file",
    "type": "file",
    "strategy": {
      "level": "info",
      "formatter": {"pattern": "%v", "json": false}
    },
    "backend": {
      "directory": "logs",
      "async": true,
      "use_dedicated_executor": true,
      "queue_capacity": 8192,
      "queue_policy": "block"
    }
  }]
})json";

logit::ConfigurationDocument document;
logit::ConfigurationDiagnostics diagnostics;
if (!logit::parse_configuration_json(text, document, diagnostics)) {
    // Inspect diagnostics.items().
}
```

Parsing has no logger side effects and replaces `document` only after a
complete successful conversion. The frontend reports JSON syntax and
shape/conversion errors with path-aware diagnostics and uses
`configuration_parse_failed` for syntax errors. Backend schema validation is a
later core phase: backend unknown fields, backend types, enum/named values,
defaults, and feature availability may be diagnosed by
`materialize_configuration()` rather than by the JSON conversion itself. JSON
floating-point values are rejected because schema v1 has only signed and
unsigned integer numeric values.

Call `materialize_configuration()` and `instantiate_configuration()` explicitly
for the later phases. The frontend does not read files, apply frontend-specific
defaults, resolve callbacks, or construct loggers. Properties input and hot
reload remain outside the current implementation.
