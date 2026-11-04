#include <cassert>
#include <string>

#include <logit.hpp>

namespace {

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
    assert(logit::parse_configuration_json(input, document, diagnostics));
    assert(diagnostics.empty());
    assert(document.schema_version == 1);
    assert(document.loggers.size() == 1);
    assert(document.loggers[0].id == "application-file");
    assert(document.loggers[0].strategy.has_level);
    assert(document.loggers[0].strategy.level == logit::LogLevel::LOG_LVL_INFO);
    assert(document.loggers[0].strategy.has_formatter);
    assert(document.loggers[0].strategy.formatter.has_pattern);
    assert(document.loggers[0].strategy.formatter.pattern == "%v");

    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    assert(logit::materialize_configuration(document, materializer, plan, diagnostics));
    assert(diagnostics.empty());
    assert(plan.loggers.size() == 1);
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
    assert(logit::parse_configuration_json(input, document, diagnostics));
    logit::ConfigurationFieldMap custom;
    assert(document.loggers[0].backend["custom"].get_object(custom));
    std::vector<logit::ConfigurationValue> labels;
    assert(custom["labels"].get_array(labels));
    assert(labels.size() == 1);
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
    assert(logit::parse_configuration_json(input, document, diagnostics));
    assert(document.loggers[0].backend["max_records"].type() ==
           logit::ConfigurationValue::Type::UnsignedInteger);
    assert(document.loggers[0].backend["max_age_ms"].type() ==
           logit::ConfigurationValue::Type::SignedInteger);
    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    assert(logit::materialize_configuration(document, materializer, plan, diagnostics));
}

void test_frontend_rejects_invalid_input_atomically() {
    logit::ConfigurationDocument output;
    output.schema_version = 77;
    logit::ConfigurationLoggerSpec existing;
    existing.id = "existing";
    output.loggers.push_back(existing);
    logit::ConfigurationDiagnostics diagnostics;

    assert(!logit::parse_configuration_json("{", output, diagnostics));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::ConfigurationParseFailed));
    assert(output.schema_version == 77);
    assert(output.loggers.size() == 1);

    const std::string invalid = R"json({
        "schema_version": 1,
        "unknown": true,
        "loggers": [{
            "id": "bad",
            "type": "memory",
            "backend": {"max_records": 1.5}
        }]
    })json";
    assert(!logit::parse_configuration_json(invalid, output, diagnostics));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::UnknownField));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::InvalidValue));
    assert(output.schema_version == 77);
    assert(output.loggers.size() == 1);
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
    assert(!logit::parse_configuration_json(input, document, diagnostics));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::UnknownField));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::InvalidValue));
}

} // namespace

int main() {
    test_valid_json_maps_and_materializes();
    test_nested_backend_values_are_preserved();
    test_parsed_and_programmatic_json_use_schema_integer_types();
    test_frontend_rejects_invalid_input_atomically();
    test_frontend_rejects_unknown_strategy_and_named_values();
    return 0;
}
