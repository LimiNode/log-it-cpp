#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "BenchmarkMetadata.hpp"
#include "LatencyRecorder.hpp"
#include "Scenario.hpp"
#include "adapters/ILoggerAdapter.hpp"
#include "adapters/LogItAdapter.hpp"
#ifdef LOGIT_BENCH_HAVE_SPDLOG
#include "adapters/SpdlogAdapter.hpp"
#endif

namespace logit_bench {
namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string mode = "matrix";
    std::size_t total = 200000;
    std::size_t warmup = 4096;
    std::size_t repeats = 5;
    std::size_t bytes = 200;
    std::vector<std::size_t> producers{1, 2, 4, 8};
    std::vector<std::size_t> queues{1024, 8192, 65536, 400000};
    std::vector<std::size_t> rates{100000, 250000, 500000, 750000, 1000000};
    std::filesystem::path csv = "bench/results/pipeline-research.csv";
    std::filesystem::path jsonl = "bench/results/pipeline-research.jsonl";
    std::filesystem::path aggregate = "bench/results/pipeline-research-aggregate.csv";
};

std::size_t env_size(const char* name, std::size_t fallback) {
    if (const char* value = std::getenv(name)) {
        try { return static_cast<std::size_t>(std::stoull(value)); }
        catch (...) {}
    }
    return fallback;
}

std::string env_string(const char* name, const char* fallback) {
    if (const char* value = std::getenv(name); value && *value) return value;
    return fallback;
}

std::vector<std::size_t> parse_list(const char* name,
                                    std::vector<std::size_t> fallback) {
    const char* value = std::getenv(name);
    if (!value || !*value) return fallback;
    std::vector<std::size_t> result;
    std::stringstream input(value);
    std::string item;
    while (std::getline(input, item, ',')) {
        try { result.push_back(static_cast<std::size_t>(std::stoull(item))); }
        catch (...) { throw std::invalid_argument(std::string("Invalid list in ") + name); }
    }
    if (result.empty()) throw std::invalid_argument(std::string("Empty list in ") + name);
    return result;
}

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count());
}

std::string message_for(std::size_t bytes) {
    return std::string(bytes, 'X');
}

struct StageResult {
    LatencyRecorder::Summary producer_call;
    LatencyRecorder::Summary sink_entry;
    std::uint64_t producer_phase_ns = 0;
    std::uint64_t drain_tail_ns = 0;
    std::uint64_t total_wall_ns = 0;
    double throughput = 0.0;
    std::uint64_t outstanding_high_water = 0;
    std::uint64_t outstanding_at_producer_done = 0;
    std::uint64_t submitted = 0;
    std::uint64_t sink_completed = 0;
};

struct RunRecord {
    std::string mode;
    std::size_t repeat = 0;
    std::size_t run_index = 0;
    std::string run_order;
    std::string library;
    std::size_t producers = 0;
    std::size_t queue = 0;
    std::size_t total = 0;
    std::size_t warmup = 0;
    std::size_t bytes = 0;
    std::size_t offered_rate = 0;
    StageResult result;
};

void validate_csv(const std::filesystem::path& path, const std::string& header) {
    namespace fs = std::filesystem;
    if (path.parent_path() != fs::path()) fs::create_directories(path.parent_path());
    if (!fs::exists(path) || fs::file_size(path) == 0) return;
    std::ifstream input(path);
    std::string actual;
    if (!input || !std::getline(input, actual) || actual != header) {
        throw std::runtime_error("Unsupported pipeline research CSV schema; rename or remove the existing file");
    }
}

const char* csv_header() {
    return "mode,repeat,run_index,run_order,library,producers,queue_capacity,total,warmup,msg_bytes,offered_rate,"
           "producer_p50_ns,producer_p99_ns,producer_p999_ns,sink_p50_ns,sink_p99_ns,sink_p999_ns,"
           "producer_phase_ns,drain_tail_ns,total_wall_ns,throughput,outstanding_high_water,"
           "outstanding_at_producer_done,submitted,sink_completed,source_commit,compiler,compiler_version,"
           "toolchain,cxx_standard,platform,build_type,architecture,machine_id,cpu_model,queue_policy,"
           "latency_completion,flush_barrier,workload_contract";
}

void append_record(const Options& options, const RunRecord& r,
                   const BenchmarkMetadata& metadata) {
    validate_csv(options.csv, csv_header());
    const bool header = !std::filesystem::exists(options.csv) ||
                        std::filesystem::file_size(options.csv) == 0;
    std::ofstream out(options.csv, std::ios::app);
    if (!out) throw std::runtime_error("Failed to open pipeline research CSV");
    if (header) out << csv_header() << '\n';
    const auto& p = r.result.producer_call;
    const auto& s = r.result.sink_entry;
    out << r.mode << ',' << r.repeat << ',' << r.run_index << ',' << r.run_order << ','
        << r.library << ',' << r.producers << ',' << r.queue << ',' << r.total << ','
        << r.warmup << ',' << r.bytes << ',' << r.offered_rate << ','
        << p.p50_ns << ',' << p.p99_ns << ',' << p.p999_ns << ','
        << s.p50_ns << ',' << s.p99_ns << ',' << s.p999_ns << ','
        << r.result.producer_phase_ns << ',' << r.result.drain_tail_ns << ','
        << r.result.total_wall_ns << ',' << std::fixed << std::setprecision(2)
        << r.result.throughput << ',' << r.result.outstanding_high_water << ','
        << r.result.outstanding_at_producer_done << ',' << r.result.submitted << ','
        << r.result.sink_completed << ',' << metadata.source_commit << ',' << metadata.compiler << ','
        << metadata.compiler_version << ',' << metadata.toolchain << ',' << metadata.cxx_standard << ','
        << metadata.platform << ',' << metadata.build_type << ',' << metadata.architecture << ','
        << metadata.machine_id << ',' << metadata.cpu_model << ',' << metadata.queue_policy << ','
        << metadata.latency_completion << ',' << metadata.flush_barrier << ','
        << metadata.workload_contract << '\n';

    if (options.jsonl.parent_path() != std::filesystem::path())
        std::filesystem::create_directories(options.jsonl.parent_path());
    std::ofstream json(options.jsonl, std::ios::app);
    if (!json) throw std::runtime_error("Failed to open pipeline research JSONL");
    json << "{\"fixture_version\":2,\"source_commit\":\"" << metadata.source_commit
         << "\",\"compiler\":\"" << metadata.compiler
         << "\",\"compiler_version\":\"" << metadata.compiler_version
         << "\",\"toolchain\":\"" << metadata.toolchain
         << "\",\"cxx_standard\":\"" << metadata.cxx_standard
         << "\",\"platform\":\"" << metadata.platform
         << "\",\"build_type\":\"" << metadata.build_type
         << "\",\"architecture\":\"" << metadata.architecture
         << "\",\"machine_id\":\"" << metadata.machine_id
         << "\",\"cpu_model\":\"" << metadata.cpu_model
         << "\",\"queue_policy\":\"" << metadata.queue_policy
         << "\",\"latency_completion\":\"" << metadata.latency_completion
         << "\",\"flush_barrier\":\"" << metadata.flush_barrier
         << "\",\"workload_contract\":\"" << metadata.workload_contract
         << "\",\"mode\":\"" << r.mode << "\",\"repeat\":"
         << r.repeat << ",\"run_index\":" << r.run_index << ",\"run_order\":\""
         << r.run_order << "\",\"library\":\"" << r.library << "\",\"producers\":"
         << r.producers << ",\"queue_capacity\":" << r.queue << ",\"total\":"
         << r.total << ",\"warmup\":" << r.warmup << ",\"msg_bytes\":" << r.bytes
         << ",\"offered_rate\":" << r.offered_rate
         << ",\"producer_p50_ns\":" << p.p50_ns << ",\"producer_p99_ns\":" << p.p99_ns
         << ",\"producer_p999_ns\":" << p.p999_ns << ",\"sink_p50_ns\":" << s.p50_ns
         << ",\"sink_p99_ns\":" << s.p99_ns << ",\"sink_p999_ns\":" << s.p999_ns
         << ",\"producer_phase_ns\":" << r.result.producer_phase_ns
         << ",\"drain_tail_ns\":" << r.result.drain_tail_ns
         << ",\"total_wall_ns\":" << r.result.total_wall_ns
         << ",\"throughput\":" << std::fixed << std::setprecision(2) << r.result.throughput
         << ",\"outstanding_high_water\":" << r.result.outstanding_high_water
         << ",\"outstanding_at_producer_done\":" << r.result.outstanding_at_producer_done
         << ",\"submitted\":" << r.result.submitted << ",\"sink_completed\":"
         << r.result.sink_completed << "}\n";
}

void run_warmup(ILoggerAdapter& adapter, const Scenario& scenario,
                std::string_view message, std::size_t count) {
    std::mutex mx;
    std::condition_variable cv;
    std::size_t ready = 0;
    bool released = false;
    std::vector<std::thread> threads;
    threads.reserve(scenario.producers);
    for (std::size_t producer = 0; producer < scenario.producers; ++producer) {
        threads.emplace_back([&, producer]() {
            const auto share = count / scenario.producers +
                (producer < count % scenario.producers ? 1 : 0);
            {
                std::unique_lock<std::mutex> lock(mx);
                ++ready;
                cv.notify_all();
                cv.wait(lock, [&] { return released; });
            }
            for (std::size_t n = 0; n < share; ++n) {
                adapter.log(LatencyRecorder::Token{}, message);
            }
        });
    }
    {
        std::unique_lock<std::mutex> lock(mx);
        cv.wait(lock, [&] { return ready == scenario.producers; });
        released = true;
        cv.notify_all();
    }
    for (auto& thread : threads) thread.join();
}

StageResult run_once(ILoggerAdapter& adapter, Scenario scenario,
                     std::size_t warmup, std::size_t offered_rate) {
    auto warmup_recorder = std::make_shared<LatencyRecorder>(1);
    scenario.telemetry.reset();
    adapter.prepare(scenario, *warmup_recorder);
    const auto message = message_for(scenario.message_bytes);

    run_warmup(adapter, scenario, message, warmup);
    adapter.flush();

    auto sink_recorder = std::make_shared<LatencyRecorder>(scenario.total_messages);
    auto producer_recorder = std::make_shared<LatencyRecorder>(scenario.total_messages);
    auto telemetry = std::make_shared<BenchmarkTelemetry>();
    scenario.telemetry = telemetry;
    adapter.prepare(scenario, *sink_recorder);
    std::uint64_t start = 0;
    std::atomic<std::uint64_t> last_producer_done{0};
    std::mutex producer_done_mx;
    std::uint64_t outstanding_at_done_snapshot = 0;
    std::mutex mx;
    std::condition_variable cv;
    std::size_t ready = 0;
    bool released = false;
    std::atomic<std::uint64_t> next_ticket{0};
    const auto interval = offered_rate ? (1'000'000'000ULL / offered_rate) : 0;
    std::vector<std::thread> threads;
    threads.reserve(scenario.producers);
    for (std::size_t producer = 0; producer < scenario.producers; ++producer) {
        threads.emplace_back([&, producer]() {
            const auto share = scenario.total_messages / scenario.producers +
                (producer < scenario.total_messages % scenario.producers ? 1 : 0);
            {
                std::unique_lock<std::mutex> lock(mx);
                ++ready;
                cv.notify_all();
                cv.wait(lock, [&] { return released; });
            }
            for (std::size_t n = 0; n < share; ++n) {
                if (offered_rate) {
                    const auto ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
                    const auto target = start + ticket * interval;
                    for (;;) {
                        const auto current = now_ns();
                        if (current >= target) break;
                        const auto remaining = target - current;
                        if (remaining > 200'000) std::this_thread::sleep_for(
                            std::chrono::nanoseconds(remaining / 2));
                        else std::this_thread::yield();
                    }
                }
                auto sink_token = sink_recorder->begin(true);
                telemetry->on_submitted();
                auto producer_token = producer_recorder->begin(true);
                adapter.log(sink_token, message);
                const auto returned = now_ns();
                if (n + 1 == share) {
                    std::lock_guard<std::mutex> lock(producer_done_mx);
                    if (returned > last_producer_done.load(std::memory_order_relaxed)) {
                        last_producer_done.store(returned, std::memory_order_relaxed);
                        outstanding_at_done_snapshot = telemetry->outstanding();
                    }
                }
                producer_recorder->complete(producer_token);
            }
        });
    }
    {
        std::unique_lock<std::mutex> lock(mx);
        cv.wait(lock, [&] { return ready == scenario.producers; });
        start = now_ns();
        released = true;
        cv.notify_all();
    }
    for (auto& thread : threads) thread.join();
    const auto producer_done = last_producer_done.load(std::memory_order_acquire);
    const auto outstanding_at_done = outstanding_at_done_snapshot;
    adapter.flush();
    const auto end = now_ns();
    sink_recorder->wait_for_all();
    producer_recorder->wait_for_all();

    StageResult result;
    result.producer_call = producer_recorder->finalize();
    result.sink_entry = sink_recorder->finalize();
    result.producer_phase_ns = producer_done > start ? producer_done - start : 0;
    const auto last_sink = telemetry->last_sink_entry_ns.load(std::memory_order_acquire);
    result.drain_tail_ns = last_sink > producer_done ? last_sink - producer_done : 0;
    result.total_wall_ns = end > start ? end - start : 0;
    result.throughput = result.total_wall_ns
        ? static_cast<double>(scenario.total_messages) * 1'000'000'000.0 /
          static_cast<double>(result.total_wall_ns) : 0.0;
    result.outstanding_high_water = telemetry->high_water.load(std::memory_order_acquire);
    result.outstanding_at_producer_done = outstanding_at_done;
    result.submitted = telemetry->submitted.load(std::memory_order_acquire);
    result.sink_completed = telemetry->sink_completed.load(std::memory_order_acquire);
    if (result.submitted != scenario.total_messages ||
        result.sink_completed != scenario.total_messages) {
        throw std::runtime_error("pipeline telemetry did not drain all messages");
    }
    return result;
}

double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

void write_aggregate(const Options& options, const std::vector<RunRecord>& records) {
    struct Group { std::string mode, library; std::size_t producers{}, queue{}, rate{}; std::vector<double> throughput, sink_p50, producer_p50; };
    std::vector<Group> groups;
    for (const auto& record : records) {
        auto it = std::find_if(groups.begin(), groups.end(), [&](const Group& g) {
            return g.mode == record.mode && g.library == record.library &&
                   g.producers == record.producers && g.queue == record.queue &&
                   g.rate == record.offered_rate;
        });
        if (it == groups.end()) {
            groups.push_back(Group{record.mode, record.library, record.producers,
                                   record.queue, record.offered_rate});
            it = groups.end() - 1;
        }
        it->throughput.push_back(record.result.throughput);
        it->sink_p50.push_back(static_cast<double>(record.result.sink_entry.p50_ns));
        it->producer_p50.push_back(static_cast<double>(record.result.producer_call.p50_ns));
    }
    if (options.aggregate.parent_path() != std::filesystem::path())
        std::filesystem::create_directories(options.aggregate.parent_path());
    std::ofstream out(options.aggregate);
    out << "mode,library,producers,queue_capacity,offered_rate,repeats,median_producer_p50_ns,"
           "median_sink_p50_ns,median_throughput\n";
    for (const auto& group : groups) {
        out << group.mode << ',' << group.library << ',' << group.producers << ',' << group.queue
            << ',' << group.rate << ',' << group.throughput.size() << ','
            << median(group.producer_p50) << ',' << median(group.sink_p50) << ','
            << std::fixed << std::setprecision(2) << median(group.throughput) << '\n';
    }
}

} // namespace
} // namespace logit_bench

int main() {
    using namespace logit_bench;
#ifndef LOGIT_BENCH_HAVE_SPDLOG
    std::cerr << "pipeline research requires LOGIT_BENCH_WITH_SPDLOG=ON\n";
    return 2;
#else
    try {
        Options options;
        options.mode = env_string("LOGIT_BENCH_RESEARCH_MODE", "matrix");
        options.total = env_size("LOGIT_BENCH_TOTAL", options.total);
        options.warmup = env_size("LOGIT_BENCH_WARMUP", options.warmup);
        options.repeats = env_size("LOGIT_BENCH_REPEATS", options.repeats);
        options.bytes = env_size("LOGIT_BENCH_BYTES", options.bytes);
        options.producers = parse_list("LOGIT_BENCH_PRODUCERS", options.producers);
        options.queues = parse_list("LOGIT_BENCH_QUEUE_CAPACITIES", options.queues);
        options.rates = parse_list("LOGIT_BENCH_RATES", options.rates);
        options.csv = env_string("LOGIT_BENCH_RESEARCH_CSV", options.csv.string().c_str());
        options.jsonl = env_string("LOGIT_BENCH_RESEARCH_JSONL", options.jsonl.string().c_str());
        options.aggregate = env_string("LOGIT_BENCH_RESEARCH_AGGREGATE", options.aggregate.string().c_str());
        if (options.repeats == 0 || options.total == 0 || options.producers.empty())
            throw std::invalid_argument("research totals, repeats, and producers must be non-zero");

        const auto metadata = make_benchmark_metadata(
            "matrix", "block", "sink-entry", "all-prior-work-drained",
            "prepared-message/async-full-message");
        validate_comparable_metadata(metadata);
        std::vector<RunRecord> records;
        auto logit_adapter = std::make_unique<LogItAdapter>();
        auto spdlog_adapter = std::make_unique<SpdlogAdapter>();
        for (std::size_t repeat = 1; repeat <= options.repeats; ++repeat) {
            std::vector<std::string> order{"log-it-cpp", "spdlog"};
            if ((repeat % 2) == 0) std::swap(order[0], order[1]);
            for (const auto& library : order) {
                ILoggerAdapter& adapter = library == "log-it-cpp"
                    ? static_cast<ILoggerAdapter&>(*logit_adapter)
                    : static_cast<ILoggerAdapter&>(*spdlog_adapter);
                const auto run_mode = options.mode == "rate" ? "rate" : "matrix";
                const auto& rates = run_mode == std::string("rate") ? options.rates : std::vector<std::size_t>{0};
                for (const auto rate : rates) {
                    for (const auto producers : options.producers) {
                        const auto queues = run_mode == std::string("rate") ?
                            std::vector<std::size_t>{400000} : options.queues;
                        for (const auto queue : queues) {
                            Scenario scenario;
                            scenario.async = true;
                            scenario.sink = SinkKind::Null;
                            scenario.async_payload = AsyncPayloadMode::FullMessage;
                            scenario.producers = producers;
                            scenario.message_bytes = options.bytes;
                            scenario.total_messages = options.total;
                            scenario.queue_capacity = queue;
                            LOGIT_SET_MAX_QUEUE(queue);
                            LOGIT_SET_QUEUE_POLICY(LOGIT_QUEUE_BLOCK);
                            auto result = run_once(adapter, scenario, options.warmup, rate);
                            RunRecord record{run_mode, repeat, records.size() + 1,
                                             (repeat % 2) ? "logit-then-spdlog" : "spdlog-then-logit",
                                             library, producers, queue, options.total,
                                             options.warmup, options.bytes, rate, result};
                            append_record(options, record, metadata);
                            records.push_back(std::move(record));
                            std::cout << library << " mode=" << run_mode << " repeat=" << repeat
                                      << " producers=" << producers << " queue=" << queue
                                      << " rate=" << rate << " sink_p50=" << result.sink_entry.p50_ns
                                      << " producer_p50=" << result.producer_call.p50_ns
                                      << " throughput=" << std::fixed << std::setprecision(2)
                                      << result.throughput << " outstanding_high_water="
                                      << result.outstanding_high_water << '\n';
                        }
                    }
                }
            }
        }
        write_aggregate(options, records);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Pipeline research failed: " << ex.what() << '\n';
        return 1;
    }
#endif
}
