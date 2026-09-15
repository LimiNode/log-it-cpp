#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
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

#ifdef LOGIT_BENCH_CONCURRENT_DISPATCH
    bool supports_concurrent_log() const noexcept override { return true; }
#endif

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

#ifdef LOGIT_BENCH_CONCURRENT_DISPATCH
    bool supports_concurrent_format() const noexcept override { return true; }
#endif
};

std::size_t env_size(const char* name, std::size_t fallback) {
    if (const char* value = std::getenv(name)) {
        try {
            return static_cast<std::size_t>(std::stoull(value));
        } catch (...) {
        }
    }
    return fallback;
}

long long run_workload(
        logit::Logger& logger,
        const logit::LogRecord& record,
        std::size_t producers,
        std::size_t total) {
    std::vector<std::size_t> per_thread(producers, total / producers);
    for (std::size_t i = 0; i < total % producers; ++i) ++per_thread[i];

    std::mutex mutex;
    std::condition_variable condition;
    std::size_t ready = 0;
    bool start = false;
    std::vector<std::thread> threads;
    threads.reserve(producers);

    for (std::size_t i = 0; i < producers; ++i) {
        threads.emplace_back([&, i]() {
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++ready;
                condition.notify_all();
                condition.wait(lock, [&]() { return start; });
            }
            for (std::size_t n = 0; n < per_thread[i]; ++n) logger.log(record);
        });
    }

    std::chrono::steady_clock::time_point begin;
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&]() { return ready == producers; });
        begin = std::chrono::steady_clock::now();
        start = true;
        condition.notify_all();
    }

    for (auto& thread : threads) thread.join();
    logger.wait();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - begin).count();
}

} // namespace

int main() {
    const std::size_t total = env_size("LOGIT_EXEC_MX_BENCH_TOTAL", 200000);
    auto sink = std::make_unique<CountingLogger>();
    auto* sink_ptr = sink.get();
    auto& logger = logit::Logger::get_instance();
    logger.add_logger(std::move(sink), std::make_unique<PassthroughFormatter>());

    const logit::LogRecord record(
            logit::LogLevel::LOG_LVL_INFO, 0, std::string(), -1,
            std::string(), std::string("prepared message"), std::string(), -1, false, false);
    const std::array<std::size_t, 4> producer_counts{{1, 4, 16, 32}};

    std::cout << "exec-mx mode="
#ifdef LOGIT_BENCH_CONCURRENT_DISPATCH
              << "concurrent";
#else
              << "serialized";
#endif
    std::cout << " total=" << total << '\n';

    std::size_t expected = 0;
    for (std::size_t producers : producer_counts) {
        const long long elapsed = run_workload(logger, record, producers, total);
        expected += total;
        if (sink_ptr->count() != expected) return 1;
        const double ns_per_call = static_cast<double>(elapsed) / static_cast<double>(total);
        std::cout << "exec-mx producers=" << producers
                  << " elapsed_ns=" << elapsed
                  << " ns_per_call=" << ns_per_call << '\n';
    }
    return 0;
}
