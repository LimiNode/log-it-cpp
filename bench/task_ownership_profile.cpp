#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <logit.hpp>
#include <logit/detail/MpscRingAny.hpp>

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
std::atomic<std::size_t> g_completed{0};

std::size_t env_size(const char* name, std::size_t fallback) {
    if (const char* value = std::getenv(name)) {
        try {
            return static_cast<std::size_t>(std::stoull(value));
        } catch (...) {
        }
    }
    return fallback;
}

template <typename Loop, typename Cleanup>
double measure(Loop loop, Cleanup cleanup, std::size_t warmup,
               std::size_t total, std::size_t repeats) {
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

#if defined(LOGIT_USE_MPSC_RING)
double measure_ring_prebuilt_std_function(std::size_t warmup,
                                          std::size_t total,
                                          std::size_t repeats) {
    auto run = [&](std::size_t count) {
        std::vector<std::function<void()>> tasks;
        tasks.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            tasks.emplace_back([]() { ++g_observer; });
        }

        logit::detail::MpscRingAny<std::function<void()>> ring(kQueueCapacity);
        std::atomic<bool> started{false};
        std::atomic<bool> done{false};
        std::atomic<std::size_t> ready{0};
        std::atomic<std::size_t> consumed{0};
        std::thread worker([&]() {
            ready.store(1, std::memory_order_release);
            while (!started.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            std::function<void()> task;
            while (!done.load(std::memory_order_acquire) || !ring.empty()) {
                if (ring.try_pop(task)) {
                    task();
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
        while (ready.load(std::memory_order_acquire) == 0) {
            std::this_thread::yield();
        }

        started.store(true, std::memory_order_release);
        const auto start = std::chrono::steady_clock::now();
        for (auto& task : tasks) {
            while (!ring.try_push(std::move(task))) {
                std::this_thread::yield();
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count();
        done.store(true, std::memory_order_release);
        worker.join();
        if (consumed.load(std::memory_order_relaxed) != count) {
            std::cerr << "prebuilt MPSC ring completion mismatch\n";
            std::exit(6);
        }
        return static_cast<std::uint64_t>(elapsed);
    };

    run(warmup);
    std::vector<std::uint64_t> samples;
    samples.reserve(repeats);
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
        samples.push_back(run(total));
    }
    std::sort(samples.begin(), samples.end());
    return static_cast<double>(samples[samples.size() / 2]) /
           static_cast<double>(total);
}

template <typename T, typename Producer, typename Consumer>
double measure_ring(Producer producer, Consumer consumer, std::size_t warmup,
                    std::size_t total, std::size_t repeats) {
    auto run = [&](std::size_t count) {
        logit::detail::MpscRingAny<T> ring(kQueueCapacity);
        std::atomic<bool> started{false};
        std::atomic<bool> done{false};
        std::atomic<std::size_t> ready{0};
        std::atomic<std::size_t> consumed{0};
        std::thread worker([&]() {
            ready.store(1, std::memory_order_release);
            while (!started.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            T task;
            while (!done.load(std::memory_order_acquire) || !ring.empty()) {
                if (ring.try_pop(task)) {
                    consumer(task);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
        while (ready.load(std::memory_order_acquire) == 0) {
            std::this_thread::yield();
        }

        started.store(true, std::memory_order_release);
        const auto start = std::chrono::steady_clock::now();
        producer(ring, count);
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start).count();
        done.store(true, std::memory_order_release);
        worker.join();
        if (consumed.load(std::memory_order_relaxed) != count) {
            std::cerr << "MPSC ring completion mismatch\n";
            std::exit(5);
        }
        return static_cast<std::uint64_t>(elapsed);
    };

    run(warmup);
    std::vector<std::uint64_t> samples;
    samples.reserve(repeats);
    for (std::size_t repeat = 0; repeat < repeats; ++repeat) {
        samples.push_back(run(total));
    }
    std::sort(samples.begin(), samples.end());
    return static_cast<double>(samples[samples.size() / 2]) /
           static_cast<double>(total);
}
#endif

} // namespace

int main() {
    const std::size_t total = env_size("LOGIT_TASK_OWNERSHIP_TOTAL", kDefaultTotal);
    const std::size_t warmup = env_size("LOGIT_TASK_OWNERSHIP_WARMUP", kDefaultWarmup);
    const std::size_t repeats = env_size("LOGIT_TASK_OWNERSHIP_REPEATS", kDefaultRepeats);
    const std::size_t roundtrip_total = std::min<std::size_t>(
        env_size("LOGIT_TASK_OWNERSHIP_ROUNDTRIP_TOTAL", 1000), total);
    const std::size_t roundtrip_warmup = std::min(roundtrip_total, warmup);
    if (total == 0 || repeats == 0) {
        std::cerr << "total and repeats must be positive\n";
        return 2;
    }

    auto& executor = logit::detail::TaskExecutor::get_instance();
    executor.set_queue_policy(logit::QueuePolicy::Block);
    executor.set_max_queue_size(kQueueCapacity);
    std::function<void()> prebuilt_task = []() {
        g_completed.fetch_add(1, std::memory_order_relaxed);
    };

    std::cout << "task-ownership-profile total=" << total
              << " warmup=" << warmup
              << " repeats=" << repeats
              << " message_bytes=" << kMessage.size()
              << " queue_capacity=" << kQueueCapacity
              << " roundtrip_total=" << roundtrip_total
              << " queue_backend=" << kQueueBackend << '\n';

    const double function_payload_ns = measure(
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
    std::cout << "case=function_construct_payload_invoke ns_per_call="
              << function_payload_ns << '\n';

    const double function_copy_ns = measure(
        [&prebuilt_task](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                std::function<void()> copy = prebuilt_task;
                copy();
            }
        },
        [](std::size_t) {}, warmup, total, repeats);
    std::cout << "case=function_copy_invoke ns_per_call="
              << function_copy_ns << '\n';
    g_completed.store(0, std::memory_order_relaxed);

    const double executor_batch_ns = measure(
        [&executor, &prebuilt_task](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                executor.add_task(prebuilt_task);
            }
        },
        [&executor](std::size_t expected) {
            executor.wait();
            if (g_completed.load(std::memory_order_relaxed) != expected) {
                std::cerr << "TaskExecutor batch completion mismatch\n";
                std::exit(3);
            }
            g_completed.store(0, std::memory_order_relaxed);
        }, warmup, total, repeats);
    std::cout << "case=taskexecutor_batch_prebuilt ns_per_call="
              << executor_batch_ns << '\n';

    const double executor_roundtrip_ns = measure(
        [&executor, &prebuilt_task](std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                executor.add_task(prebuilt_task);
                executor.wait();
            }
        },
        [&executor](std::size_t expected) {
            executor.wait();
            if (g_completed.load(std::memory_order_relaxed) != expected) {
                std::cerr << "TaskExecutor roundtrip completion mismatch\n";
                std::exit(4);
            }
            g_completed.store(0, std::memory_order_relaxed);
        }, roundtrip_warmup, roundtrip_total, repeats);
    std::cout << "case=taskexecutor_roundtrip_prebuilt ns_per_call="
              << executor_roundtrip_ns << '\n';

#if defined(LOGIT_USE_MPSC_RING)
    const double ring_construct_ns = measure_ring<std::function<void()>>(
        [](auto& ring, std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                std::function<void()> value = []() { ++g_observer; };
                while (!ring.try_push(std::move(value))) {
                    std::this_thread::yield();
                }
            }
        },
        [](std::function<void()>& task) { task(); },
        warmup, total, repeats);
    std::cout << "case=mpsc_ring_construct_and_publish_std_function ns_per_call="
              << ring_construct_ns << '\n';

    const double ring_prebuilt_ns = measure_ring_prebuilt_std_function(
        warmup, total, repeats);
    std::cout << "case=mpsc_ring_publish_prebuilt_std_function ns_per_call="
              << ring_prebuilt_ns << '\n';

    const double ring_uint64_ns = measure_ring<std::uint64_t>(
        [](auto& ring, std::size_t count) {
            for (std::size_t i = 0; i < count; ++i) {
                while (!ring.try_push(static_cast<std::uint64_t>(i))) {
                    std::this_thread::yield();
                }
            }
        },
        [](std::uint64_t& value) { g_observer += value; },
        warmup, total, repeats);
    std::cout << "case=mpsc_ring_publish_uint64_control ns_per_call="
              << ring_uint64_ns << '\n';
#else
    std::cout << "case=mpsc_ring_construct_and_publish_std_function ns_per_call=unavailable\n";
    std::cout << "case=mpsc_ring_publish_prebuilt_std_function ns_per_call=unavailable\n";
    std::cout << "case=mpsc_ring_publish_uint64_control ns_per_call=unavailable\n";
#endif

    std::cout << "observer=" << g_observer << '\n';
    return 0;
}
