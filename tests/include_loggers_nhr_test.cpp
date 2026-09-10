#include <logit/loggers.hpp>
#include <logit/loggers/ConsoleLogger.hpp>

int main() {
    const logit::QueuePolicy policy = logit::QueuePolicy::Block;
    logit::ConsoleLogger logger(false);
    logger.set_log_level(logit::LogLevel::LOG_LVL_WARN);
    logger.wait();
    return policy == logit::QueuePolicy::Block ? 0 : 1;
}
