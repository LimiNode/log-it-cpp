#include "adapters/LogItAdapter.hpp"

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>

int main() {
    using namespace logit_bench;

    const std::string expected(200, 'P');
    std::mutex observer_mutex;
    std::string observed;

    Scenario scenario;
    scenario.async = true;
    scenario.sink = SinkKind::Null;
    scenario.async_payload = AsyncPayloadMode::FullMessage;
    scenario.async_payload_observer = [&](std::string_view payload) {
        std::lock_guard<std::mutex> lock(observer_mutex);
        observed.assign(payload.data(), payload.size());
    };

    LatencyRecorder recorder(1);
    LogItAdapter adapter;
    adapter.prepare(scenario, recorder);
    adapter.log(recorder.begin(true), expected);
    adapter.flush();
    recorder.wait_for_all();

    std::lock_guard<std::mutex> lock(observer_mutex);
    return observed == expected ? 0 : 1;
}
