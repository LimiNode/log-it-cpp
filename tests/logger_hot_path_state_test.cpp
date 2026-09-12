#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <logit.hpp>

namespace {
class CountingLogger final : public logit::ILogger {
public:
    void log(const logit::LogRecord&, const std::string&) override {
        count.fetch_add(1, std::memory_order_relaxed);
    }

    std::string get_string_param(const logit::LoggerParam&) const override { return std::string(); }
    int64_t get_int_param(const logit::LoggerParam&) const override { return 0; }
    double get_float_param(const logit::LoggerParam&) const override { return 0.0; }
    void set_log_level(logit::LogLevel level) override {
        m_level.store(static_cast<int>(level), std::memory_order_relaxed);
    }
    logit::LogLevel get_log_level() const override {
        return static_cast<logit::LogLevel>(m_level.load(std::memory_order_relaxed));
    }
    void wait() override {}

    std::atomic<std::uint64_t> count{0};

private:
    std::atomic<int> m_level{static_cast<int>(logit::LogLevel::LOG_LVL_TRACE)};
};
} // namespace

int main() {
    CountingLogger* backend = new CountingLogger();
    logit::Logger& logger = logit::Logger::get_instance();
    logger.add_logger(
        std::unique_ptr<logit::ILogger>(backend),
        std::unique_ptr<logit::ILogFormatter>(new logit::SimpleLogFormatter("%v")));

    const logit::LogRecord record(
        logit::LogLevel::LOG_LVL_INFO, 0, std::string(), 0, std::string(),
        std::string("hot-path"), std::string(), -1, false, false);

    std::thread producer([&]() {
        for (int i = 0; i < 20000; ++i) {
            logger.log(record);
        }
    });

    for (int i = 0; i < 20000; ++i) {
        logger.set_logger_enabled(0, (i & 1) == 0);
        logger.set_logger_single_mode(0, false);
    }
    producer.join();

    logger.set_logger_enabled(0, true);
    logger.log(record);
    return backend->count.load(std::memory_order_relaxed) == 0 ? 1 : 0;
}
