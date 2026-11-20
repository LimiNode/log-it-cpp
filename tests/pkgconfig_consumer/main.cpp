#include <logit.hpp>

#if defined(LOGIT_CONSUMER_REQUIRE_JSON_CONFIG) && !defined(LOGIT_WITH_JSON_CONFIG)
#  error "The pkg-config package did not propagate LOGIT_WITH_JSON_CONFIG"
#endif

int main() {
    logit::ConfigurationPreparedSet prepared;
    prepared.schema_version = 1;
    std::shared_ptr<const logit::ConfigurationRegistrationResult> registered;
    logit::ConfigurationDiagnostics registration_diagnostics;
    if (!logit::Logger::get_instance().register_configuration(
            prepared, logit::ConfigurationRegistrationOptions(), registered,
            registration_diagnostics) || !registered || !registered->indices.empty()) {
        return 3;
    }
#if defined(LOGIT_WITH_JSON_CONFIG)
    logit::ConfigurationDocument document;
    logit::ConfigurationDiagnostics diagnostics;
    if (!logit::parse_configuration_json(
            "{\"schema_version\":1,\"loggers\":[]}", document, diagnostics)) {
        return 2;
    }
#endif
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_INFO("pkg-config consumer works");
    LOGIT_WAIT();
    return 0;
}
