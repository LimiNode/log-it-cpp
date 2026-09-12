#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

int main() {
    class DelayedSink final : public spdlog::sinks::sink {
    public:
        void log(const spdlog::details::log_msg&) override {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            m_seen.fetch_add(1, std::memory_order_release);
        }
        void set_pattern(const std::string&) override {}
        void set_formatter(std::unique_ptr<spdlog::formatter>) override {}
        void flush() override {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                ++m_flushes;
            }
            m_cv.notify_all();
        }
        bool wait_for_flush(std::size_t expected, std::size_t count) {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_cv.wait_for(lock, std::chrono::seconds(5), [&]() {
                return m_flushes >= expected && m_seen.load(std::memory_order_acquire) == count;
            });
        }
        std::size_t seen() const { return m_seen.load(std::memory_order_acquire); }
    private:
        std::atomic<std::size_t> m_seen{0};
        std::condition_variable m_cv;
        std::mutex m_mutex;
        std::size_t m_flushes = 0;
    };

    constexpr std::size_t message_count = 32;
    spdlog::init_thread_pool(8, 1);
    auto sink = std::make_shared<DelayedSink>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "flush-regression", sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::trace);
    for (std::size_t i = 0; i < message_count; ++i) {
        logger->info("delayed message {}", i);
    }

    logger->flush();
    const bool complete = sink->wait_for_flush(1, message_count);
    spdlog::shutdown();
    return complete ? 0 : 1;
}
