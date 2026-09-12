#include "BenchmarkValidation.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

int main() {
    using namespace logit_bench;

    bool rejected_capacity = false;
    try {
        validate_queue_capacity(0);
    } catch (const std::invalid_argument&) {
        rejected_capacity = true;
    }
    assert(rejected_capacity);
    validate_queue_capacity(1);

    bool rejected_legacy_schema = false;
    try {
        validate_latency_csv_header(
            "lib,async,sink,producers,msg_bytes,total,p50_ns,p99_ns,p999_ns,throughput");
    } catch (const std::runtime_error&) {
        rejected_legacy_schema = true;
    }
    assert(rejected_legacy_schema);

    validate_latency_csv_header(std::string(latency_csv_header()) + "\r");
    validate_latency_csv_header(latency_csv_header());
    return 0;
}
