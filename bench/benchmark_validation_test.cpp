#include "BenchmarkValidation.hpp"
#include "BenchmarkMetadata.hpp"

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
    if (!rejected_capacity) return 1;
    validate_queue_capacity(1);

    bool rejected_legacy_schema = false;
    try {
        validate_latency_csv_header(
            "lib,async,sink,producers,msg_bytes,total,p50_ns,p99_ns,p999_ns,throughput");
    } catch (const std::runtime_error&) {
        rejected_legacy_schema = true;
    }
    if (!rejected_legacy_schema) return 2;

    validate_latency_csv_header(std::string(latency_csv_header()) + "\r");
    validate_latency_csv_header(latency_csv_header());

    const auto comparable = make_benchmark_metadata(
        "8192", "block", "sink-entry", "all-prior-work-drained");
    auto complete = comparable;
    complete.source_commit = "test-commit";
    complete.compiler = "test-compiler";
    complete.compiler_version = "1";
    complete.toolchain = "test-toolchain";
    complete.cxx_standard = "201703";
    complete.platform = "test-platform";
    complete.build_type = "Release";
    complete.architecture = "test-architecture";
    complete.machine_id = "test-machine";
    complete.cpu_model = "test-cpu";
    validate_comparable_metadata(complete, true);

    auto incomplete = complete;
    incomplete.source_commit = "unknown";
    bool rejected_unknown_metadata = false;
    try {
        validate_comparable_metadata(incomplete, true);
    } catch (const std::runtime_error&) {
        rejected_unknown_metadata = true;
    }
    if (!rejected_unknown_metadata) return 3;

    return 0;
}
