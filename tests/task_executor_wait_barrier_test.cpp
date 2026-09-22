#include <logit.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace {

bool test_wait_blocks_for_running_task(logit::detail::TaskExecutor& executor) {
    std::mutex gate_mutex;
    std::condition_variable gate_cv;
    bool task_started = false;
    bool release_task = false;

    executor.add_task([&]() {
        std::unique_lock<std::mutex> lock(gate_mutex);
        task_started = true;
        gate_cv.notify_all();
        gate_cv.wait(lock, [&]() { return release_task; });
    });

    {
        std::unique_lock<std::mutex> lock(gate_mutex);
        if (!gate_cv.wait_for(lock, std::chrono::seconds(2), [&]() {
                return task_started;
            })) {
            return false;
        }
    }

    std::mutex completion_mutex;
    std::condition_variable completion_cv;
    bool waiter_started = false;
    bool waiter_done = false;

    std::thread waiter([&]() {
        {
            std::lock_guard<std::mutex> lock(completion_mutex);
            waiter_started = true;
        }
        completion_cv.notify_all();

        executor.wait();

        {
            std::lock_guard<std::mutex> lock(completion_mutex);
            waiter_done = true;
        }
        completion_cv.notify_all();
    });

    bool blocked = false;
    {
        std::unique_lock<std::mutex> lock(completion_mutex);
        if (completion_cv.wait_for(lock, std::chrono::seconds(2), [&]() {
                return waiter_started;
            })) {
            blocked = !completion_cv.wait_for(lock, std::chrono::milliseconds(100), [&]() {
                return waiter_done;
            });
        }
    }

    {
        std::lock_guard<std::mutex> lock(gate_mutex);
        release_task = true;
    }
    gate_cv.notify_all();
    waiter.join();

    return blocked && waiter_done;
}

bool test_wait_drains_mpsc_submissions(logit::detail::TaskExecutor& executor) {
    constexpr std::size_t kRounds = 200;
    constexpr std::size_t kProducers = 4;
    constexpr std::size_t kTasksPerProducer = 64;

    for (std::size_t round = 0; round < kRounds; ++round) {
        std::atomic<bool> start(false);
        std::atomic<std::size_t> completed(0);
        std::vector<std::thread> producers;
        producers.reserve(kProducers);

        for (std::size_t producer = 0; producer < kProducers; ++producer) {
            producers.emplace_back([&]() {
                while (!start.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (std::size_t task = 0; task < kTasksPerProducer; ++task) {
                    executor.add_task([&completed]() {
                        completed.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (auto& producer : producers) {
            producer.join();
        }

        executor.wait();
        if (completed.load(std::memory_order_relaxed) !=
            kProducers * kTasksPerProducer) {
            return false;
        }
    }

    return true;
}

bool test_wait_drains_nested_submission(logit::detail::TaskExecutor& executor) {
    std::atomic<std::size_t> completed(0);
    executor.add_task([&]() {
        completed.fetch_add(1, std::memory_order_relaxed);
        executor.add_task([&completed]() {
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    });
    executor.wait();
    return completed.load(std::memory_order_relaxed) == 2;
}

} // namespace

int main() {
    auto& executor = logit::detail::TaskExecutor::get_instance();
    executor.wait();
    executor.set_queue_policy(logit::detail::QueuePolicy::Block);
    executor.set_max_queue_size(0);
    executor.reset_dropped_tasks();

    const bool running_task = test_wait_blocks_for_running_task(executor);
    executor.wait();
    const bool mpsc_submissions = test_wait_drains_mpsc_submissions(executor);
    const bool nested_submission = test_wait_drains_nested_submission(executor);

    executor.wait();
    executor.reset_dropped_tasks();

    const bool ok = running_task && mpsc_submissions && nested_submission;
    std::cout << (ok ? "PASS" : "FAIL")
              << ": task_executor_wait_barrier" << std::endl;
    return ok ? 0 : 1;
}
