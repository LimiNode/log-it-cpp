#pragma once

#ifndef LOGIT_CPP_HEADER_BENCH_BENCHMARK_METADATA_HPP_INCLUDED
#define LOGIT_CPP_HEADER_BENCH_BENCHMARK_METADATA_HPP_INCLUDED

#include <cstddef>
#include <cstdlib>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace logit_bench {

struct BenchmarkMetadata {
    std::string source_commit;
    std::string compiler;
    std::string compiler_version;
    std::string toolchain;
    std::string cxx_standard;
    std::string platform;
    std::string build_type;
    std::string architecture;
    std::string machine_id;
    std::string cpu_model;
    std::string queue_capacity;
    std::string queue_policy;
    std::string latency_completion;
    std::string flush_barrier;
};

inline std::string benchmark_env(const char* name, const char* fallback) {
    if (const char* value = std::getenv(name)) {
        if (*value != '\0') return value;
    }
    return fallback;
}

inline std::string benchmark_label(const char* name, const char* fallback) {
    std::string value = benchmark_env(name, fallback);
    for (char& character : value) {
        if (character == ' ' || character == '\t' ||
            character == '\r' || character == '\n') {
            character = '_';
        }
    }
    return value;
}

inline std::string benchmark_compiler() {
#if defined(_MSC_VER)
    return "MSVC";
#elif defined(__clang__)
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "unknown";
#endif
}

inline std::string benchmark_compiler_version() {
#if defined(_MSC_VER)
    return std::to_string(_MSC_VER);
#elif defined(__clang__)
    return std::to_string(__clang_major__) + "." +
           std::to_string(__clang_minor__) + "." +
           std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return std::to_string(__GNUC__) + "." +
           std::to_string(__GNUC_MINOR__) + "." +
           std::to_string(__GNUC_PATCHLEVEL__);
#else
    return "unknown";
#endif
}

inline std::string benchmark_source_commit() {
    if (const char* value = std::getenv("LOGIT_BENCH_COMMIT")) {
        if (*value != '\0') return value;
    }
    if (const char* value = std::getenv("GITHUB_SHA")) {
        if (*value != '\0') return value;
    }
    return "unknown";
}

inline std::string benchmark_cxx_standard() {
#if defined(_MSVC_LANG)
    return std::to_string(_MSVC_LANG);
#else
    return std::to_string(__cplusplus);
#endif
}

inline std::string benchmark_platform() {
#if defined(__EMSCRIPTEN__)
    return "emscripten";
#elif defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

inline std::string benchmark_architecture() {
#if defined(__EMSCRIPTEN__) && defined(__wasm32__)
    return "wasm32";
#elif defined(_M_X64) || defined(__x86_64__)
    return "x86_64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif defined(_M_ARM) || defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}

inline std::string benchmark_build_type() {
    if (const char* value = std::getenv("LOGIT_BENCH_BUILD_TYPE")) {
        if (*value != '\0') return value;
    }
#ifdef LOGIT_BENCH_BUILD_TYPE
    return LOGIT_BENCH_BUILD_TYPE;
#else
    return "unknown";
#endif
}

inline BenchmarkMetadata make_benchmark_metadata(
        std::string queue_capacity,
        std::string queue_policy,
        std::string latency_completion,
        std::string flush_barrier) {
    const std::string compiler = benchmark_compiler();
    const std::string compiler_version = benchmark_compiler_version();
    std::string toolchain = compiler + "-" + compiler_version;
    toolchain = benchmark_label("LOGIT_BENCH_TOOLCHAIN", toolchain.c_str());
    return BenchmarkMetadata{
        benchmark_source_commit(),
        compiler,
        compiler_version,
        std::move(toolchain),
        benchmark_cxx_standard(),
        benchmark_platform(),
        benchmark_build_type(),
        benchmark_architecture(),
        benchmark_label("LOGIT_BENCH_MACHINE_ID", "unknown"),
        benchmark_label("LOGIT_BENCH_CPU_MODEL", "unknown"),
        std::move(queue_capacity),
        std::move(queue_policy),
        std::move(latency_completion),
        std::move(flush_barrier)};
}

inline bool benchmark_value_unknown(const std::string& value) {
    return value.empty() || value == "unknown";
}

inline void validate_comparable_metadata(const BenchmarkMetadata& metadata,
                                         bool require_comparable) {
    if (!require_comparable) return;

    struct MetadataField {
        const char* name;
        const std::string* value;
        bool allow_not_applicable;
    };
    const MetadataField values[] = {
        {"source_commit", &metadata.source_commit, false},
        {"compiler", &metadata.compiler, false},
        {"compiler_version", &metadata.compiler_version, false},
        {"toolchain", &metadata.toolchain, false},
        {"cxx_standard", &metadata.cxx_standard, false},
        {"platform", &metadata.platform, false},
        {"build_type", &metadata.build_type, false},
        {"architecture", &metadata.architecture, false},
        {"machine_id", &metadata.machine_id, false},
        {"cpu_model", &metadata.cpu_model, false},
        {"queue_capacity", &metadata.queue_capacity, true},
        {"queue_policy", &metadata.queue_policy, true},
        {"latency_completion", &metadata.latency_completion, false},
        {"flush_barrier", &metadata.flush_barrier, false}};

    for (const auto& value : values) {
        if (benchmark_value_unknown(*value.value) ||
            (!value.allow_not_applicable && *value.value == "not-applicable")) {
            throw std::runtime_error(
                std::string("Comparable benchmark requires metadata field '") +
                value.name + "'; set the corresponding LOGIT_BENCH_* value or disable "
                "LOGIT_BENCH_REQUIRE_COMPARABLE");
        }
    }
}

inline void validate_comparable_metadata(const BenchmarkMetadata& metadata) {
    const char* required = std::getenv("LOGIT_BENCH_REQUIRE_COMPARABLE");
    validate_comparable_metadata(
        metadata, required && std::string(required) == "1");
}

inline void print_benchmark_metadata(std::ostream& out,
                                     const BenchmarkMetadata& metadata,
                                     std::size_t total_messages,
                                     std::size_t warmup_messages) {
    out << "benchmark-fixture version=1"
        << " source_commit=" << metadata.source_commit
        << " compiler=" << metadata.compiler
        << " compiler_version=" << metadata.compiler_version
        << " toolchain=" << metadata.toolchain
        << " cxx_standard=" << metadata.cxx_standard
        << " platform=" << metadata.platform
        << " build_type=" << metadata.build_type
        << " architecture=" << metadata.architecture
        << " machine_id=" << metadata.machine_id
        << " cpu_model=" << metadata.cpu_model
        << " queue_capacity=" << metadata.queue_capacity
        << " queue_policy=" << metadata.queue_policy
        << " latency_completion=" << metadata.latency_completion
        << " flush_barrier=" << metadata.flush_barrier
        << " total=" << total_messages
        << " warmup=" << warmup_messages << '\n';
}

} // namespace logit_bench

#endif // LOGIT_CPP_HEADER_BENCH_BENCHMARK_METADATA_HPP_INCLUDED
