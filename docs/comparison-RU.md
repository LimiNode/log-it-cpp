# Сравнение LogIt++ с другими C++-библиотеками логирования

Эта страница — сравнение архитектуры и возможностей, а не универсальный
рейтинг «кто лучше». Она отвечает на практический вопрос: **когда LogIt++
подходит лучше компактного string-oriented логгера, общего event framework или
диагностической macro-утилиты?**

Сравнение проверено **10.09.2026** по следующим upstream-релизам:

| Проект | Проверенная версия | Основная документация |
| --- | --- | --- |
| LogIt++ | `1.0.2-dev` (`main`) | [документация проекта](https://liminode.github.io/log-it-cpp/) |
| spdlog | [`v1.17.0`](https://github.com/gabime/spdlog/releases/tag/v1.17.0) | [README](https://github.com/gabime/spdlog), [асинхронное логирование](https://github.com/gabime/spdlog/wiki/Asynchronous-logging) |
| Quill | [`v13.0.0`](https://github.com/odygrd/quill/releases/tag/v13.0.0) | [документация проекта](https://quillcpp.readthedocs.io/) |
| Boost.Log | [Boost `1.92.0`](https://www.boost.org/users/history/version_1_92_0.html) | [документация Boost.Log](https://www.boost.org/doc/libs/1_92_0/libs/log/doc/html/) |
| glog | [`v0.7.1`](https://github.com/google/glog/releases/tag/v0.7.1) | [документация glog](https://google.github.io/glog/) |
| IceCream-Cpp | [`v1.0.0`](https://github.com/renatoGarcia/icecream-cpp/releases/tag/v1.0.0) | [README проекта](https://github.com/renatoGarcia/icecream-cpp) |

Upstream-проекты развиваются независимо. Перед принятием решения о внедрении
нужно повторно проверить версии и ссылки.

## На что оптимизирован LogIt++

LogIt++ строится вокруг диагностической записи, а не только готовой строки.
Обычный macro-вызов может сохранить значения и имена аргументов для форматтеров,
memory/storage-бэкендов, callback-ов и telemetry exporters. Тот же macro-first
API покрывает условное логирование, throttling, scope timers, tags, MDC/NDC и
маршрутизацию в конкретный logger.

Это удобно, когда логирование является частью диагностического data flow
приложения. Если требуется только минимальная стоимость вывода заранее
сформатированной строки, более узкий логгер может оказаться подходящим выбором.

## Сравнение возможностей

Таблица описывает основную поставку и документированный API проверенных версий,
а не все возможности, которые можно получить через сторонние adapters.

- **Built in** — возможность прямо документирована в библиотеке;
- **Different model** — похожий результат достигается другой абстракцией;
- **Extension** — обычно нужен adapter, custom sink или код приложения;
- `—` — это не основная цель проекта, а не утверждение о невозможности
  реализовать такую функцию.

| Возможность | LogIt++ | spdlog | Quill | Boost.Log | glog | IceCream-Cpp |
| --- | --- | --- | --- | --- | --- | --- |
| Macro-first instrumentation | Built in | Встроенные severity macros | Built in macros | Встроенные macros над моделью records/streams | Built in macros | Основная функция |
| Structured records / attributes | Built in `LogRecord` и values | Formatted messages и MDC (только synchronous logging); другая record model | Встроенные named-value logging, JSON output, tags и MDC; другая record model | Built in attributes/events | Extension / message-centric | Диагностические values, не sink record |
| Захват имён аргументов | Built in | — | Built in через `LOGV_*` | — | — | Основная функция |
| Форматирование в стиле `{fmt}` | Built in при `LOGIT_WITH_FMT=ON` | Built in | Built in | Extension / предварительное форматирование | Extension / предварительное форматирование | — |
| Логирование в стиле `printf` | Built in | Extension / предварительное форматирование | Extension / предварительное форматирование | Extension / предварительное форматирование | Built in через низкоуровневый `RAW_LOG` | — |
| Логирование в стиле stream | Built in | Extension / предварительное форматирование | Extension / предварительное форматирование | Built in | Built in | Диагностический вывод, не stream API |
| Условное логирование | Built in | Нет специального helper-а; используется условие приложения | Нет специального helper-а; используется условие приложения | Filters и predicates; нет аналогичного call-site macro | Built in через `LOG_IF` | Configuration helpers, не logging framework |
| Rate-limited logging | Built in | Нет специального семейства macros | Built in через `LOG_*_LIMIT` и `LOGV_*_LIMIT` | Extension / custom filter | Built in через `LOG_EVERY_N`, `LOG_FIRST_N` и связанные macros | — |
| Асинхронная очередь | Built in | Built in | Основная архитектура | Built in через asynchronous sink frontends | Не основная модель | — |
| Настраиваемый overflow/backpressure | Built in (`Block`, `DropNewest`, `DropOldest`) | Built in overflow policies | Встроенные bounded/unbounded и blocking/dropping queue modes | Built in через bounded async sink queue strategies (`drop_on_overflow`, `block_on_overflow`) | — | — |
| Rotating file sink | Built in | Built in | Built in через `RotatingFileSink` | Built in sink types | Встроенные file rollover/cleanup; другая модель | — |
| In-memory history и read-back | Built in для выбранных backend-ов | Встроенный backtrace buffer; нет аналогичного read-back API | Встроенный backtrace logging; нет аналогичного read-back API | Extension / custom sink | — | — |
| Live subscriptions | Built in для выбранных backend-ов | Встроенный callback sink; другая модель | Extension / custom sink | Extension / custom sink | — | — |
| Встроенное queryable structured storage | Built in через опциональный MDBX backend | Extension | Extension | Extension | — | — |
| Экспорт логов через OTLP / OpenTelemetry | Опциональный встроенный backend | Extension / adapter | Extension / custom sink | Extension / adapter | Extension / adapter | — |
| Prometheus metrics | Опциональные встроенные backend-ы | Extension / adapter | Built in через `PrometheusSink` | Extension / adapter | Extension / adapter | — |
| Header-only integration | Built in | Поддерживаемый режим | CMake/library integration | Compiled Boost component | CMake/library integration | Header-oriented utility |

Слово «встроенный» используется намеренно узко. Все эти проекты можно соединять
с другими sink-ами и telemetry-системами кодом приложения, но это не делает такую
интеграцию частью основной поставки.

## Удобство API и syntactic sugar

LogIt++ сознательно предоставляет большую регулярную поверхность macros:

```cpp
#include <logit.hpp>

LOGIT_INFO("Connected", host, port);
LOGIT_WARN_ONCE("configuration missing");
LOGIT_INFO_EVERY_N(100, "processed", count);
LOGIT_ERROR_THROTTLE(1000, "connection still unavailable");
LOGIT_SCOPE_INFO("load_database");
LOGIT_INFO_TAG(({{"order_id", order_id}, {"symbol", symbol}}), "order sent");
```

Первый вызов может сохранить имена и значения `host` и `port` — в зависимости от
выбранного macro и build options. Это удобнее для диагностики, но требует больше
работы, чем минимальный вызов с готовой строкой.

У spdlog, Quill и glog тоже есть полезные macro families, однако их основные
абстракции — logger calls, formatted messages или severity/check macros. Boost.Log
предоставляет logging macros над своей моделью records, attributes, filters и
sinks, а не единый широкий macro facade. IceCream-Cpp ближе всего как отдельная
introspection-утилита; Quill также захватывает имена исходных переменных через
logging macros `LOGV_*`.

## Почему IceCream-Cpp — особый случай

IceCream-Cpp — утилита для diagnostic printing и introspection. Она полезна,
когда разработчику нужен короткий вызов, показывающий выражение и его значение:

```cpp
// Диагностический вывод в стиле IceCream-Cpp
IC(x, user_id);
```

IceCream-Cpp не пытается предоставлять sinks, asynchronous queues, file rotation,
retention/read-back, subscriptions, OTLP или Prometheus backends. Поэтому здесь
имеет смысл сравнивать ergonomics и захват имён аргументов, а не количество sink-ов.
IceCream-Cpp может использоваться вместе с настоящим logger-ом.

## Асинхронная доставка и очереди

LogIt++ поддерживает synchronous и asynchronous режимы, общий и dedicated
executor, размер очереди и явные overflow policies. Для deque и MPSC намеренно
задокументирована разная семантика: в MPSC `DropOldest` отбрасывает входящую
задачу, сохраняя порядок уже принятых задач.

spdlog и Quill ближе всего для сравнений асинхронной производительности и настройки
очереди. Их topology workers, options и overflow semantics нельзя автоматически
считать эквивалентными LogIt++. Boost.Log предоставляет asynchronous sink
frontends, включая bounded FIFO queues со стратегиями drop-on-overflow и
block-on-overflow, через более общий sink configuration model. glog и IceCream-Cpp
не являются прямыми аналогами этой queue architecture.

## Structured data, storage и telemetry

LogIt++ особенно уместен, когда log record нужен после самого вызова:

- `MemoryLogger` поддерживает snapshots, readers и subscribers;
- `FileLogger` поддерживает перечисление сохранённых файлов и чтение их текста;
- `MdbxLogger` сохраняет structured records для последующих запросов;
- OTLP exporters передают выбранные structured attributes и context;
- Prometheus backends публикуют application и built-in metrics;
- MDC/NDC, tags и argument values проходят через одну record model.

Ближайший архитектурный аналог здесь — Boost.Log с его extensible records,
attributes, filters и sinks. spdlog обычно проще для formatted messages и
high-throughput sinks. Quill сочетает этот performance-фокус с named-value macros,
JSON output, tags и MDC в рамках собственной record model. glog сфокусирован на
application diagnostics и severity/check macros. Это trade-offs области
применения, а не универсальный рейтинг библиотек.

## Снимок производительности

Сейчас в репозитории есть воспроизводимый adapter только для LogIt++ и spdlog,
а не для всех шести проектов. Поэтому таблица — **снимок LogIt++/spdlog
pipeline**, а не рейтинг всех библиотек.

| Режим | Sink | LogIt++ p50 | LogIt++ throughput | spdlog p50 | spdlog throughput |
| --- | --- | ---: | ---: | ---: | ---: |
| Sync | Null | 119 ns | 2 127 704 msg/s | 86 ns | 5 803 783 msg/s |
| Sync | File | 130 ns | 1 035 690 msg/s | 87 ns | 1 593 987 msg/s |
| Async | Null | 20 916 ns | 1 846 272 msg/s | 1 248 779 ns | 1 303 573 msg/s |
| Async | File | 255 323 ns | 651 384 msg/s | 5 001 140 ns | 1 153 976 msg/s |

Условия snapshot: Release build, четыре producer-а, сообщения по 200 байт,
`LOGIT_BENCH_TOTAL=10000`, fixture от 05.12.2025. Путь LogIt++ может включать
извлечение имён аргументов и упаковку structured values, а spdlog adapter получает
готовую строку. Async-результаты также включают enqueue, wake-up/scheduling worker-а
и работу sink-а.

Методика описана в [`docs/benchmarks.md`](benchmarks.html), полный fixture — в
[`latency-2025-12-05-10k.csv`](https://github.com/LimiNode/log-it-cpp/blob/main/bench/results/latency-2025-12-05-10k.csv).
Harness пока не измеряет allocations на сообщение, binary size, compile time или
сопоставимые сценарии Quill/Boost.Log/glog; эти цифры здесь не выдумываются.

## Trade-offs

- поверхность macros намеренно широкая;
- захват имён аргументов и structured records могут стоить дороже минимального
  вызова с готовой строкой;
- OTLP, Prometheus HTTP server и MDBX требуют C++17;
- optional dependencies и installed-package composition требуют настроек CMake;
- публичный API использует aggregate-first umbrella headers, а не обещает
  standalone-включение каждого leaf-заголовка;
- приоритет — диагностическая насыщенность, storage и telemetry, а не абсолютный
  минимум overhead одного logging call.

## Как выбрать библиотеку

**Выбирайте LogIt++, если:**

- нужен один macro-first API для console, file, memory, storage и telemetry;
- важны structured values, захват имён аргументов, tags, MDC/NDC или read-back;
- важны queue capacity, overflow policy, dedicated executors и поведение разных
  backend-ов;
- подходит header-only C++11 core с отдельными C++17 integrations.

**Рассмотрите spdlog, если:**

- главное — сфокусированный быстрый pipeline для formatted messages;
- подходят его sinks, async queue design или ecosystem;
- хранить structured diagnostic records не требуется.

**Рассмотрите Quill, если:**

- главное — asynchronous low-latency logging pipeline;
- его named-value macros, JSON output, tags, MDC и Prometheus metrics покрывают
  требования к structured data;
- не нужны persistent structured storage, программный read-back и live
  subscriptions в модели LogIt++.

**Рассмотрите Boost.Log, если:**

- важнее общий framework attributes/filters/sinks, чем компактный macro facade;
- проект уже использует Boost и нужен его extensibility model.

**Рассмотрите glog, если:**

- нужны прежде всего Google-style severity, check и diagnostic macros;
- узкая application logging model предпочтительнее storage и telemetry backend-ов.

На момент проверки 10.09.2026
[upstream-репозиторий `google/glog`](https://github.com/google/glog) был
архивирован и переведён в режим read-only. Учитывайте этот lifecycle-статус
перед добавлением glog как новой зависимости.

**Рассмотрите IceCream-Cpp, если:**

- нужен лёгкий introspection исходных выражений во время разработки;
- logger/sink/retention framework не нужен. IceCream-Cpp может дополнять, а не
  заменять, одну из logging libraries выше.

## Источники и воспроизводимость

Ссылки на версии и upstream-документацию находятся в английской canonical-версии
этой страницы. Для поведения LogIt++ используйте guides, generated API reference,
examples и benchmark fixture репозитория. Перед performance decision повторяйте
измерения на целевых compiler, OS, hardware, sink, queue configuration и workload.
