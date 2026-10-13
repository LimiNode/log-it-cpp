#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <logit.hpp>

namespace {

struct RecordingMaterializer : logit::ConfigurationMaterializer {
    mutable int validate_calls = 0;
    mutable int materialize_calls = 0;
    bool add_validation_diagnostic = false;
    bool add_materialization_diagnostic = false;
    int validation_diagnostic_at = -1;
    int fail_materialization_at = -1;

    bool validate_backend(const logit::ConfigurationLoggerSpec& logger,
                          logit::ConfigurationDiagnostics& diagnostics) const override {
        ++validate_calls;
        if (add_validation_diagnostic &&
            (validation_diagnostic_at < 0 || validate_calls == validation_diagnostic_at)) {
            diagnostics.add(logit::ConfigurationErrorCode::UnavailableBackend,
                            "type",
                            "test validation diagnostic");
        }
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
                             logit::ConfigurationDiagnostics& diagnostics) const override {
        ++materialize_calls;
        if (add_materialization_diagnostic) {
            diagnostics.add(logit::ConfigurationErrorCode::MissingProgrammaticBinding,
                            "binding",
                            "test materialization diagnostic");
        }
        if (fail_materialization_at == materialize_calls) {
            return false;
        }
        output.id = logger.id;
        output.type = logger.type;
        output.strategy = logger.strategy;
        output.backend = logger.backend;
        return true;
    }
};

struct RecordingBindingResolver : logit::ConfigurationBindingResolver {
    bool provide_binding = true;
    bool provide_null_binding = false;
    bool provide_wrong_type = false;
    int missing_at = -1;
    mutable int calls = 0;

    bool resolve(const logit::ConfigurationMaterializedLogger& logger,
                 logit::ConfigurationBindingSet& bindings,
                 logit::ConfigurationDiagnostics&) const override {
        ++calls;
        if (provide_binding && !logger.required_bindings.empty() &&
            (missing_at < 0 || calls != missing_at)) {
            if (provide_null_binding) {
                bindings.set("on_payload", std::shared_ptr<int>());
            } else if (provide_wrong_type) {
                bindings.set("on_payload", std::shared_ptr<std::string>(new std::string("wrong")));
            } else {
                bindings.set("on_payload", std::shared_ptr<int>(new int(1)));
            }
        }
        return true;
    }
};

struct RecordingLoggerFactory : logit::ConfigurationLoggerFactory {
    mutable int calls = 0;
    int fail_at = -1;

    std::shared_ptr<logit::ILogger> instantiate(
        const logit::ConfigurationMaterializedLogger&,
        const logit::ConfigurationBindingSet& bindings,
        logit::ConfigurationDiagnostics&) const override {
        ++calls;
        (void)bindings.get<int>("on_payload");
        if (fail_at == calls) return std::shared_ptr<logit::ILogger>();
        return std::shared_ptr<logit::ILogger>(new logit::MemoryLogger());
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

void test_nested_values_are_representable() {
    std::vector<logit::ConfigurationValue> routes;
    routes.push_back(logit::ConfigurationValue::make_object(
        std::map<std::string, logit::ConfigurationValue>{{
            "min_level", logit::ConfigurationValue::make_string("info")
        }}));
    const logit::ConfigurationValue value = logit::ConfigurationValue::make_array(routes);
    std::vector<logit::ConfigurationValue> observed;
    assert(value.get_array(observed));
    assert(observed.size() == 1);
    std::map<std::string, logit::ConfigurationValue> observed_route;
    assert(observed[0].get_object(observed_route));
    assert(observed_route["min_level"].type() ==
           logit::ConfigurationValue::Type::String);
}

void test_diagnostics_fail_closed() {
    const logit::ConfigurationDocument document = valid_document();

    RecordingMaterializer validation_diagnostic;
    validation_diagnostic.add_validation_diagnostic = true;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics diagnostics;
    assert(!logit::materialize_configuration(document,
                                              validation_diagnostic,
                                              plan,
                                              diagnostics));
    assert(validation_diagnostic.materialize_calls == 0);
    assert(!diagnostics.empty());

    RecordingMaterializer materialization_diagnostic;
    materialization_diagnostic.add_materialization_diagnostic = true;
    assert(!logit::materialize_configuration(document,
                                              materialization_diagnostic,
                                              plan,
                                              diagnostics));
    assert(!diagnostics.empty());
}

void test_plan_is_preserved_on_late_failure() {
    logit::ConfigurationDocument document = valid_document();
    document.loggers.push_back(document.loggers[0]);
    document.loggers[1].id = "file-secondary";

    RecordingMaterializer materializer;
    materializer.fail_materialization_at = 2;
    logit::ConfigurationPlan plan;
    plan.schema_version = 77;
    logit::ConfigurationMaterializedLogger sentinel;
    sentinel.id = "existing";
    plan.loggers.push_back(sentinel);
    logit::ConfigurationDiagnostics diagnostics;
    assert(!logit::materialize_configuration(document, materializer, plan, diagnostics));
    assert(plan.schema_version == 77);
    assert(plan.loggers.size() == 1);
    assert(plan.loggers[0].id == "existing");
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

void test_builtin_materializer_applies_file_defaults() {
    logit::ConfigurationDocument document;
    document.schema_version = 1;
    logit::ConfigurationLoggerSpec logger;
    logger.id = "file-defaults";
    logger.type = "file";
    document.loggers.push_back(logger);

    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics diagnostics;
    assert(logit::materialize_configuration(document, materializer, plan, diagnostics));
    assert(diagnostics.empty());

    std::string directory;
    std::uint64_t queue_capacity = 1;
    bool async = false;
    assert(plan.loggers[0].backend.at("directory").get_string(directory));
    assert(directory == "logs");
    assert(plan.loggers[0].backend.at("async").get_boolean(async));
    assert(async);
    assert(plan.loggers[0].backend.at("queue_capacity").get_unsigned(queue_capacity));
    assert(queue_capacity == 0);
}

void test_builtin_materializer_rejects_unknown_field() {
    logit::ConfigurationDocument document;
    document.schema_version = 1;
    logit::ConfigurationLoggerSpec logger;
    logger.id = "memory-invalid";
    logger.type = "memory";
    logger.backend["not_a_memory_option"] = logit::ConfigurationValue::make_boolean(true);
    document.loggers.push_back(logger);

    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics diagnostics;
    assert(!logit::materialize_configuration(document, materializer, plan, diagnostics));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::UnknownField));
}

void test_builtin_materializer_handles_optional_backends_explicitly() {
    logit::ConfigurationDocument document;
    document.schema_version = 1;
    logit::ConfigurationLoggerSpec logger;
    logger.id = "mdbx-or-optional";
    logger.type = "mdbx";
    document.loggers.push_back(logger);

    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics diagnostics;
    const bool materialized =
        logit::materialize_configuration(document, materializer, plan, diagnostics);
#if defined(LOGIT_WITH_MDBX)
    assert(materialized);
    assert(diagnostics.empty());
    assert(plan.loggers[0].backend.at("path").type() ==
           logit::ConfigurationValue::Type::String);
#else
    assert(!materialized);
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::UnavailableBackend));
#endif
}

void test_materializer_diagnostics_are_rebased_to_logger_index() {
    logit::ConfigurationDocument document = valid_document();
    document.loggers.push_back(document.loggers[0]);
    document.loggers[1].id = "file-secondary";

    RecordingMaterializer materializer;
    materializer.add_validation_diagnostic = true;
    materializer.validation_diagnostic_at = 2;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics diagnostics;
    assert(!logit::materialize_configuration(document, materializer, plan, diagnostics));
    assert(diagnostics.size() == 1);
    assert(diagnostics.items()[0].path == "loggers[1].type");
}

void test_callback_backends_record_required_bindings_when_available() {
#if defined(LOGIT_WITH_OTLP)
    logit::ConfigurationDocument document;
    document.schema_version = 1;
    logit::ConfigurationLoggerSpec logger;
    logger.id = "otlp-payload";
    logger.type = "otlp_payload";
    document.loggers.push_back(logger);

    logit::BuiltinConfigurationMaterializer materializer;
    logit::ConfigurationPlan plan;
    logit::ConfigurationDiagnostics diagnostics;
    assert(logit::materialize_configuration(document, materializer, plan, diagnostics));
    assert(diagnostics.empty());
    assert(plan.loggers[0].required_bindings.size() == 1);
    assert(plan.loggers[0].required_bindings[0] == "on_payload");
#endif
}

void test_optional_format_defaults_merge_recursively() {
#if defined(LOGIT_WITH_OTLP)
    {
        logit::ConfigurationDocument document;
        document.schema_version = 1;
        logit::ConfigurationLoggerSpec logger;
        logger.id = "otlp-partial-format";
        logger.type = "otlp_http";
        std::map<std::string, logit::ConfigurationValue> format;
        format["service_name"] = logit::ConfigurationValue::make_string("orders");
        logger.backend["format"] = logit::ConfigurationValue::make_object(format);
        document.loggers.push_back(logger);

        logit::BuiltinConfigurationMaterializer materializer;
        logit::ConfigurationPlan plan;
        logit::ConfigurationDiagnostics diagnostics;
        assert(logit::materialize_configuration(document, materializer, plan, diagnostics));
        std::map<std::string, logit::ConfigurationValue> observed;
        assert(plan.loggers[0].backend["format"].get_object(observed));
        std::string service_name;
        bool include_source = false;
        assert(observed["service_name"].get_string(service_name));
        assert(service_name == "orders");
        assert(observed["include_source"].get_boolean(include_source));
        assert(include_source);
    }
#endif
#if defined(LOGIT_WITH_PROMETHEUS) || defined(LOGIT_WITH_PROMETHEUS_SERVER)
    {
        logit::ConfigurationDocument document;
        document.schema_version = 1;
        logit::ConfigurationLoggerSpec logger;
        logger.id = "prometheus-partial-format";
#if defined(LOGIT_WITH_PROMETHEUS_SERVER)
        logger.type = "prometheus_http";
#else
        logger.type = "prometheus_payload";
        logger.backend["emit_on_log"] = logit::ConfigurationValue::make_boolean(true);
#endif
        std::map<std::string, logit::ConfigurationValue> format;
        format["metric_prefix"] = logit::ConfigurationValue::make_string("app_");
        logger.backend["format"] = logit::ConfigurationValue::make_object(format);
        document.loggers.push_back(logger);

        logit::BuiltinConfigurationMaterializer materializer;
        logit::ConfigurationPlan plan;
        logit::ConfigurationDiagnostics diagnostics;
        assert(logit::materialize_configuration(document, materializer, plan, diagnostics));
        std::map<std::string, logit::ConfigurationValue> observed;
        assert(plan.loggers[0].backend["format"].get_object(observed));
        std::string metric_prefix;
        bool include_help = false;
        assert(observed["metric_prefix"].get_string(metric_prefix));
        assert(metric_prefix == "app_");
        assert(observed["include_help"].get_boolean(include_help));
        assert(include_help);
#if !defined(LOGIT_WITH_PROMETHEUS_SERVER)
        bool emit_on_log = false;
        bool emit_on_wait = false;
        assert(plan.loggers[0].backend["emit_on_log"].get_boolean(emit_on_log));
        assert(plan.loggers[0].backend["emit_on_wait"].get_boolean(emit_on_wait));
        assert(emit_on_log);
        assert(emit_on_wait);
#endif

        std::map<std::string, logit::ConfigurationValue> invalid_label;
        invalid_label["name"] = logit::ConfigurationValue::make_string("env");
        invalid_label["value"] = logit::ConfigurationValue::make_string("prod");
        invalid_label["typo"] = logit::ConfigurationValue::make_unsigned(1);
        std::vector<logit::ConfigurationValue> labels;
        labels.push_back(logit::ConfigurationValue::make_object(invalid_label));
        document.loggers[0].backend["format"] =
            logit::ConfigurationValue::make_object(
                std::map<std::string, logit::ConfigurationValue>{
                    {"const_labels", logit::ConfigurationValue::make_array(labels)}});
        diagnostics = logit::ConfigurationDiagnostics();
        assert(!logit::materialize_configuration(document, materializer, plan, diagnostics));
        assert(diagnostics.has_code(logit::ConfigurationErrorCode::UnknownField));
    }
#endif
}

void test_runtime_binding_and_instantiation_are_atomic() {
    logit::ConfigurationPlan plan;
    plan.schema_version = 1;
    logit::ConfigurationMaterializedLogger first;
    first.id = "callback-entry";
    first.required_bindings.push_back("on_payload");
    plan.loggers.push_back(first);

    RecordingBindingResolver resolver;
    RecordingLoggerFactory factory;
    std::vector<std::shared_ptr<logit::ILogger>> loggers;
    std::shared_ptr<logit::ILogger> sentinel(new logit::MemoryLogger());
    loggers.push_back(sentinel);
    logit::ConfigurationDiagnostics diagnostics;
    assert(logit::instantiate_configuration(plan, resolver, factory, loggers, diagnostics));
    assert(diagnostics.empty());
    assert(loggers.size() == 1);
    assert(loggers[0] != sentinel);

    resolver.provide_binding = false;
    loggers.clear();
    loggers.push_back(sentinel);
    assert(!logit::instantiate_configuration(plan, resolver, factory, loggers, diagnostics));
    assert(diagnostics.items()[0].path == "loggers[0].binding.on_payload");
    assert(loggers.size() == 1);
    assert(loggers[0] == sentinel);

    resolver.provide_binding = true;
    resolver.provide_null_binding = true;
    factory.calls = 0;
    assert(!logit::instantiate_configuration(plan, resolver, factory, loggers, diagnostics));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::MissingProgrammaticBinding));
    assert(factory.calls == 0);

    resolver.provide_null_binding = false;
    resolver.provide_wrong_type = true;
    logit::ConfigurationBindingSet bindings;
    bindings.set("value", std::shared_ptr<int>(new int(1)));
    assert(bindings.get<int>("value"));
    assert(!bindings.get<std::string>("value"));
    resolver.provide_wrong_type = false;

    resolver.provide_binding = true;
    factory.calls = 0;
    logit::ConfigurationMaterializedLogger second = first;
    second.id = "second";
    plan.loggers.push_back(second);
    resolver.calls = 0;
    resolver.missing_at = 2;
    assert(!logit::instantiate_configuration(plan, resolver, factory, loggers, diagnostics));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::MissingProgrammaticBinding));
    assert(factory.calls == 0);
    assert(loggers.size() == 1);
    assert(loggers[0] == sentinel);

    resolver.provide_binding = true;
    resolver.missing_at = -1;
    factory.fail_at = 2;
    assert(!logit::instantiate_configuration(plan, resolver, factory, loggers, diagnostics));
    assert(diagnostics.has_code(logit::ConfigurationErrorCode::InstantiationFailed));
}

} // namespace

int main() {
    test_validation_and_materialization();
    test_structural_diagnostics();
    test_nested_values_are_representable();
    test_diagnostics_fail_closed();
    test_plan_is_preserved_on_late_failure();
    test_materializer_is_not_called_after_validation_failure();
    test_builtin_materializer_applies_file_defaults();
    test_builtin_materializer_rejects_unknown_field();
    test_builtin_materializer_handles_optional_backends_explicitly();
    test_materializer_diagnostics_are_rebased_to_logger_index();
    test_callback_backends_record_required_bindings_when_available();
    test_optional_format_defaults_merge_recursively();
    test_runtime_binding_and_instantiation_are_atomic();
    return 0;
}
