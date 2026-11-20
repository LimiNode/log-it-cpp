#pragma once
#ifndef LOGIT_CPP_HEADER_TESTS_CONFIGURATION_REGISTRATION_TEST_SUPPORT_HPP_INCLUDED
#define LOGIT_CPP_HEADER_TESTS_CONFIGURATION_REGISTRATION_TEST_SUPPORT_HPP_INCLUDED

#include <logit.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <thread>

namespace logit { namespace detail {

struct LoggerRegistrationTestAccess {
    static std::shared_ptr<const void> snapshot(Logger& logger) {
        const auto value = std::atomic_load_explicit(
                &logger.m_loggers_snapshot, std::memory_order_acquire);
        return std::shared_ptr<const void>(value, value.get());
    }

    static std::size_t configured_count(Logger& logger) {
        LoggerReadLock lock(logger.m_loggers_mx);
        return logger.m_configured_ids.size();
    }

    static bool capacity(std::size_t current, std::size_t count) {
        return Logger::registration_capacity_available(current, count);
    }

    static void fail_snapshot(Logger& logger) {
        LoggerWriteLock lock(logger.m_loggers_mx);
        logger.m_registration_allocation_failure = Logger::RegistrationAllocationStage::Snapshot;
    }

    static void fail_result(Logger& logger) {
        LoggerWriteLock lock(logger.m_loggers_mx);
        logger.m_registration_allocation_failure = Logger::RegistrationAllocationStage::Result;
    }

    static void fail_result_map(Logger& logger) {
        LoggerWriteLock lock(logger.m_loggers_mx);
        logger.m_registration_allocation_failure = Logger::RegistrationAllocationStage::ResultMap;
    }
};

} } // namespace logit::detail

namespace registration_test {

inline void require_impl(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "registration requirement failed at line %d: %s\n", line, expression);
        std::abort();
    }
}

#define REG_REQUIRE(condition) \
    ::registration_test::require_impl(static_cast<bool>(condition), #condition, __LINE__)

struct Gate {
    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        condition.notify_all();
        condition.wait(lock, [this]() { return released; });
    }
    void wait_until_entered() {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this]() { return entered; });
    }
    void release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        condition.notify_all();
    }
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool released = false;
};

class ProbeLogger : public logit::ILogger {
public:
    void log(const logit::LogRecord&, const std::string& message) override {
        if (log_gate) log_gate->arrive_and_wait();
        std::lock_guard<std::mutex> lock(mutex);
        messages.push_back(message);
        ++calls;
    }
    std::string get_string_param(const logit::LoggerParam&) const override { return "probe"; }
    std::int64_t get_int_param(const logit::LoggerParam&) const override { return calls.load(); }
    double get_float_param(const logit::LoggerParam&) const override { return 1.0; }
    void set_log_level(logit::LogLevel value) override { level.store(static_cast<int>(value)); }
    logit::LogLevel get_log_level() const override { return static_cast<logit::LogLevel>(level.load()); }
    bool supports_concurrent_log() const noexcept override {
        // Reentrant query proves registration never asks for capabilities under its lock.
        (void)logit::Logger::get_instance().logger_count();
        ++capability_calls;
        if (capability_gate) capability_gate->arrive_and_wait();
        return concurrent;
    }
    void wait() override { ++waits; }
    void shutdown() override {
        observed_shutdown_count = logit::Logger::get_instance().logger_count();
        ++shutdowns;
    }
    std::vector<std::string> get_buffered_strings() const override {
        std::lock_guard<std::mutex> lock(mutex);
        return messages;
    }
    logit::LogClearResult clear_logs(const logit::LogClearOptions&) override {
        std::lock_guard<std::mutex> lock(mutex);
        logit::LogClearResult result;
        result.ok = true;
        result.status = logit::LogClearStatus::Cleared;
        result.cleared_records = messages.size();
        messages.clear();
        return result;
    }
    std::atomic<int> calls{0};
    std::atomic<int> waits{0};
    std::atomic<int> shutdowns{0};
    mutable std::atomic<int> capability_calls{0};
    std::atomic<int> level{static_cast<int>(logit::LogLevel::LOG_LVL_TRACE)};
    std::atomic<std::size_t> observed_shutdown_count{0};
    bool concurrent = false;
    Gate* log_gate = nullptr;
    Gate* capability_gate = nullptr;
    mutable std::mutex mutex;
    std::vector<std::string> messages;
};

class ProbeFormatter : public logit::ILogFormatter {
public:
    void set_timestamp_offset(std::int64_t) override {}
    std::string format(const logit::LogRecord& record) const override { return record.format; }
    bool supports_concurrent_format() const noexcept override { return true; }
};

class Combined final : public ProbeLogger, public logit::ILogFormatter {
public:
    void set_timestamp_offset(std::int64_t) override {}
    std::string format(const logit::LogRecord& record) const override { return record.format; }
};

typedef std::shared_ptr<const logit::ConfigurationRegistrationResult> Result;
typedef logit::detail::LoggerRegistrationTestAccess Access;

inline logit::ConfigurationPreparedLogger entry(const std::string& id) {
    logit::ConfigurationPreparedLogger value;
    value.id = id;
    value.logger = std::make_shared<ProbeLogger>();
    value.formatter = std::make_shared<ProbeFormatter>();
    return value;
}

inline logit::ConfigurationPreparedSet batch(const std::string& id) {
    logit::ConfigurationPreparedSet value;
    value.schema_version = 1;
    value.loggers.push_back(entry(id));
    return value;
}

inline std::shared_ptr<ProbeLogger> probe(const logit::ConfigurationPreparedLogger& value) {
    const auto result = std::dynamic_pointer_cast<ProbeLogger>(value.logger);
    REG_REQUIRE(result);
    return result;
}

inline logit::LogRecord record(int index = -1) {
    return logit::LogRecord(logit::LogLevel::LOG_LVL_INFO, 0, "", -1, "", "message", "", index, false, false);
}

inline bool append(const logit::ConfigurationPreparedSet& value, Result& output,
                   logit::ConfigurationDiagnostics& diagnostics,
                   const logit::ConfigurationRegistrationOptions& options =
                       logit::ConfigurationRegistrationOptions()) {
    return logit::Logger::get_instance().register_configuration(value, options, output, diagnostics);
}

inline void expect_rejected(const logit::ConfigurationPreparedSet& value,
                            const logit::ConfigurationRegistrationOptions& options =
                                logit::ConfigurationRegistrationOptions()) {
    logit::Logger& logger = logit::Logger::get_instance();
    const auto snapshot = Access::snapshot(logger);
    const auto count = logger.logger_count();
    const auto ids = Access::configured_count(logger);
    Result output(new logit::ConfigurationRegistrationResult());
    const Result previous = output;
    logit::ConfigurationDiagnostics diagnostics;
    REG_REQUIRE(!append(value, output, diagnostics, options));
    REG_REQUIRE(!diagnostics.empty());
    REG_REQUIRE(output == previous);
    REG_REQUIRE(Access::snapshot(logger) == snapshot);
    REG_REQUIRE(logger.logger_count() == count);
    REG_REQUIRE(Access::configured_count(logger) == ids);
}

} // namespace registration_test

#endif // LOGIT_CPP_HEADER_TESTS_CONFIGURATION_REGISTRATION_TEST_SUPPORT_HPP_INCLUDED
