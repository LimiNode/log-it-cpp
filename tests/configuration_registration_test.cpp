#include "configuration_registration_test_support.hpp"

namespace {

void invalid_manual_sets() {
    auto value = registration_test::batch("invalid-manual");
    value.schema_version = 0;
    registration_test::expect_rejected(value);
    value.schema_version = 2;
    registration_test::expect_rejected(value);
    value.schema_version = 1;
    value.loggers[0].id.clear();
    registration_test::expect_rejected(value);
    value = registration_test::batch("duplicate-incoming");
    value.loggers.push_back(registration_test::entry("duplicate-incoming"));
    registration_test::expect_rejected(value);
    value = registration_test::batch("invalid-second");
    value.loggers.push_back(registration_test::entry("null-backend"));
    value.loggers[1].logger.reset();
    registration_test::expect_rejected(value);
    value.loggers[1] = registration_test::entry("null-formatter");
    value.loggers[1].formatter.reset();
    registration_test::expect_rejected(value);
    value = registration_test::batch("invalid-level");
    value.loggers[0].level = static_cast<logit::LogLevel>(-1);
    registration_test::expect_rejected(value);
    value.loggers[0].level = static_cast<logit::LogLevel>(999);
    registration_test::expect_rejected(value);
    value = registration_test::batch("unknown-option");
    logit::ConfigurationRegistrationOptions options;
    options["not-an-incoming-id"].enabled = false;
    registration_test::expect_rejected(value, options);
}

void alias_validation() {
    for (int concurrent = 0; concurrent != 2; ++concurrent) {
        auto value = registration_test::batch("alias-a");
        registration_test::probe(value.loggers[0])->concurrent = concurrent != 0;
        value.loggers.push_back(registration_test::entry("alias-b"));
        value.loggers[1].logger = value.loggers[0].logger;
        registration_test::expect_rejected(value);
        REG_REQUIRE(registration_test::probe(value.loggers[0])->capability_calls == 0);
        value.loggers[1].logger = std::make_shared<registration_test::ProbeLogger>();
        value.loggers[1].formatter = value.loggers[0].formatter;
        registration_test::expect_rejected(value);
    }

    auto combined = std::make_shared<registration_test::Combined>();
    auto value = registration_test::batch("combined-invalid-ownership");
    value.loggers[0].logger = combined;
    value.loggers[0].formatter = std::shared_ptr<logit::ILogFormatter>(
            combined.get(), [](logit::ILogFormatter*) {});
    registration_test::expect_rejected(value);
    value.loggers[0].id = "combined-valid";
    value.loggers[0].formatter = combined;
    registration_test::Result output;
    logit::ConfigurationDiagnostics diagnostics;
    REG_REQUIRE(registration_test::append(value, output, diagnostics));
    logit::Logger::get_instance().log(registration_test::record(output->indices.at("combined-valid")));
    REG_REQUIRE(combined->calls == 1);

    auto cross_role = registration_test::batch("cross-role");
    cross_role.loggers[0].formatter = combined;
    registration_test::expect_rejected(cross_role);
    value.loggers[0].id = "combined-later";
    registration_test::expect_rejected(value);

    struct Pair {
        registration_test::ProbeLogger logger;
        registration_test::ProbeFormatter formatter;
        registration_test::ProbeLogger second_logger;
        registration_test::ProbeFormatter second_formatter;
    };
    auto owner = std::make_shared<Pair>();
    auto distinct = registration_test::batch("distinct-shared-owner");
    distinct.loggers[0].logger = std::shared_ptr<logit::ILogger>(owner, &owner->logger);
    distinct.loggers[0].formatter = std::shared_ptr<logit::ILogFormatter>(owner, &owner->formatter);
    distinct.loggers.push_back(registration_test::entry("distinct-shared-owner-second"));
    distinct.loggers[1].logger = std::shared_ptr<logit::ILogger>(owner, &owner->second_logger);
    distinct.loggers[1].formatter = std::shared_ptr<logit::ILogFormatter>(owner, &owner->second_formatter);
    REG_REQUIRE(registration_test::append(distinct, output, diagnostics));

    auto legacy_backend = new registration_test::ProbeLogger();
    auto legacy_formatter = new registration_test::ProbeFormatter();
    logit::Logger::get_instance().add_logger(
            std::unique_ptr<logit::ILogger>(legacy_backend),
            std::unique_ptr<logit::ILogFormatter>(legacy_formatter));
    auto legacy_alias = registration_test::batch("legacy-backend-alias");
    legacy_alias.loggers[0].logger = std::shared_ptr<logit::ILogger>(legacy_backend, [](logit::ILogger*) {});
    registration_test::expect_rejected(legacy_alias);
    legacy_alias = registration_test::batch("legacy-formatter-alias");
    legacy_alias.loggers[0].formatter = std::shared_ptr<logit::ILogFormatter>(legacy_formatter, [](logit::ILogFormatter*) {});
    registration_test::expect_rejected(legacy_alias);
}

void success_routing_and_lifetime() {
    auto& logger = logit::Logger::get_instance();
    const auto old_count = logger.logger_count();
    auto value = registration_test::batch("routing-a");
    value.loggers.push_back(registration_test::entry("routing-b"));
    auto first = registration_test::probe(value.loggers[0]);
    auto second = registration_test::probe(value.loggers[1]);
    // Registration validates metadata but never reapplies it to a manual object.
    value.loggers[0].level = logit::LogLevel::LOG_LVL_FATAL;
    logit::ConfigurationRegistrationOptions options;
    options["routing-b"].single_mode = true;
    options["routing-b"].enabled = false;
    registration_test::Result output;
    logit::ConfigurationDiagnostics diagnostics;
    diagnostics.add(logit::ConfigurationErrorCode::InvalidValue, "stale", "stale");
    REG_REQUIRE(registration_test::append(value, output, diagnostics, options));
    REG_REQUIRE(diagnostics.empty());
    REG_REQUIRE(logger.logger_count() == old_count + 2);
    const int first_index = output->indices.at("routing-a");
    const int second_index = output->indices.at("routing-b");
    REG_REQUIRE(first_index == static_cast<int>(old_count));
    REG_REQUIRE(second_index == first_index + 1);
    REG_REQUIRE(logger.is_logger_enabled(first_index));
    REG_REQUIRE(!logger.is_logger_enabled(second_index));
    REG_REQUIRE(logger.is_logger_single_mode(second_index));
    REG_REQUIRE(logger.get_log_level(first_index) == logit::LogLevel::LOG_LVL_TRACE);
    logger.log(registration_test::record());
    REG_REQUIRE(first->calls == 1 && second->calls == 0);
    logger.log(registration_test::record(second_index));
    REG_REQUIRE(second->calls == 0);
    logger.set_logger_enabled(second_index, true);
    logger.log(registration_test::record());
    REG_REQUIRE(first->calls == 2 && second->calls == 0);
    logger.log(registration_test::record(second_index));
    REG_REQUIRE(second->calls == 1);
    REG_REQUIRE(logger.get_buffered_strings(first_index).size() == 2);
    REG_REQUIRE(logger.get_string_param(first_index, logit::LoggerParam::LastFileName) == "probe");
    REG_REQUIRE(logger.get_int_param(first_index, logit::LoggerParam::DroppedLogCount) == 2);
    REG_REQUIRE(logger.get_float_param(first_index, logit::LoggerParam::DroppedLogCount) == 1.0);
    logger.wait();
    REG_REQUIRE(first->waits == 1 && second->waits == 1);
    REG_REQUIRE(logger.clear_logger(first_index).cleared_records == 2);
    REG_REQUIRE(logger.get_buffered_strings(first_index).empty());
    auto previous_snapshot = registration_test::Access::snapshot(logger);
    registration_test::expect_rejected(value);
    REG_REQUIRE(registration_test::Access::snapshot(logger) == previous_snapshot);
    auto later_alias = value;
    later_alias.loggers[0].id = "later-alias-a";
    later_alias.loggers[1].id = "later-alias-b";
    registration_test::expect_rejected(later_alias);
    value.loggers.clear();
    first.reset();
    second.reset();
    logger.log(registration_test::record(second_index));
    REG_REQUIRE(logger.get_logger_as<registration_test::ProbeLogger>(second_index)->calls == 2);
    logger.set_logger_enabled(first_index, false);
    logger.add_logger(std::unique_ptr<logit::ILogger>(new registration_test::ProbeLogger()),
                      std::unique_ptr<logit::ILogFormatter>(new registration_test::ProbeFormatter()), true);
    REG_REQUIRE(output->indices.at("routing-a") == first_index);
    REG_REQUIRE(!logger.is_logger_enabled(first_index));
    REG_REQUIRE(logger.is_logger_single_mode(second_index));
    REG_REQUIRE(logger.clear_all_loggers().ok);
}

void empty_and_capacity() {
    auto& logger = logit::Logger::get_instance();
    logit::ConfigurationPreparedSet empty;
    registration_test::expect_rejected(empty);
    empty.schema_version = 1;
    registration_test::Result output;
    logit::ConfigurationDiagnostics diagnostics;
    const auto old_snapshot = registration_test::Access::snapshot(logger);
    REG_REQUIRE(registration_test::append(empty, output, diagnostics));
    REG_REQUIRE(output && output->indices.empty());
    REG_REQUIRE(registration_test::Access::snapshot(logger) == old_snapshot);
    logit::ConfigurationRegistrationOptions options;
    options["unknown"].enabled = true;
    registration_test::expect_rejected(empty, options);
    const auto limit = static_cast<std::size_t>(INT_MAX);
    REG_REQUIRE(registration_test::Access::capacity(limit - 1, 1));
    REG_REQUIRE(registration_test::Access::capacity(limit, 0));
    REG_REQUIRE(!registration_test::Access::capacity(limit, 1));
    REG_REQUIRE(!registration_test::Access::capacity(limit + 1, 0));
    REG_REQUIRE(!registration_test::Access::capacity(1, std::numeric_limits<std::size_t>::max()));
}

class ReentrantDestructorProbe final : public registration_test::ProbeLogger {
public:
    explicit ReentrantDestructorProbe(std::atomic<bool>& destroyed) : m_destroyed(destroyed) {}
    ~ReentrantDestructorProbe() override {
        (void)logit::Logger::get_instance().logger_count();
        m_destroyed.store(true);
    }
private:
    std::atomic<bool>& m_destroyed;
};

void allocation_failures() {
    auto& logger = logit::Logger::get_instance();
    for (int stage = 0; stage != 3; ++stage) {
        auto value = registration_test::batch("failure-stage-" + std::to_string(stage));
        value.loggers.push_back(registration_test::entry("failure-second-" + std::to_string(stage)));
        registration_test::Result output(new logit::ConfigurationRegistrationResult());
        const auto previous_output = output;
        const auto old_snapshot = registration_test::Access::snapshot(logger);
        const auto old_count = logger.logger_count();
        const auto old_ids = registration_test::Access::configured_count(logger);
        if (stage == 0) registration_test::Access::fail_result(logger);
        else if (stage == 1) registration_test::Access::fail_snapshot(logger);
        else registration_test::Access::fail_result_map(logger);
        logit::ConfigurationDiagnostics diagnostics;
        bool caught = false;
        try { (void)registration_test::append(value, output, diagnostics); }
        catch (const std::bad_alloc&) { caught = true; }
        REG_REQUIRE(caught);
        REG_REQUIRE(output == previous_output);
        REG_REQUIRE(logger.logger_count() == old_count);
        REG_REQUIRE(registration_test::Access::configured_count(logger) == old_ids);
        REG_REQUIRE(registration_test::Access::snapshot(logger) == old_snapshot);
        REG_REQUIRE(registration_test::probe(value.loggers[0])->shutdowns == 0);
        REG_REQUIRE(registration_test::append(value, output, diagnostics));
    }
    const auto old_count = logger.logger_count();
    const auto old_snapshot = registration_test::Access::snapshot(logger);
    registration_test::Access::fail_snapshot(logger);
    std::atomic<bool> destroyed{false};
    bool caught = false;
    try {
        logger.add_logger(std::unique_ptr<logit::ILogger>(new ReentrantDestructorProbe(destroyed)),
                          std::unique_ptr<logit::ILogFormatter>(new registration_test::ProbeFormatter()));
    } catch (const std::bad_alloc&) { caught = true; }
    REG_REQUIRE(caught);
    REG_REQUIRE(destroyed.load());
    REG_REQUIRE(logger.logger_count() == old_count);
    REG_REQUIRE(registration_test::Access::snapshot(logger) == old_snapshot);
    logger.log(registration_test::record(0));
}

void dispatch_retains_complete_snapshot() {
    auto& logger = logit::Logger::get_instance();
    registration_test::Gate gate;
    auto baseline = registration_test::batch("snapshot-baseline");
    auto baseline_probe = registration_test::probe(baseline.loggers[0]);
    baseline_probe->log_gate = &gate;
    registration_test::Result output;
    logit::ConfigurationDiagnostics diagnostics;
    REG_REQUIRE(registration_test::append(baseline, output, diagnostics));
    std::thread dispatch([&logger]() { logger.log(registration_test::record()); });
    gate.wait_until_entered();
    auto value = registration_test::batch("snapshot-new-a");
    value.loggers.push_back(registration_test::entry("snapshot-new-b"));
    auto first = registration_test::probe(value.loggers[0]);
    auto second = registration_test::probe(value.loggers[1]);
    REG_REQUIRE(registration_test::append(value, output, diagnostics));
    REG_REQUIRE(first->calls == 0 && second->calls == 0);
    gate.release();
    dispatch.join();
    baseline_probe->log_gate = nullptr;
    REG_REQUIRE(first->calls == 0 && second->calls == 0);
    logger.log(registration_test::record());
    REG_REQUIRE(first->calls == 1 && second->calls == 1);
}

class OnceGatedProbe final : public registration_test::ProbeLogger {
public:
    explicit OnceGatedProbe(registration_test::Gate& gate) : m_gate(gate) {}
    bool supports_concurrent_log() const noexcept override {
        if (m_queries.fetch_add(1) == 0) m_gate.arrive_and_wait();
        return true;
    }
private:
    registration_test::Gate& m_gate;
    mutable std::atomic<int> m_queries{0};
};

void competing_writers_revalidate() {
    for (int alias = 0; alias != 2; ++alias) {
        auto& logger = logit::Logger::get_instance();
        registration_test::Gate gate;
        auto blocked_probe = std::make_shared<OnceGatedProbe>(gate);
        auto first = registration_test::batch(alias ? "racing-alias-first" : "racing-duplicate");
        first.loggers[0].logger = blocked_probe;
        registration_test::Result first_output;
        logit::ConfigurationDiagnostics first_diagnostics;
        bool first_success = true;
        std::thread first_writer([&]() {
            first_success = registration_test::append(first, first_output, first_diagnostics);
        });
        gate.wait_until_entered();
        auto second = registration_test::batch(alias ? "racing-alias-second" : "racing-duplicate");
        if (alias) second.loggers[0].logger = blocked_probe;
        registration_test::Result second_output;
        logit::ConfigurationDiagnostics second_diagnostics;
        const auto count = logger.logger_count();
        REG_REQUIRE(registration_test::append(second, second_output, second_diagnostics));
        gate.release();
        first_writer.join();
        REG_REQUIRE(!first_success && !first_output && !first_diagnostics.empty());
        REG_REQUIRE(logger.logger_count() == count + 1);
        REG_REQUIRE(first_diagnostics.has_code(alias ? logit::ConfigurationErrorCode::RegistrationFailed
                                                   : logit::ConfigurationErrorCode::DuplicateLoggerId));
    }
}

} // namespace

int main() {
    invalid_manual_sets();
    alias_validation();
    success_routing_and_lifetime();
    empty_and_capacity();
    allocation_failures();
    dispatch_retains_complete_snapshot();
    competing_writers_revalidate();
    logit::Logger::get_instance().shutdown();
}
