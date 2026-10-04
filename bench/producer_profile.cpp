#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <logit.hpp>

namespace {

constexpr std::size_t kDefaultTotal = 100000;
constexpr std::size_t kDefaultWarmup = 10000;
constexpr std::size_t kDefaultRepeats = 5;
constexpr std::size_t kQueueCapacity = 262144;
const std::string kMessage(200, 'x');

#if defined(LOGIT_USE_MPSC_RING)
constexpr const char* kQueueBackend = "mpsc_ring";
#else
constexpr const char* kQueueBackend = "mutex_deque";
#endif

std::uint64_t g_observer = 0;
std::atomic<std::size_t> g_queue_completed{0};

std::size_t env_size(const char* name, std::size_t fallback) {
    if (const char* value = std::getenv(name)) {
        try {
            return static_cast<std::size_t>(std::stoull(value));
        } catch (...) {
        }
    }
    return fallback;
}

class PassthroughFormatter final : public logit::ILogFormatter {
public:
    void set_timestamp_offset(std::int64_t) override {}
    std::string format(const logit::LogRecord& record) const override {
        return record.format;
    }
    bool is_passthrough() const noexcept override { return true; }
};

class ProfilingSink final : public logit::ILogger {
public:
    enum class Mode {
        Synchronous,
        ConstructAndInvokeTaskOnly,
        AsyncFullMessage,
    };

    void set_mode(Mode mode) {
        wait();
        m_mode = mode;
        reset_count();
    }

    void log(const logit::LogRecord&, const std::string& message) override {
        if (m_mode == Mode::Synchronous) {
            m_count.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        std::string payload = message;
        std::function<void()> task =
            [this, payload = std::move(payload)]() mutable {
                g_observer += payload.size();
                m_count.fetch_add(1, std::memory_order_relaxed);
            };
        if (m_mode == Mode::ConstructAndInvokeTaskOnly) {
            task();
            return;
        }
        logit::detail::TaskExecutor::get_instance().add_task(std::move(task));
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
    void wait() override {
        if (m_mode == Mode::AsyncFullMessage) {
            logit::detail::TaskExecutor::get_instance().wait();
        }
    }
    std::size_t count() const { return m_count.load(std::memory_order_acquire); }
    void reset_count() { m_count.store(0, std::memory_order_relaxed); }

private:
    Mode m_mode = Mode::Synchronous;
    std::atomic<std::size_t> m_count{0};
    std::atomic<int> m_level{static_cast<int>(logit::LogLevel::LOG_LVL_TRACE)};
};

using Loop = std::function<void(std::size_t)>;
using Cleanup = std::function<void(std::size_t)>;

double median_ns_per_call(const Loop& loop, const Cleanup& cleanup,
                          std::size_t warmup, std::size_t total,
                          std::size_t repeats) {
    loop(warmup);
    cleanup(warmup);

    std::vector<std::uint64_t> samples;
    samples.reserve(repeats);
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
        const auto start = std::chrono::steady_clock::now();
        loop(total);
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count();
        cleanup(total);
        samples.push_back(static_cast<std::uint64_t>(elapsed));
    }

    std::sort(samples.begin(), samples.end());
    return static_cast<double>(samples[samples.size() / 2]) /
           static_cast<double>(total);
}

logit::LogRecord make_record() {
    return logit::LogRecord(
        logit::LogLevel::LOG_LVL_INFO,
        0,
        std::string(),
        -1,
        std::string(),
        kMessage,
        std::string(),
        -1,
        false,
        false);
}

} // namespace

int main() {
    const std::size_t total = env_size("LOGIT_PRODUCER_PROFILE_TOTAL", kDefaultTotal);
    const std::size_t warmup = env_size("LOGIT_PRODUCER_PROFILE_WARMUP", kDefaultWarmup);
    const std::size_t repeats = env_size("LOGIT_PRODUCER_PROFILE_REPEATS", kDefaultRepeats);
    if (total == 0 || repeats == 0) {
        std::cerr << "total and repeats must be positive\n";
        return 2;
    }

    auto& executor = logit::detail::TaskExecutor::get_instance();
    executor.set_queue_policy(logit::QueuePolicy::Block);
    executor.set_max_queue_size(kQueueCapacity);

    auto sink = std::make_unique<ProfilingSink>();
    auto* sink_ptr = sink.get();
    auto& logger = logit::Logger::get_instance();
    logger.add_logger(std::move(sink), std::make_unique<PassthroughFormatter>());

    const logit::LogRecord prepared = make_record();
    const auto log_prepared = [&logger, &prepared](std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            logger.log(prepared);
        }
    };

    std::cout << "producer-profile total=" << total
              << " warmup=" << warmup
              << " repeats=" << repeats
              << " message_bytes=" << kMessage.size()
              << " queue_capacity=" << kQueueCapacity
              << " queue_backend=" << kQueueBackend << '\n';

    const double copy_ns = median_ns_per_call(
        [](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                std::string copy = kMessage;
                g_observer += copy.size();
            }
        },
        [](std::size_t) {}, warmup, total, repeats);
    std::cout << "case=string_copy_only ns_per_call=" << copy_ns << '\n';

    const double record_ns = median_ns_per_call(
        [](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                const auto record = make_record();
                g_observer += record.format.size();
            }
        },
        [](std::size_t) {}, warmup, total, repeats);
    std::cout << "case=logrecord_construct ns_per_call=" << record_ns << '\n';

    const double task_marker_ns = median_ns_per_call(
        [](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                std::function<void()> task = []() { ++g_observer; };
                task();
            }
        },
        [](std::size_t) {}, warmup, total, repeats);
    std::cout << "case=task_object_marker_only ns_per_call=" << task_marker_ns << '\n';

    const double task_payload_ns = median_ns_per_call(
        [](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                std::string payload = kMessage;
                std::function<void()> task =
                    [payload = std::move(payload)]() mutable {
                        g_observer += payload.size();
                    };
                task();
            }
        },
        [](std::size_t) {}, warmup, total, repeats);
    std::cout << "case=task_object_full_message ns_per_call=" << task_payload_ns << '\n';

    auto& task_executor = logit::detail::TaskExecutor::get_instance();
    const double queue_noop_ns = median_ns_per_call(
        [&task_executor](std::size_t count) {
            std::function<void()> task = []() {
                g_queue_completed.fetch_add(1, std::memory_order_relaxed);
            };
            for (std::size_t i = 0; i < count; ++i) {
                task_executor.add_task(task);
            }
        },
        [&task_executor](std::size_t expected) {
            task_executor.wait();
            const auto completed = g_queue_completed.load(std::memory_order_relaxed);
            if (completed != expected) {
                std::cerr << "prebuilt TaskExecutor completion mismatch\n";
                std::exit(7);
            }
            g_queue_completed.store(0, std::memory_order_relaxed);
        }, warmup, total, repeats);
    std::cout << "case=taskexecutor_enqueue_prebuilt_noop ns_per_call="
              << queue_noop_ns << '\n';

    const double queue_payload_ns = median_ns_per_call(
        [&task_executor](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                std::string payload = kMessage;
                task_executor.add_task(
                    [payload = std::move(payload)]() mutable {
                        g_observer += payload.size();
                    });
            }
        },
        [&task_executor](std::size_t) { task_executor.wait(); },
        warmup, total, repeats);
    std::cout << "case=taskexecutor_enqueue_full_message ns_per_call="
              << queue_payload_ns << '\n';

    sink_ptr->set_mode(ProfilingSink::Mode::Synchronous);
    const double sync_ns = median_ns_per_call(
        log_prepared,
        [&logger, sink_ptr](std::size_t expected) {
            logger.wait();
            if (sink_ptr->count() != expected) {
                std::cerr << "sync sink count mismatch\n";
                std::exit(3);
            }
            sink_ptr->reset_count();
        },
        warmup, total, repeats);
    std::cout << "case=logger_log_sync_null ns_per_call=" << sync_ns << '\n';

    sink_ptr->set_mode(ProfilingSink::Mode::ConstructAndInvokeTaskOnly);
    const double dispatch_construct_invoke_ns = median_ns_per_call(
        log_prepared,
        [&logger, sink_ptr](std::size_t expected) {
            logger.wait();
            if (sink_ptr->count() != expected) {
                std::cerr << "construct-only sink count mismatch\n";
                std::exit(8);
            }
            sink_ptr->reset_count();
        },
        warmup, total, repeats);
    std::cout << "case=logger_log_construct_and_invoke_full ns_per_call="
              << dispatch_construct_invoke_ns << '\n';

    sink_ptr->set_mode(ProfilingSink::Mode::AsyncFullMessage);
    auto task_completed = std::make_shared<std::atomic<std::size_t>>(0);
    const double enqueue_ns = median_ns_per_call(
        [task_completed](std::size_t count) {
            auto& task_executor = logit::detail::TaskExecutor::get_instance();
            for (std::size_t i = 0; i < count; ++i) {
                task_executor.add_task([task_completed]() {
                    task_completed->fetch_add(1, std::memory_order_relaxed);
                });
            }
        },
        [task_completed](std::size_t expected) {
            logit::detail::TaskExecutor::get_instance().wait();
            const auto completed = task_completed->load(std::memory_order_relaxed);
            if (completed != expected) {
                std::cerr << "TaskExecutor completion mismatch\n";
                std::exit(6);
            }
            g_observer += completed;
            task_completed->store(0, std::memory_order_relaxed);
        }, warmup, total, repeats);
    std::cout << "case=taskexecutor_enqueue_noop ns_per_call=" << enqueue_ns << '\n';

    const double async_prepared_ns = median_ns_per_call(
        log_prepared,
        [&logger, sink_ptr](std::size_t expected) {
            logger.wait();
            if (sink_ptr->count() != expected) {
                std::cerr << "async sink count mismatch\n";
                std::exit(4);
            }
            sink_ptr->reset_count();
        },
        warmup, total, repeats);
    std::cout << "case=logger_log_async_full_prepared ns_per_call="
              << async_prepared_ns << '\n';

    const double full_path_ns = median_ns_per_call(
        [&logger](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                auto record = make_record();
                logger.log(record);
            }
        },
        [&logger, sink_ptr](std::size_t expected) {
            logger.wait();
            if (sink_ptr->count() != expected) {
                std::cerr << "full producer path count mismatch\n";
                std::exit(5);
            }
            sink_ptr->reset_count();
        },
        warmup, total, repeats);
    std::cout << "case=prepared_record_plus_logger_async_full ns_per_call="
              << full_path_ns << '\n';

    std::cout << "observer=" << g_observer << '\n';
    return 0;
}
