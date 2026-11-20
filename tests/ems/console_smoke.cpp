#include <logit.hpp>

int main() {
    logit::ConfigurationPreparedSet prepared;
    prepared.schema_version = 1;
    logit::ConfigurationPreparedLogger entry;
    entry.id = "ems-memory";
    entry.logger = std::make_shared<logit::MemoryLogger>();
    entry.formatter = std::make_shared<logit::SimpleLogFormatter>("%v");
    prepared.loggers.push_back(entry);
    std::shared_ptr<const logit::ConfigurationRegistrationResult> registered;
    logit::ConfigurationDiagnostics diagnostics;
    if (!logit::Logger::get_instance().register_configuration(
            prepared, logit::ConfigurationRegistrationOptions(), registered, diagnostics)) {
        return 2;
    }
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_TRACE("trace message");
    LOGIT_DEBUG("debug message");
    LOGIT_INFO("info message");
    LOGIT_WARN("warn message");
    LOGIT_ERROR("error message");
    LOGIT_FATAL("fatal message");
    return 0;
}
