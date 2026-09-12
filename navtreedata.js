/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "LogIt++", "index.html", [
    [ "LogIt++ Library", "index.html", "index" ],
    [ "Quick start and documentation map", "quickstart.html", [
      [ "Quick start", "quickstart.html#autotoc_md0", [
        [ "Minimal application", "quickstart.html#autotoc_md1", null ],
        [ "Documentation map", "quickstart.html#autotoc_md2", null ],
        [ "Public API boundary", "quickstart.html#autotoc_md3", null ]
      ] ]
    ] ],
    [ "Installation guide", "installation.html", [
      [ "Installation guide", "installation.html#autotoc_md4", [
        [ "Requirements", "installation.html#autotoc_md5", null ],
        [ "Vendored checkout", "installation.html#autotoc_md6", null ],
        [ "Git submodule", "installation.html#autotoc_md7", null ],
        [ "Installed package", "installation.html#autotoc_md8", null ],
        [ "Optional dependencies and features", "installation.html#autotoc_md9", null ],
        [ "Install limitations", "installation.html#autotoc_md10", null ],
        [ "Package managers", "installation.html#autotoc_md11", null ]
      ] ]
    ] ],
    [ "Backend matrix", "backends.html", [
      [ "Backend matrix", "backends.html#autotoc_md12", [
        [ "Delivery models", "backends.html#autotoc_md13", null ],
        [ "Packaging", "backends.html#autotoc_md14", null ]
      ] ]
    ] ],
    [ "API reference and concepts", "api_reference.html", [
      [ "Overview", "api_reference.html#reference_overview_sec", null ],
      [ "Macro-first usage", "api_reference.html#macro_first_usage_sec", null ],
      [ "Backpressure and queue variants", "api_reference.html#backpressure_sec", null ],
      [ "Macro Examples", "api_reference.html#macro_examples_sec", [
        [ "Long-form macros", "api_reference.html#macro_examples_long", null ],
        [ "Short aliases", "api_reference.html#macro_examples_short", null ],
        [ "Diagnostic context (MDC/NDC)", "api_reference.html#macro_examples_context", null ],
        [ "Optional backends", "api_reference.html#macro_examples_optional", null ],
        [ "Targeted, conditional, and scope helpers", "api_reference.html#macro_examples_targeted", null ],
        [ "In-memory snapshot logger", "api_reference.html#macro_examples_memory", null ],
        [ "Common stored-log API", "api_reference.html#macro_examples_shared_reader", null ]
      ] ],
      [ "Features", "api_reference.html#features_sec", [
        [ "Flexible Log Formatting", "api_reference.html#flexible_formatting", null ],
        [ "Logging with Macros", "api_reference.html#macro_logging", null ],
        [ "Support for Multiple Backends", "api_reference.html#multiple_backends", null ],
        [ "Asynchronous Logging", "api_reference.html#async_logging", null ],
        [ "Queue Buffer Modes", "api_reference.html#buffer_modes", null ],
        [ "Stream-Based Logging", "api_reference.html#stream_logging", null ],
        [ "Compile-Time Log Level", "api_reference.html#compile_level", null ],
        [ "Extensibility", "api_reference.html#extensibility", null ]
      ] ],
      [ "Usage", "api_reference.html#usage_sec", null ],
      [ "Customizing Log Formats", "api_reference.html#log_formatting_sec", [
        [ "Examples of Formatting Patterns", "api_reference.html#pattern_example", [
          [ "Example for Setting a Custom Console Logger Format", "api_reference.html#autotoc_md15", null ],
          [ "Example Using Macros for Simplicity", "api_reference.html#autotoc_md16", null ]
        ] ]
      ] ],
      [ "Log Message Formatting Flags", "api_reference.html#format_flags_sec", [
        [ "Date and Time Flags", "api_reference.html#datetime_flags", null ],
        [ "Weekday and Month Names", "api_reference.html#weekday_month_flags", null ],
        [ "Log Level", "api_reference.html#log_level_flags", null ],
        [ "File and Function Information", "api_reference.html#file_function_flags", null ],
        [ "Thread Information", "api_reference.html#thread_flags", null ],
        [ "Color Formatting", "api_reference.html#color_flags", null ],
        [ "Message Content", "api_reference.html#message_flags", null ],
        [ "Alignment and Truncation", "api_reference.html#alignment_truncation_flags", null ],
        [ "Advanced Path Handling", "api_reference.html#advanced_path_handling", null ]
      ] ],
      [ "Shortened Logging Macros", "api_reference.html#short_macros", [
        [ "Available TRACE-level macros:", "api_reference.html#autotoc_md17", null ]
      ] ],
      [ "Configuration Macros", "api_reference.html#config_macros", null ],
      [ "Custom Logger Backend and Formatter", "api_reference.html#custom_backend_sec", [
        [ "Custom Logger Example", "api_reference.html#custom_logger", null ],
        [ "Custom Formatter Example", "api_reference.html#custom_formatter", null ],
        [ "Summary", "api_reference.html#summary_custom_backend", null ]
      ] ],
      [ "Installation", "api_reference.html#install_sec", [
        [ "Step 1: Clone the Repository", "api_reference.html#step1", null ],
        [ "Step 2: Include the LogIt++ Headers in Your Project", "api_reference.html#step2", null ],
        [ "Step 3: Configure Dependencies", "api_reference.html#step3", null ],
        [ "Step 4: Using fmt (Optional)", "api_reference.html#step4", null ],
        [ "CMake options", "api_reference.html#cmake_options_sec", null ]
      ] ],
      [ "Benchmarks", "api_reference.html#bench_sec", [
        [ "Step 5: Build and Run Your Project", "api_reference.html#step5", null ]
      ] ],
      [ "Repository", "api_reference.html#repo_sec", null ],
      [ "License", "api_reference.html#license_sec", null ]
    ] ],
    [ "Performance and benchmarks", "benchmarks.html", [
      [ "Performance and benchmarks", "benchmarks.html#autotoc_md18", [
        [ "Interpreting results", "benchmarks.html#autotoc_md19", null ],
        [ "Historical snapshot", "benchmarks.html#autotoc_md20", null ],
        [ "Harness details", "benchmarks.html#autotoc_md21", null ]
      ] ]
    ] ],
    [ "Comparison with other C++ logging libraries", "comparison.html", [
      [ "LogIt++ compared with other C++ logging libraries", "comparison.html#autotoc_md22", [
        [ "What LogIt++ optimizes for", "comparison.html#autotoc_md23", null ],
        [ "Feature comparison", "comparison.html#autotoc_md24", null ],
        [ "Developer ergonomics", "comparison.html#autotoc_md25", null ],
        [ "Why IceCream-Cpp is a different comparison", "comparison.html#autotoc_md26", null ],
        [ "Async delivery and queue semantics", "comparison.html#autotoc_md27", null ],
        [ "Structured data, storage, and telemetry", "comparison.html#autotoc_md28", null ],
        [ "Performance snapshot", "comparison.html#autotoc_md29", null ],
        [ "Trade-offs", "comparison.html#autotoc_md30", null ],
        [ "Which library should I choose?", "comparison.html#autotoc_md31", null ],
        [ "Sources and reproducibility", "comparison.html#autotoc_md32", null ]
      ] ]
    ] ],
    [ "OTLP/HTTP logger", "otlp_http_logger.html", [
      [ "OTLP/HTTP logger", "otlp_http_logger.html#autotoc_md33", [
        [ "CMake", "otlp_http_logger.html#autotoc_md34", null ],
        [ "Usage", "otlp_http_logger.html#autotoc_md35", null ],
        [ "OtlpPayloadLogger callback exporter", "otlp_http_logger.html#autotoc_md36", null ],
        [ "Export model", "otlp_http_logger.html#autotoc_md37", null ],
        [ "Structured typed attributes", "otlp_http_logger.html#autotoc_md38", [
          [ "Configuration flags", "otlp_http_logger.html#autotoc_md39", null ],
          [ "Type mapping", "otlp_http_logger.html#autotoc_md40", null ],
          [ "Name sanitization and deduplication", "otlp_http_logger.html#autotoc_md41", null ],
          [ "Reserved prefix", "otlp_http_logger.html#autotoc_md42", null ],
          [ "Cardinality warning", "otlp_http_logger.html#autotoc_md43", null ],
          [ "Deprecation notice", "otlp_http_logger.html#autotoc_md44", null ]
        ] ],
        [ "Diagnostics", "otlp_http_logger.html#autotoc_md45", null ],
        [ "Notes", "otlp_http_logger.html#autotoc_md46", null ]
      ] ]
    ] ],
    [ "Prometheus Logger", "prometheus_logger.html", [
      [ "Prometheus Logger", "prometheus_logger.html#autotoc_md47", [
        [ "Overview", "prometheus_logger.html#autotoc_md48", null ],
        [ "Built-in Metrics", "prometheus_logger.html#autotoc_md49", null ],
        [ "CMake Options", "prometheus_logger.html#autotoc_md50", null ],
        [ "Usage: PrometheusPayloadLogger", "prometheus_logger.html#autotoc_md51", null ],
        [ "Usage: PrometheusHttpServerLogger", "prometheus_logger.html#autotoc_md52", null ],
        [ "Custom Metrics", "prometheus_logger.html#autotoc_md53", null ],
        [ "Prometheus Scrape Config", "prometheus_logger.html#autotoc_md54", null ],
        [ "Limitations", "prometheus_logger.html#autotoc_md55", null ]
      ] ]
    ] ],
    [ "TaskExecutor Implementation Notes", "task_executor.html", [
      [ "TaskExecutor Implementation Notes", "task_executor.html#autotoc_md56", [
        [ "Implementation variants", "task_executor.html#autotoc_md57", [
          [ "Default deque worker (without LOGIT_USE_MPSC_RING)", "task_executor.html#autotoc_md58", null ],
          [ "Lock-free MPSC ring (LOGIT_USE_MPSC_RING)", "task_executor.html#autotoc_md59", null ],
          [ "Emscripten builds without pthreads", "task_executor.html#autotoc_md60", null ]
        ] ],
        [ "Backpressure semantics", "task_executor.html#autotoc_md61", null ],
        [ "Hot queue resize (LOGIT_USE_MPSC_RING)", "task_executor.html#autotoc_md62", null ],
        [ "Ordering and completion guarantees", "task_executor.html#autotoc_md63", null ],
        [ "Singleton and lifetime management", "task_executor.html#autotoc_md64", null ],
        [ "Emscripten (no pthreads)", "task_executor.html#autotoc_md65", null ],
        [ "API surface and macros", "task_executor.html#autotoc_md66", [
          [ "Examples", "task_executor.html#autotoc_md67", null ]
        ] ],
        [ "Thread-safety and TSAN considerations", "task_executor.html#autotoc_md68", null ],
        [ "Performance and tuning", "task_executor.html#autotoc_md69", null ]
      ] ]
    ] ],
    [ "Queue Back-Pressure Controls", "backpressure.html", [
      [ "Queue Back-Pressure Controls", "backpressure.html#autotoc_md70", null ]
    ] ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", "globals_defs" ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"BufferedLogEntry_8hpp.html",
"api_reference.html#color_flags",
"classlogit_1_1FileLogger.html#ae5a0e21591b65367c85f9a444851996f",
"classlogit_1_1MdbxLogger.html#a557205fabc1bffb05bee2ed6f77e6c87",
"classlogit_1_1PrometheusHttpServerLogger.html#acdd0c3afd4ff60c6ad8ae640e46262af",
"classlogit_1_1detail_1_1MdbxByteReader.html#a0a28bec2661666899600c0ef0b44b48c",
"dir_d28a4824dc47e487b107a5db32ef43c4.html",
"group__LoggingMacros.html#ga0247b77eb0e8b91aa9512b5797ab70d7",
"log__macros_8hpp.html#a7ef3045384a6d7775dd06931a0afde6f",
"namespacelogit.html#a43e16f3ddf06e535296fbb8f5b55b919",
"structlogit_1_1FormatInstruction.html#a02acd24128bb2dafba5b7edbefb21d7e",
"structlogit_1_1MdbxLogger_1_1Payload.html#a83069602fdd346c7459f2be72de32b58",
"structlogit_1_1PrometheusTextFormatConfig.html#ad889a7a1de34c62d9b3363dab742f7f2"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';