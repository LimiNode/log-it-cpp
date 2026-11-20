# Загрузка конфигурации

Конфигурация LogIt++ разделена на явные этапы:

```text
JSON value или text
    -> ConfigurationDocument
    -> validation и materialization
    -> разрешение programmatic bindings
    -> подготовка runtime-пар (ConfigurationPreparedSet)
    -> атомарная append-only регистрация (Logger)
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
defaults, не разрешает callback'и и не создаёт logger'ы. `instantiate_configuration()`
остаётся compatibility API для backend-instantiation: factory сама владеет
level logger'а и не создаёт formatter из materialized strategy. Для атомарного
применения strategy используйте `prepare_configuration()`: он создаёт formatter
и назначает explicit level или default `TRACE`. Properties input и hot reload
пока не реализованы.

Для подготовки runtime-пар доступен `prepare_configuration()`. Он возвращает
`ConfigurationPreparedSet`: стабильный ID logger'а, конкретные `ILogger`,
`ILogFormatter` и применённый `LogLevel`. При отсутствии level используется
`TRACE`; formatter schema v1 имеет тип `simple`, defaults публичного
`SimpleLogFormatter::Config`, а затем применяет явные pattern и JSON overrides.
Prepared result использует shared ownership в соответствии с существующим
factory API. Подготовка не изменяет глобальный singleton поэлементно.

## Атомарная регистрация

`Logger::register_configuration()` добавляет весь подготовленный batch одной
публикацией dispatch snapshot, сохраняя существующие индексы и strategy flags:

```cpp
logit::ConfigurationPreparedSet prepared; // Successful prepare_configuration() output.
logit::ConfigurationRegistrationOptions options;
options["application-file"].enabled = true;
options["application-file"].single_mode = false;
std::shared_ptr<const logit::ConfigurationRegistrationResult> registered;
if (logit::Logger::get_instance().register_configuration(
        prepared, options, registered, diagnostics)) {
    const int file_index = registered->indices.at("application-file");
    logit::Logger::get_instance().set_logger_enabled(file_index, true);
}
```

Перед вызовом default-constructed `prepared` из примера должен быть заполнен
успешной подготовкой schema v1. Регистрация проверяет и вручную созданные наборы:
ID, non-null пары, level metadata, ID опций, object aliases и предел индексов.
Level и formatter повторно не применяются. Повторные ID отклоняются: это не
replacement, hot reload или идемпотентная повторная регистрация. Корректный пустой
batch возвращает пустую карту без публикации snapshot. Количество стратегий
ограничено `INT_MAX`, последний допустимый индекс — `INT_MAX - 1`.

При ошибке prepared input, живой registry и прежний result handle сохраняются;
diagnostics заменяются при каждом вызове. Исключения allocation могут выйти только
до commit с той же гарантией сохранения. Готовый результат передаётся без allocation.
При гонке с shutdown либо публикуется весь batch, который увидит shutdown, либо
регистрация завершается без изменений.

Один complete backend/formatter object не может принадлежать разным стратегиям,
включая legacy entries и предыдущие batch, независимо от concurrency capability.
Объект с двумя интерфейсами внутри одной стратегии требует общего control block.
Разные объекты с одним control block допустимы. Внешние shared owners сохраняют
lifetime, но не дают права на несинхронизированные вызовы, изменение или shutdown.
Уничтожение prepared set не удаляет зарегистрированные entries.
`Logger::add_logger(unique_ptr, unique_ptr, bool)` сохраняет прежний интерфейс
и также подготавливает snapshot до изменения registry. Reset/restart singleton
не добавлен.
