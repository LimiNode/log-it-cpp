#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <logit.hpp>

#include "BenchmarkMetadata.hpp"

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
    void reset() { m_count.store(0, std::memory_order_relaxed); }

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

std::unique_ptr<logit::ILogFormatter> make_formatter() {
#ifdef LOGIT_PUBLIC_BENCH_FORMATTED
    return std::make_unique<logit::SimpleLogFormatter>("[%l] %v");
#else
    return std::make_unique<PassthroughFormatter>();
#endif
}

const char* benchmark_mode() {
#ifdef LOGIT_PUBLIC_BENCH_FORMATTED
    return "formatted";
#else
    return "passthrough";
#endif
}

std::size_t env_size(const char* name, std::size_t fallback) {
    if (const char* value = std::getenv(name)) {
        try { return static_cast<std::size_t>(std::stoull(value)); }
        catch (...) {}
    }
    return fallback;
}

std::chrono::nanoseconds run_workload(std::size_t producers, std::size_t total) {
    std::mutex start_mx;
    std::condition_variable start_cv;
    std::condition_variable ready_cv;
    bool start_flag = false;
    std::size_t ready = 0;

    std::vector<std::thread> workers;
    workers.reserve(producers);
    for (std::size_t producer = 0; producer < producers; ++producer) {
        workers.emplace_back([&, producer]() {
            const std::size_t begin = (total * producer) / producers;
            const std::size_t end = (total * (producer + 1)) / producers;
            {
                std::unique_lock<std::mutex> lock(start_mx);
                ++ready;
                if (ready == producers) ready_cv.notify_one();
                start_cv.wait(lock, [&] { return start_flag; });
            }
            for (std::size_t i = begin; i < end; ++i) {
                LOGIT_INFO("public macro message", i);
            }
        });
    }

    std::chrono::steady_clock::time_point start;
    {
        std::unique_lock<std::mutex> lock(start_mx);
        ready_cv.wait(lock, [&] { return ready == producers; });
        start = std::chrono::steady_clock::now();
        start_flag = true;
    }
    start_cv.notify_all();

    for (auto& worker : workers) worker.join();
    logit::Logger::get_instance().wait();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
}

} // namespace

int main() {
    const std::size_t producers = env_size("LOGIT_PUBLIC_BENCH_PRODUCERS", 4);
    const std::size_t total = env_size("LOGIT_PUBLIC_BENCH_TOTAL", 20000);
    const std::size_t warmup = env_size("LOGIT_PUBLIC_BENCH_WARMUP", 0);
    if (producers == 0 || total == 0) return 2;

    const auto metadata = logit_bench::make_benchmark_metadata(
        "not-applicable",
        "not-applicable",
        "backend-count",
        "logger-wait");
    logit_bench::validate_comparable_metadata(metadata);
    logit_bench::print_benchmark_metadata(std::cout, metadata, total, warmup);

    auto sink = std::make_unique<CountingLogger>();
    auto* sink_ptr = sink.get();
    logit::Logger::get_instance().add_logger(
        std::move(sink), make_formatter());

    if (warmup > 0) {
        run_workload(producers, warmup);
        sink_ptr->reset();
    }
    const auto elapsed = run_workload(producers, total).count();

    if (sink_ptr->count() != total) return 1;
    const double throughput = static_cast<double>(total) * 1e9 / static_cast<double>(elapsed);
    std::cout << "public-macro mode=" << benchmark_mode()
              << " producers=" << producers
              << " total=" << total
              << " elapsed_ns=" << elapsed
              << " throughput=" << throughput << " msg/s\n";
    return 0;
}
