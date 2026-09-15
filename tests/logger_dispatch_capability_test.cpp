#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <logit.hpp>

namespace {

class OverlapProbeLogger final : public logit::ILogger {
public:
    void log(const logit::LogRecord&, const std::string&) override {
        const int active = m_active.fetch_add(1, std::memory_order_acq_rel) + 1;
        int observed = m_max_active.load(std::memory_order_relaxed);
        while (active > observed &&
               !m_max_active.compare_exchange_weak(
                       observed, active, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
        // Keep the critical section open long enough for the opt-in test to
        // observe overlap; the serialized fallback remains deterministic.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        m_active.fetch_sub(1, std::memory_order_acq_rel);
        m_calls.fetch_add(1, std::memory_order_relaxed);
    }

#ifdef LOGIT_TEST_CONCURRENT_CAPABILITY
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

    int max_active() const { return m_max_active.load(std::memory_order_relaxed); }
    int calls() const { return m_calls.load(std::memory_order_relaxed); }

private:
    std::atomic<int> m_active{0};
    std::atomic<int> m_max_active{0};
    std::atomic<int> m_calls{0};
    std::atomic<int> m_level{static_cast<int>(logit::LogLevel::LOG_LVL_TRACE)};
};

class StatelessFormatter final : public logit::ILogFormatter {
public:
    void set_timestamp_offset(std::int64_t) override {}
    std::string format(const logit::LogRecord& record) const override { return record.format; }

#ifdef LOGIT_TEST_CONCURRENT_CAPABILITY
    bool supports_concurrent_format() const noexcept override { return true; }
#endif
};

void run_probe(OverlapProbeLogger& probe, std::size_t workers) {
    std::mutex mutex;
    std::condition_variable condition;
    std::size_t ready = 0;
    bool start = false;
    std::vector<std::thread> threads;
    threads.reserve(workers);

    const logit::LogRecord record(
            logit::LogLevel::LOG_LVL_INFO, 0, std::string(), -1,
            std::string(), std::string("probe"), std::string(), -1, false, false);

    for (std::size_t i = 0; i < workers; ++i) {
        threads.emplace_back([&]() {
            {
                std::unique_lock<std::mutex> lock(mutex);
                ++ready;
                condition.notify_all();
                condition.wait(lock, [&]() { return start; });
            }
            logit::Logger::get_instance().log(record);
        });
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&]() { return ready == workers; });
        start = true;
        condition.notify_all();
    }

    for (auto& thread : threads) thread.join();
}

} // namespace

int main() {
    auto probe = std::make_unique<OverlapProbeLogger>();
    auto* probe_ptr = probe.get();
    logit::Logger::get_instance().add_logger(
            std::move(probe), std::make_unique<StatelessFormatter>());

    constexpr std::size_t workers = 8;
    run_probe(*probe_ptr, workers);
    assert(probe_ptr->calls() == static_cast<int>(workers));

#ifdef LOGIT_TEST_CONCURRENT_CAPABILITY
    // The explicitly opted-in pair must be callable concurrently.
    assert(probe_ptr->max_active() >= 2);
#else
    // Existing/custom implementations retain the serialized fallback.
    assert(probe_ptr->max_active() == 1);
#endif

    return 0;
}
