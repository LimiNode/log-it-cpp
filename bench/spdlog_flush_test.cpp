#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string_view>

#include "LatencyRecorder.hpp"
#include "Scenario.hpp"
#include "adapters/SpdlogAdapter.hpp"

int main() {
#ifdef _WIN32
    _putenv_s("LOGIT_BENCH_SPDLOG_SINK_DELAY_MS", "2");
#else
    setenv("LOGIT_BENCH_SPDLOG_SINK_DELAY_MS", "2", 1);
#endif

    logit_bench::Scenario scenario;
    scenario.async = true;
    scenario.sink = logit_bench::SinkKind::Null;
    scenario.producers = 1;
    scenario.message_bytes = 1;
    scenario.total_messages = 32;
    scenario.queue_capacity = 8;

    auto recorder = std::make_shared<logit_bench::LatencyRecorder>(
        scenario.total_messages);
    logit_bench::SpdlogAdapter adapter;
    adapter.set_recorder_handle(recorder);
    adapter.prepare(scenario, *recorder);

    for (std::size_t i = 0; i < scenario.total_messages; ++i) {
        const auto token = recorder->begin(true);
        adapter.log(token, std::string_view("x", 1));
    }

    // SpdlogAdapter::flush() must wait for the worker-side flush marker. No
    // additional wait is allowed here: completion is the adapter contract.
    adapter.flush();
    const bool complete = recorder->completed() == scenario.total_messages;
    adapter.set_recorder_handle(nullptr);

#ifdef _WIN32
    _putenv_s("LOGIT_BENCH_SPDLOG_SINK_DELAY_MS", "");
#else
    unsetenv("LOGIT_BENCH_SPDLOG_SINK_DELAY_MS");
#endif
    return complete ? 0 : 1;
}
