#include <logit.hpp>

int main() {
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
