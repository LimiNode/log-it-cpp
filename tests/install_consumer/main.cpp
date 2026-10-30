#include <logit.hpp>

#if defined(LOGIT_CONSUMER_REQUIRE_JSON_CONFIG) && !defined(LOGIT_WITH_JSON_CONFIG)
#  error "The installed package did not propagate LOGIT_WITH_JSON_CONFIG"
#endif

int main() {
#if defined(LOGIT_WITH_JSON_CONFIG)
    logit::ConfigurationDocument document;
    logit::ConfigurationDiagnostics diagnostics;
    if (!logit::parse_configuration_json(
            "{\"schema_version\":1,\"loggers\":[]}", document, diagnostics)) {
        return 2;
    }
#endif
#if defined(LOGIT_CONSUMER_REQUIRE_FMT)
#  if !defined(LOGIT_WITH_FMT)
#    error "The installed package did not propagate LOGIT_WITH_FMT"
#  endif
    LOGIT_FMT_INFO("consumer fmt {}", 42);
#endif
#if LOGIT_SYSLOG_ENABLED
    LOGIT_ADD_SYSLOG_DEFAULT();
    LOGIT_INFO("hello");
#elif LOGIT_WIN_EVENT_ENABLED
    LOGIT_ADD_EVENT_LOG_DEFAULT();
    LOGIT_ERROR("hello");
#else
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_INFO("consumer works");
#endif
    LOGIT_WAIT();
    return 0;
}
