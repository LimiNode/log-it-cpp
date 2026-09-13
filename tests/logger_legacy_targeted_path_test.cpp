#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <logit.hpp>

namespace {

class CountingLogger final : public logit::ILogger {
public:
    void log(const logit::LogRecord&, const std::string&) override {
        count.fetch_add(1, std::memory_order_relaxed);
    }

    std::string get_string_param(const logit::LoggerParam&) const override { return {}; }
    std::int64_t get_int_param(const logit::LoggerParam&) const override { return 0; }
    double get_float_param(const logit::LoggerParam&) const override { return 0.0; }
    void set_log_level(logit::LogLevel level) override {
        m_level.store(static_cast<int>(level), std::memory_order_relaxed);
    }
    logit::LogLevel get_log_level() const override {
        return static_cast<logit::LogLevel>(m_level.load(std::memory_order_relaxed));
    }
    void wait() override {}

    std::atomic<std::size_t> count{0};

private:
    std::atomic<int> m_level{static_cast<int>(logit::LogLevel::LOG_LVL_TRACE)};
};

} // namespace

int main() {
    auto first = std::unique_ptr<CountingLogger>(new CountingLogger());
    auto second = std::unique_ptr<CountingLogger>(new CountingLogger());
    CountingLogger* first_ptr = first.get();
    CountingLogger* second_ptr = second.get();

    logit::Logger& logger = logit::Logger::get_instance();
    logger.add_logger(
        std::move(first),
        std::unique_ptr<logit::ILogFormatter>(new logit::SimpleLogFormatter("%v")));
    logger.add_logger(
        std::move(second),
        std::unique_ptr<logit::ILogFormatter>(new logit::SimpleLogFormatter("%v")));

    const logit::LogRecord targeted(
        logit::LogLevel::LOG_LVL_INFO, 0, std::string(), 0, std::string(),
        std::string("targeted"), std::string(), 1, false, false);
    logger.log(targeted);

    if (first_ptr->count.load(std::memory_order_relaxed) != 0) return 1;
    if (second_ptr->count.load(std::memory_order_relaxed) != 1) return 2;
    return 0;
}
