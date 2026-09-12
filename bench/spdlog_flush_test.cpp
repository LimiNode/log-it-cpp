#include <cstddef>
#include <memory>
#include <string>

#include "LatencyRecorder.hpp"
#include "Scenario.hpp"
#include "adapters/SpdlogAdapter.hpp"

int main() {
    logit_bench::Scenario scenario;
    scenario.async = true;
    scenario.sink = logit_bench::SinkKind::Null;
    scenario.producers = 1;
    scenario.message_bytes = 1;
    scenario.total_messages = 64;
    scenario.queue_capacity = 8;

    logit_bench::SpdlogAdapter adapter;
    auto recorder = std::make_shared<logit_bench::LatencyRecorder>(
        scenario.total_messages);
    adapter.set_recorder_handle(recorder);
    adapter.prepare(scenario, *recorder);

    for (std::size_t i = 0; i < scenario.total_messages; ++i) {
        const auto token = recorder->begin(true);
        adapter.log(token, std::string_view("x", 1));
    }

    adapter.flush();
    return recorder->completed() == scenario.total_messages ? 0 : 1;
}
