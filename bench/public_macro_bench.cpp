#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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
    void set_log_level(logit::LogLevel level) override { m_level.store(static_cast<int>(level)); }
    logit::LogLevel get_log_level() const override {
        return static_cast<logit::LogLevel>(m_level.load());
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
    const std::size_t producers = env_size("LOGIT_PUBLIC_BENCH_PRODUCERS", 4);
    const std::size_t total = env_size("LOGIT_PUBLIC_BENCH_TOTAL", 20000);
    if (producers == 0 || total == 0) return 2;

    auto sink = std::make_unique<CountingLogger>();
    auto* sink_ptr = sink.get();
    logit::Logger::get_instance().add_logger(
        std::move(sink), std::make_unique<PassthroughFormatter>());

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> workers;
    workers.reserve(producers);
    for (std::size_t producer = 0; producer < producers; ++producer) {
        workers.emplace_back([producer, producers, total]() {
            const std::size_t begin = (total * producer) / producers;
            const std::size_t end = (total * (producer + 1)) / producers;
            for (std::size_t i = begin; i < end; ++i) {
                LOGIT_INFO("public macro message", i);
            }
        });
    }
    for (auto& worker : workers) worker.join();
    logit::Logger::get_instance().wait();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();

    if (sink_ptr->count() != total) return 1;
    const double throughput = static_cast<double>(total) * 1e9 / static_cast<double>(elapsed);
    std::cout << "public-macro producers=" << producers
              << " total=" << total
              << " elapsed_ns=" << elapsed
              << " throughput=" << throughput << " msg/s\n";
    return 0;
}
