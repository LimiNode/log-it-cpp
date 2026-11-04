# Загрузка конфигурации

Конфигурация LogIt++ разделена на явные этапы:

```text
JSON value или text
    -> ConfigurationDocument
    -> validation и materialization
    -> разрешение programmatic bindings
    -> создание runtime logger'ов
```

Основная semantic model и runtime-слои не зависят от JSON-библиотеки. JSON
frontend является optional и использует `nlohmann::json` только при включённой
опции `LOGIT_WITH_JSON_CONFIG=ON`.

## Включение JSON frontend

Установите `nlohmann_json` либо разрешите fallback из дерева исходников:

```bash
cmake -S . -B build \
  -DLOGIT_WITH_JSON_CONFIG=ON \
  -DLOGIT_USE_SUBMODULES=ON
```

Установленный пакет с этой опцией использует
`find_dependency(nlohmann_json 3.11 CONFIG)`. Установка сборки с bundled
fallback намеренно запрещена: для экспортируемого пакета установите
`nlohmann_json` отдельно.

## Разбор JSON

При включённой опции `<logit.hpp>` предоставляет overload'ы для JSON-текста и
уже разобранного значения `nlohmann::json`:

```cpp
#include <logit.hpp>

const std::string text = R"json({
  "schema_version": 1,
  "loggers": [{
    "id": "application-file",
    "type": "file",
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
    // Обработайте diagnostics.items().
}
```

Разбор не создаёт logger'ы и заменяет `document` только после полного успеха.
Frontend возвращает path-aware diagnostics для JSON syntax и shape/conversion
ошибок; синтаксические ошибки используют категорию
`configuration_parse_failed`. Backend schema validation выполняется отдельной
core-фазой: unknown fields, backend types, enum/named values, defaults и
feature availability могут быть диагностированы `materialize_configuration()`,
а не JSON conversion. Floating-point значения JSON отклоняются, поскольку в
schema v1 предусмотрены только signed и unsigned integer значения.

Следующие этапы вызываются явно через `materialize_configuration()` и
`instantiate_configuration()`. Frontend не читает файлы, не вводит собственные
defaults, не разрешает callback'и и не создаёт logger'ы. Properties input и hot
reload пока не реализованы.
