#pragma once

#include <cstddef>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace logit_bench {

enum class SinkKind {
    Null,
    File,
};

enum class AsyncPayloadMode {
    MarkerOnly,
    FullMessage,
};

// Benchmark-only, library-neutral pipeline counters. The harness records an
// issued call before entering adapter.log() and a sink completion after the
// adapter callback. This compares admission and drain behaviour without
// reaching into either library's private queue.
struct BenchmarkTelemetry {
    std::atomic<std::uint64_t> issued{0};
    std::atomic<std::uint64_t> sink_completed{0};
    std::atomic<std::uint64_t> high_water{0};
    std::atomic<std::uint64_t> last_sink_entry_ns{0};

    static std::uint64_t now_ns() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void on_issued() {
        const auto current = issued.fetch_add(1, std::memory_order_acq_rel) + 1;
        const auto completed_now = sink_completed.load(std::memory_order_acquire);
        const auto outstanding = current > completed_now ? current - completed_now : 0;
        auto observed = high_water.load(std::memory_order_relaxed);
        while (observed < outstanding &&
               !high_water.compare_exchange_weak(observed, outstanding,
                                                  std::memory_order_relaxed,
                                                  std::memory_order_relaxed)) {}
    }

    void on_sink_entry() {
        const auto timestamp = now_ns();
        auto previous = last_sink_entry_ns.load(std::memory_order_relaxed);
        while (previous < timestamp &&
               !last_sink_entry_ns.compare_exchange_weak(previous, timestamp,
                                                         std::memory_order_relaxed,
                                                         std::memory_order_relaxed)) {}
        sink_completed.fetch_add(1, std::memory_order_acq_rel);
    }

    std::uint64_t outstanding() const {
        const auto accepted = issued.load(std::memory_order_acquire);
        const auto completed = sink_completed.load(std::memory_order_acquire);
        return accepted > completed ? accepted - completed : 0;
    }
};

inline std::string sink_name(SinkKind sink) {
    switch (sink) {
        case SinkKind::Null: return "null";
        case SinkKind::File: return "file";
    }
    return "unknown";
}

struct Scenario {
    bool        async          = false;
    SinkKind    sink           = SinkKind::Null;
    AsyncPayloadMode async_payload = AsyncPayloadMode::MarkerOnly;
    std::function<void(std::string_view)> async_payload_observer;
    std::size_t producers      = 1;
    std::size_t message_bytes  = 0;
    std::size_t total_messages = 0;
    std::size_t queue_capacity = 0;
    std::shared_ptr<BenchmarkTelemetry> telemetry;
};

} // namespace logit_bench
