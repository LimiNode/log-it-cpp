#include "configuration_registration_test_support.hpp"

namespace {

void registration_wins() {
    auto& logger = logit::Logger::get_instance();
    auto value = registration_test::batch("shutdown-batch-a");
    value.loggers.push_back(registration_test::entry("shutdown-batch-b"));
    auto first = registration_test::probe(value.loggers[0]);
    auto second = registration_test::probe(value.loggers[1]);
    registration_test::Gate committed;
    // Destruction of the displaced result runs after publication and unlock,
    // keeping registration in flight while shutdown observes the committed batch.
    registration_test::Result output(new logit::ConfigurationRegistrationResult(),
        [&committed](const logit::ConfigurationRegistrationResult* previous) {
            delete previous;
            committed.arrive_and_wait();
        });
    bool registered = false;
    logit::ConfigurationDiagnostics diagnostics;
    std::thread registration([&]() { registered = registration_test::append(value, output, diagnostics); });
    committed.wait_until_entered();
    logger.shutdown();
    REG_REQUIRE(first->shutdowns == 1 && second->shutdowns == 1);
    REG_REQUIRE(first->observed_shutdown_count == 2 && second->observed_shutdown_count == 2);
    committed.release();
    registration.join();
    REG_REQUIRE(registered && diagnostics.empty());
    REG_REQUIRE(output->indices.at("shutdown-batch-a") == 0);
    REG_REQUIRE(output->indices.at("shutdown-batch-b") == 1);
    value.loggers.clear();
    logger.shutdown();
    REG_REQUIRE(first->shutdowns == 1 && second->shutdowns == 1);
}

void shutdown_wins() {
    auto& logger = logit::Logger::get_instance();
    auto existing = registration_test::batch("existing");
    auto existing_probe = registration_test::probe(existing.loggers[0]);
    registration_test::Result existing_result;
    logit::ConfigurationDiagnostics existing_diagnostics;
    REG_REQUIRE(registration_test::append(existing, existing_result, existing_diagnostics));
    auto value = registration_test::batch("late");
    auto incoming_probe = registration_test::probe(value.loggers[0]);
    registration_test::Gate preparing;
    incoming_probe->capability_gate = &preparing;
    registration_test::Result output = existing_result;
    const auto previous = output;
    const auto snapshot = registration_test::Access::snapshot(logger);
    bool registered = true;
    logit::ConfigurationDiagnostics diagnostics;
    std::thread registration([&]() { registered = registration_test::append(value, output, diagnostics); });
    preparing.wait_until_entered();
    logger.shutdown();
    preparing.release();
    registration.join();
    incoming_probe->capability_gate = nullptr;
    REG_REQUIRE(!registered && !diagnostics.empty());
    REG_REQUIRE(output == previous);
    REG_REQUIRE(logger.logger_count() == 1);
    REG_REQUIRE(registration_test::Access::snapshot(logger) == snapshot);
    REG_REQUIRE(existing_probe->shutdowns == 1 && existing_probe->observed_shutdown_count == 1);
    REG_REQUIRE(incoming_probe->shutdowns == 0);
    logit::ConfigurationPreparedSet empty;
    empty.schema_version = 1;
    registration_test::expect_rejected(empty);
    registration_test::expect_rejected(value);
    logger.add_logger(std::unique_ptr<logit::ILogger>(new registration_test::ProbeLogger()),
                      std::unique_ptr<logit::ILogFormatter>(new registration_test::ProbeFormatter()));
    REG_REQUIRE(logger.logger_count() == 1);
    logger.shutdown();
    REG_REQUIRE(existing_probe->shutdowns == 1);
}

} // namespace

int main(int argc, char** argv) {
    REG_REQUIRE(argc == 2);
    if (std::string(argv[1]) == "registration-wins") registration_wins();
    else if (std::string(argv[1]) == "shutdown-wins") shutdown_wins();
    else REG_REQUIRE(false);
}
