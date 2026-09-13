#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <logit.hpp>

namespace {

class CountingLogger final : public logit::ILogger {
public:
    void log(const logit::LogRecord&, const std::string&) override {
        m_count.fetch_add(1, std::memory_order_relaxed);
    }
    std::string get_string_param(const logit::LoggerParam&) const override { return {}; }
    std::int64_t get_int_param(const logit::LoggerParam&) const override { return 0; }
    double get_float_param(const logit::LoggerParam&) const override { return 0.0; }
    void set_log_level(logit::LogLevel level) override {
        m_level.store(static_cast<int>(level), std::memory_order_relaxed);
    }
    logit::LogLevel get_log_level() const override {
        return static_cast<logit::LogLevel>(m_level.load(std::memory_order_relaxed));
    }
    void wait() override {}
    std::size_t count() const { return m_count.load(std::memory_order_relaxed); }

private:
    std::atomic<std::size_t> m_count{0};
    std::atomic<int> m_level{static_cast<int>(logit::LogLevel::LOG_LVL_TRACE)};
};

class PassthroughFormatter final : public logit::ILogFormatter {
public:
    void set_timestamp_offset(std::int64_t) override {}
    std::string format(const logit::LogRecord& record) const override { return record.format; }
    bool is_passthrough() const noexcept override { return true; }
};

std::size_t env_size(const char* name, std::size_t fallback) {
    if (const char* value = std::getenv(name)) {
        try { return static_cast<std::size_t>(std::stoull(value)); }
        catch (...) {}
    }
    return fallback;
}

} // namespace

int main() {
    const std::size_t iterations = env_size("LOGIT_HOTPATH_BENCH_TOTAL", 200000);
    auto sink = std::make_unique<CountingLogger>();
    auto* sink_ptr = sink.get();
    logit::Logger::get_instance().add_logger(
        std::move(sink), std::make_unique<PassthroughFormatter>());

    logit::LogRecord record(
        logit::LogLevel::LOG_LVL_INFO, 0, std::string(), -1,
        std::string(), std::string("prepared message"), std::string(), -1, false, false);

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        logit::Logger::get_instance().log(record);
    }
    logit::Logger::get_instance().wait();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();

    if (sink_ptr->count() != iterations) return 1;
    const double ns_per_call = static_cast<double>(elapsed) / static_cast<double>(iterations);
    std::cout << "logger-hotpath mode="
#ifdef LOGIT_BENCH_LEGACY_REGISTRY
              << "legacy";
#else
              << "snapshot";
#endif
    std::cout << " iterations=" << iterations
              << " elapsed_ns=" << elapsed
              << " ns_per_call=" << ns_per_call << '\n';
    return 0;
}
