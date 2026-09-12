#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace logit_bench {

inline void validate_queue_capacity(std::size_t capacity) {
    if (capacity == 0) {
        throw std::invalid_argument(
            "LOGIT_BENCH_QUEUE_CAPACITY must be greater than zero for a comparative benchmark");
    }
}

inline const char* latency_csv_header() {
    return "lib,async,sink,producers,msg_bytes,total,queue_capacity,"
           "p50_ns,p99_ns,p999_ns,throughput";
}

inline void validate_latency_csv_header(std::string header) {
    if (!header.empty() && header.back() == '\r') {
        header.pop_back();
    }
    if (header != latency_csv_header()) {
        throw std::runtime_error(
            "Unsupported bench/results/latency.csv schema; rename or remove "
            "the existing file before running this benchmark");
    }
}

} // namespace logit_bench
