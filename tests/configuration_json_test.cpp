#include <cstdio>
#include <cstdlib>
#include <string>

#include <logit.hpp>

namespace {

void require_impl(bool condition, const char* expression,
                  const char* file, int line) {
    if (!condition) {
        std::fprintf(stderr, "%s:%d: requirement failed: %s\n", file, line, expression);
        std::abort();
    }
}

#define require(condition) \
    require_impl(static_cast<bool>(condition), #condition, __FILE__, __LINE__)

struct EmptyBindingResolver : logit::ConfigurationBindingResolver {
    bool resolve(const logit::ConfigurationMaterializedLogger&,
                 logit::ConfigurationBindingSet&,
                 logit::ConfigurationDiagnostics&) const override {
        return true;
    }
};

void test_valid_json_maps_and_materializes() {
    const std::string input = R"json({
        "schema_version": 1,
        "loggers": [{
            "id": "application-file",
            "type": "file",
            "strategy": {
                "level": "info",
                "formatter": {"type": "simple", "pattern": "%v", "json": false}
            },
            "backend": {
                "directory": "logs",
                "async": true,
                "use_dedicated_executor": true,
                "queue_capacity": 8192,
                "queue_policy": "block",
                "compress_level": 1
            }
        }]
    })json";

    logit::ConfigurationDocument document;
    logit::ConfigurationDiagnostics diagnostics;
    require(logit::parse_configuration_json(input, document, diagnostics));
    require(diagnostics.empty());
    require(document.schema_version == 1);
    require(document.loggers.size() == 1);
    require(document.loggers[0].id == "application-file");
    require(document.loggers[0].strategy.has_level);
    require(document.loggers[0].strategy.level == logit::LogLevel::LOG_LVL_INFO);
    require(document.loggers[0].strategy.has_formatter);
    require(document.loggers[0].strategy.formatter.has_pattern);
    require(document.loggers[0].strategy.formatter.pattern == "%v");

    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    require(logit::materialize_configuration(document, materializer, plan, diagnostics));
    require(diagnostics.empty());
    require(plan.loggers.size() == 1);
}

void test_nested_backend_values_are_preserved() {
    const nlohmann::json input = {
        {"schema_version", 1},
        {"loggers", nlohmann::json::array({{
            {"id", "nested"},
            {"type", "memory"},
            {"backend", {
                {"custom", {
                    {"labels", nlohmann::json::array({{{"name", "env"}, {"value", "test"}}})}
                }}
            }}
        }})}
    };

    logit::ConfigurationDocument document;
    logit::ConfigurationDiagnostics diagnostics;
    require(logit::parse_configuration_json(input, document, diagnostics));
    logit::ConfigurationFieldMap custom;
    require(document.loggers[0].backend["custom"].get_object(custom));
    std::vector<logit::ConfigurationValue> labels;
    require(custom["labels"].get_array(labels));
    require(labels.size() == 1);
}

void test_parsed_and_programmatic_json_use_schema_integer_types() {
    const nlohmann::json input = {
        {"schema_version", 1},
        {"loggers", nlohmann::json::array({{
            {"id", "memory-integers"},
            {"type", "memory"},
            {"backend", {{"max_records", 10}, {"max_age_ms", 100}}}
        }})}
    };
    logit::ConfigurationDocument document;
    logit::ConfigurationDiagnostics diagnostics;
    require(logit::parse_configuration_json(input, document, diagnostics));
    require(document.loggers[0].backend["max_records"].type() ==
           logit::ConfigurationValue::Type::UnsignedInteger);
    require(document.loggers[0].backend["max_age_ms"].type() ==
           logit::ConfigurationValue::Type::SignedInteger);
    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    require(logit::materialize_configuration(document, materializer, plan, diagnostics));
}

void test_frontend_rejects_invalid_input_atomically() {
    logit::ConfigurationDocument output;
    output.schema_version = 77;
    logit::ConfigurationLoggerSpec existing;
    existing.id = "existing";
    output.loggers.push_back(existing);
    logit::ConfigurationDiagnostics diagnostics;

    require(!logit::parse_configuration_json("{", output, diagnostics));
    require(diagnostics.has_code(logit::ConfigurationErrorCode::ConfigurationParseFailed));
    require(output.schema_version == 77);
    require(output.loggers.size() == 1);

    const std::string invalid = R"json({
        "schema_version": 1,
        "unknown": true,
        "loggers": [{
            "id": "bad",
            "type": "memory",
            "backend": {"max_records": 1.5}
        }]
    })json";
    require(!logit::parse_configuration_json(invalid, output, diagnostics));
    require(diagnostics.has_code(logit::ConfigurationErrorCode::UnknownField));
    require(diagnostics.has_code(logit::ConfigurationErrorCode::InvalidValue));
    require(output.schema_version == 77);
    require(output.loggers.size() == 1);
}

void test_frontend_rejects_unknown_strategy_and_named_values() {
    const std::string input = R"json({
        "schema_version": 1,
        "loggers": [{
            "id": "bad-strategy",
            "type": "memory",
            "strategy": {"level": "verbose", "typo": true}
        }]
    })json";
    logit::ConfigurationDocument document;
    logit::ConfigurationDiagnostics diagnostics;
    require(!logit::parse_configuration_json(input, document, diagnostics));
    require(diagnostics.has_code(logit::ConfigurationErrorCode::UnknownField));
    require(diagnostics.has_code(logit::ConfigurationErrorCode::InvalidValue));
}

void test_json_reaches_prepared_runtime_strategy() {
    const std::string input = R"json({
        "schema_version": 1,
        "loggers": [{
            "id": "json-memory",
            "type": "memory",
            "strategy": {
                "level": "info",
                "formatter": {"pattern": "%l|%v"}
            },
            "backend": {}
        }]
    })json";
    logit::ConfigurationDocument document;
    logit::ConfigurationDiagnostics diagnostics;
    require(logit::parse_configuration_json(input, document, diagnostics));
    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    require(logit::materialize_configuration(document, materializer, plan, diagnostics));
    logit::ConfigurationPreparedSet prepared;
    EmptyBindingResolver resolver;
    logit::BuiltinConfigurationLoggerFactory factory;
    require(logit::prepare_configuration(plan, resolver, factory, prepared, diagnostics));
    require(prepared.loggers.size() == 1);
    require(prepared.loggers[0].id == "json-memory");
    require(prepared.loggers[0].level == logit::LogLevel::LOG_LVL_INFO);
    logit::LogRecord record(logit::LogLevel::LOG_LVL_INFO, 0, "json.cpp", 1,
                            "test", "payload", "", -1, false);
    require(prepared.loggers[0].formatter->format(record) == "INFO|payload");
    require(prepared.loggers[0].logger->get_log_level() == logit::LogLevel::LOG_LVL_INFO);
}

} // namespace

int main() {
    test_valid_json_maps_and_materializes();
    test_nested_backend_values_are_preserved();
    test_parsed_and_programmatic_json_use_schema_integer_types();
    test_frontend_rejects_invalid_input_atomically();
    test_frontend_rejects_unknown_strategy_and_named_values();
    test_json_reaches_prepared_runtime_strategy();
    return 0;
}
