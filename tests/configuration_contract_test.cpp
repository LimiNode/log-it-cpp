#include <cassert>
#include <string>

#include <logit.hpp>

namespace {

struct RecordingMaterializer : logit::ConfigurationMaterializer {
    mutable int validate_calls = 0;
    mutable int materialize_calls = 0;

    bool validate_backend(const logit::ConfigurationLoggerSpec& logger,
                          logit::ConfigurationDiagnostics& diagnostics) const override {
        ++validate_calls;
        if (logger.type == "file" && logger.backend.find("directory") == logger.backend.end()) {
            diagnostics.add(logit::ConfigurationErrorCode::UnknownField,
                            "loggers[0].backend",
                            "test materializer requires directory");
            return false;
        }
        return true;
    }

    bool materialize_backend(const logit::ConfigurationLoggerSpec& logger,
                             logit::ConfigurationMaterializedLogger& output,
                             logit::ConfigurationDiagnostics&) const override {
        ++materialize_calls;
        output.id = logger.id;
        output.type = logger.type;
        output.strategy = logger.strategy;
        output.backend = logger.backend;
        return true;
    }
};

logit::ConfigurationDocument valid_document() {
    logit::ConfigurationDocument document;
    document.schema_version = 1;
    logit::ConfigurationLoggerSpec logger;
    logger.id = "file-main";
    logger.type = "file";
    logger.strategy.has_level = true;
    logger.strategy.level = logit::LogLevel::LOG_LVL_INFO;
    logger.backend["directory"] = logit::ConfigurationValue::make_string("logs");
    logger.backend["async"] = logit::ConfigurationValue::make_boolean(true);
    logger.backend["use_dedicated_executor"] =
        logit::ConfigurationValue::make_boolean(true);
    logger.backend["queue_capacity"] =
        logit::ConfigurationValue::make_unsigned(8192);
    logger.backend["queue_policy"] =
        logit::ConfigurationValue::make_string("block");
    document.loggers.push_back(logger);
    return document;
}

void test_validation_and_materialization() {
    const logit::ConfigurationDocument document = valid_document();
    const logit::ConfigurationDiagnostics diagnostics = logit::validate_configuration(document);
    assert(diagnostics.empty());

    RecordingMaterializer materializer;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics materialization_diagnostics;
    assert(logit::materialize_configuration(document,
                                             materializer,
                                             plan,
                                             materialization_diagnostics));
    assert(materialization_diagnostics.empty());
    assert(materializer.validate_calls == 1);
    assert(materializer.materialize_calls == 1);
    assert(plan.loggers.size() == 1);
    assert(plan.loggers[0].id == "file-main");
}

void test_structural_diagnostics() {
    logit::ConfigurationDocument document = valid_document();
    document.schema_version = 2;
    document.loggers[0].backend.erase("use_dedicated_executor");

    logit::ConfigurationLoggerSpec duplicate = document.loggers[0];
    document.loggers.push_back(duplicate);
    const logit::ConfigurationDiagnostics diagnostics = logit::validate_configuration(document);
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::UnsupportedSchemaVersion));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::DuplicateLoggerId));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::UnsupportedCombination));
}

void test_materializer_is_not_called_after_validation_failure() {
    logit::ConfigurationDocument document = valid_document();
    document.loggers[0].backend.erase("use_dedicated_executor");

    RecordingMaterializer materializer;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics diagnostics;
    assert(!logit::materialize_configuration(document, materializer, plan, diagnostics));
    assert(materializer.validate_calls == 0);
    assert(materializer.materialize_calls == 0);
}

} // namespace

int main() {
    test_validation_and_materialization();
    test_structural_diagnostics();
    test_materializer_is_not_called_after_validation_failure();
    return 0;
}
