#pragma once

#include <cstddef>
#include <functional>
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
};

} // namespace logit_bench
