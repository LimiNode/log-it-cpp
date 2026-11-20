#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGER_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGER_HPP_INCLUDED

/// \file Logger.hpp
/// \brief Defines the Logger class for managing multiple loggers and formatters.

#include "config.hpp"
#include "configuration_registration.hpp"
#include "loggers/ILogger.hpp"
#include "formatter.hpp"
#include "detail/TaskExecutor.hpp"
#include <memory>
#include <mutex>
#include <sstream>
#include <atomic>
#include <cstddef>
#include <climits>
#include <map>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

#if __cplusplus >= 201703L
#include <shared_mutex>
#define LOGIT_HAS_SHARED_MUTEX 1
#else
#define LOGIT_HAS_SHARED_MUTEX 0
#endif

namespace logit {

    namespace detail {
        /// \brief Private regression access without a production singleton reset API.
        struct LoggerRegistrationTestAccess;
    }

#if LOGIT_HAS_SHARED_MUTEX
    using LoggerMutex = std::shared_mutex;
    using LoggerReadLock = std::shared_lock<LoggerMutex>;
    using LoggerWriteLock = std::unique_lock<LoggerMutex>;
#else
    using LoggerMutex = std::mutex;
    using LoggerReadLock = std::unique_lock<LoggerMutex>;
    using LoggerWriteLock = std::unique_lock<LoggerMutex>;
#endif

    /// \class Logger
    /// \brief Singleton class managing multiple loggers and formatters.
    ///
    /// Allows adding multiple logger and formatter pairs.
    /// Provides methods to log messages using these strategies and supports
    /// both synchronous and asynchronous logging. Class is thread-safe.
    class Logger {
    private:
        struct LoggerStrategy;
        using StrategyList = std::vector<std::shared_ptr<LoggerStrategy>>;

    public:

        /// \brief Retrieves singleton instance of Logger.
        /// \return Reference to singleton Logger instance.
        static Logger& get_instance() {
            static Logger* instance = new Logger();
            return *instance;
        }

        /// \brief Adds a logger and its corresponding formatter.
        /// \param logger Unique pointer to a logger instance.
        /// \param formatter Unique pointer to a formatter instance.
        /// \param single_mode If true, this logger will only be invoked by specific log macros
        /// (e.g., LOGIT_TRACE_TO) that explicitly target it using the logger's index.
        /// It will not process logs from general log macros (e.g., LOGIT_TRACE).
        void add_logger(
                std::unique_ptr<ILogger> logger,
                std::unique_ptr<ILogFormatter> formatter,
                bool single_mode = false) {
            if (m_shutdown.load(std::memory_order_acquire)) return;
            auto strategy = std::make_shared<LoggerStrategy>();
            strategy->logger = std::move(logger);
            strategy->formatter = std::move(formatter);
            strategy->logger_identity = dynamic_cast<const void*>(strategy->logger.get());
            strategy->formatter_identity = dynamic_cast<const void*>(strategy->formatter.get());
            strategy->single_mode = single_mode;
            strategy->enabled = true;
            strategy->concurrent_dispatch =
                    strategy->logger &&
                    strategy->logger->supports_concurrent_log() &&
                    (!strategy->formatter || strategy->formatter->supports_concurrent_format());

            StrategyList candidate;
            std::shared_ptr<const StrategyList> snapshot;
            std::shared_ptr<const StrategyList> previous_snapshot;
            {
                LoggerWriteLock lock(m_loggers_mx);
                if (m_shutdown.load(std::memory_order_acquire)) return;
                if (!registration_capacity_available(m_loggers.size(), 1)) {
                    throw std::length_error("Logger index capacity exhausted");
                }
                candidate = m_loggers;
                candidate.push_back(strategy);
                snapshot = make_registration_snapshot(candidate);
                previous_snapshot = std::atomic_load_explicit(
                        &m_loggers_snapshot, std::memory_order_acquire);
                m_loggers.swap(candidate);
                std::atomic_store_explicit(
                        &m_loggers_snapshot, snapshot, std::memory_order_release);
            }
        }

        /// \brief Appends an entire prepared configuration with one snapshot publication.
        /// \param prepared Caller-owned complete backend/formatter pairs; not mutated.
        /// \param options Routing overrides keyed only by incoming configuration IDs.
        /// \param output Immutable ID-to-index result, replaced only after full success.
        /// \param diagnostics Replaced with path-aware registration diagnostics.
        /// \return True only when every entry is registered, or a valid empty batch succeeds.
        ///
        /// Existing entries and indices are preserved. Object aliases between strategies
        /// are rejected even when they advertise concurrent dispatch. Retained owners
        /// preserve lifetime, not authority for unsynchronized external use or shutdown.
        /// Allocation exceptions propagate before commit with registry and output unchanged.
        bool register_configuration(
                const ConfigurationPreparedSet& prepared,
                const ConfigurationRegistrationOptions& options,
                std::shared_ptr<const ConfigurationRegistrationResult>& output,
                ConfigurationDiagnostics& diagnostics) {
            diagnostics = ConfigurationDiagnostics();
            if (prepared.schema_version != 1) {
                diagnostics.add(ConfigurationErrorCode::UnsupportedSchemaVersion,
                                "schema_version", "Only schema version 1 can be registered");
                return false;
            }

            std::map<std::string, std::size_t> incoming_ids;
            for (std::size_t index = 0; index < prepared.loggers.size(); ++index) {
                const ConfigurationPreparedLogger& entry = prepared.loggers[index];
                const std::string path = "loggers[" + std::to_string(index) + "]";
                if (entry.id.empty()) {
                    diagnostics.add(ConfigurationErrorCode::EmptyLoggerId,
                                    path + ".id", "Logger ID must not be empty");
                } else if (!incoming_ids.insert(std::make_pair(entry.id, index)).second) {
                    diagnostics.add(ConfigurationErrorCode::DuplicateLoggerId,
                                    path + ".id", "Logger ID occurs more than once in this batch");
                }
                if (!entry.logger || !entry.formatter) {
                    diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                    path + (!entry.logger ? ".logger" : ".formatter"),
                                    "Prepared backend and formatter must be non-null");
                }
                if (entry.level < LogLevel::LOG_LVL_TRACE ||
                    entry.level > LogLevel::LOG_LVL_FATAL) {
                    diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                    path + ".level", "Prepared level is not a declared LogLevel");
                }
            }
            for (ConfigurationRegistrationOptions::const_iterator it = options.begin();
                 it != options.end(); ++it) {
                if (incoming_ids.find(it->first) == incoming_ids.end()) {
                    diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                    "options." + it->first,
                                    "Registration options must name an incoming logger ID");
                }
            }
            if (!diagnostics.empty()) return false;

            // Every owning candidate outlives either registry lock scope, including
            // failed candidates and displaced result/snapshot owners with user deleters.
            StrategyList additions;
            StrategyList candidate;
            std::map<std::string, int> candidate_ids;
            std::shared_ptr<const StrategyList> snapshot;
            std::shared_ptr<const StrategyList> previous_snapshot;
            std::shared_ptr<ConfigurationRegistrationResult> result;
            std::shared_ptr<const ConfigurationRegistrationResult> result_handle;
            additions.reserve(prepared.loggers.size());
            for (std::size_t index = 0; index < prepared.loggers.size(); ++index) {
                const ConfigurationPreparedLogger& entry = prepared.loggers[index];
                std::shared_ptr<LoggerStrategy> strategy = std::make_shared<LoggerStrategy>();
                strategy->logger = entry.logger;
                strategy->formatter = entry.formatter;
                strategy->logger_identity = dynamic_cast<const void*>(entry.logger.get());
                strategy->formatter_identity = dynamic_cast<const void*>(entry.formatter.get());
                if (strategy->logger_identity == strategy->formatter_identity &&
                    (entry.logger.owner_before(entry.formatter) ||
                     entry.formatter.owner_before(entry.logger))) {
                    diagnostics.add(ConfigurationErrorCode::UnsupportedCombination,
                                    "loggers[" + std::to_string(index) + "].formatter",
                                    "Interfaces of one complete object must share an ownership control block");
                    return false;
                }
                for (std::size_t previous = 0; previous < additions.size(); ++previous) {
                    if (registration_objects_overlap(*strategy, *additions[previous])) {
                        diagnostics.add(ConfigurationErrorCode::RegistrationFailed,
                                        "loggers[" + std::to_string(index) + "]",
                                        "One runtime object cannot belong to multiple strategies");
                        return false;
                    }
                }
                ConfigurationRegistrationOptions::const_iterator option = options.find(entry.id);
                if (option != options.end()) {
                    strategy->enabled = option->second.enabled;
                    strategy->single_mode = option->second.single_mode;
                }
                additions.push_back(strategy);
            }
            {
                LoggerReadLock lock(m_loggers_mx);
                if (!validate_registration_state(prepared, additions, diagnostics)) return false;
            }
            // Capability queries are virtual user code, so never run them under the
            // registry mutex. Recheck the live state after these queries complete.
            for (const auto& strategy : additions) {
                strategy->concurrent_dispatch = strategy->logger->supports_concurrent_log() &&
                        strategy->formatter->supports_concurrent_format();
            }
            {
                LoggerWriteLock lock(m_loggers_mx);
                if (!validate_registration_state(prepared, additions, diagnostics)) return false;
                fail_registration_allocation_if_requested(RegistrationAllocationStage::Result);
                result = std::make_shared<ConfigurationRegistrationResult>();
                if (!additions.empty()) {
                    candidate = m_loggers;
                    candidate.reserve(m_loggers.size() + additions.size());
                    candidate_ids = m_configured_ids;
                    for (std::size_t index = 0; index < additions.size(); ++index) {
                        const int runtime_index = static_cast<int>(m_loggers.size() + index);
                        const std::string& id = prepared.loggers[index].id;
                        candidate.push_back(additions[index]);
                        candidate_ids.insert(std::make_pair(id, runtime_index));
                        result->indices.insert(std::make_pair(id, runtime_index));
                        fail_registration_allocation_if_requested(RegistrationAllocationStage::ResultMap);
                    }
                    snapshot = make_registration_snapshot(candidate);
                    previous_snapshot = std::atomic_load_explicit(
                            &m_loggers_snapshot, std::memory_order_acquire);
                }
                result_handle = result;
                if (!additions.empty()) {
                    m_loggers.swap(candidate);
                    m_configured_ids.swap(candidate_ids);
                    // Registration linearizes here; no throwing work follows.
                    std::atomic_store_explicit(
                            &m_loggers_snapshot, snapshot, std::memory_order_release);
                }
                output.swap(result_handle);
            }
            return true;
        }

        /// \brief Enables or disables a logger by index.
        /// \param logger_index Index of logger.
        /// \param enabled True to enable, false to disable.
        void set_logger_enabled(int logger_index, bool enabled) {
            if (m_shutdown) return;
            LoggerWriteLock lock(m_loggers_mx);
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                m_loggers[logger_index]->enabled.store(enabled, std::memory_order_relaxed);
            }
        }

        /// \brief Checks if a logger is enabled.
        /// \param logger_index Index of logger.
        /// \return True if logger is enabled, false otherwise.
        bool is_logger_enabled(int logger_index) const {
            LoggerReadLock lock(m_loggers_mx);
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                return m_loggers[logger_index]->enabled.load(std::memory_order_relaxed);
            }
            return false;
        }

        /// \brief Sets single-mode flag for a logger.
        /// \param logger_index Index of logger to modify.
        /// \param single_mode True to enable single mode, false to disable.
        void set_logger_single_mode(int logger_index, bool single_mode) {
            if (m_shutdown) return;
            LoggerWriteLock lock(m_loggers_mx);
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                m_loggers[logger_index]->single_mode.store(single_mode, std::memory_order_relaxed);
            }
        }

        /// \brief Sets timestamp offset for a specific logger.
        /// \param logger_index Index of logger to modify.
        /// \param offset_ms Offset in milliseconds.
        void set_timestamp_offset(int logger_index, int64_t offset_ms) {
            if (m_shutdown) return;
            LoggerWriteLock lock(m_loggers_mx);
            if (m_shutdown.load(std::memory_order_acquire)) return;
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                std::lock_guard<std::mutex> exec_lock(m_loggers[logger_index]->exec_mx);
                m_loggers[logger_index]->formatter->set_timestamp_offset(offset_ms);
            }
        }

        /// \brief Sets minimal log level for all loggers.
        /// \param level Minimum log level.
        void set_log_level(LogLevel level) {
            if (m_shutdown) return;
            LoggerReadLock lock(m_loggers_mx);
            if (m_shutdown.load(std::memory_order_acquire)) return;
            for (auto& strategy : m_loggers) {
                if (!strategy) continue;
                std::lock_guard<std::mutex> exec_lock(strategy->exec_mx);
                strategy->logger->set_log_level(level);
            }
        }

        /// \brief Sets minimal log level for a specific logger.
        /// \param logger_index Index of logger.
        /// \param level Minimum log level.
        void set_log_level(int logger_index, LogLevel level) {
            if (m_shutdown) return;
            LoggerWriteLock lock(m_loggers_mx);
            if (m_shutdown.load(std::memory_order_acquire)) return;
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                std::lock_guard<std::mutex> exec_lock(m_loggers[logger_index]->exec_mx);
                m_loggers[logger_index]->logger->set_log_level(level);
            }
        }

        /// \brief Checks whether a logger is in single mode.
        /// \param logger_index Index of logger.
        /// \return True if logger is in single mode, false otherwise.
        bool is_logger_single_mode(int logger_index) const {
            if (m_shutdown) return false;
            LoggerReadLock lock(m_loggers_mx);
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                return m_loggers[logger_index]->single_mode.load(std::memory_order_relaxed);
            }
            return false;
        }

        /// \brief Logs a LogRecord using added loggers and formatters.
        ///
        /// Formats the log message using each logger's corresponding formatter and sends
        /// the formatted message to the logger.
        /// \param record Log record to be logged.
        void log(const LogRecord& record) {
            if (m_shutdown.load(std::memory_order_acquire)) return;

            const bool targeted = record.logger_index >= 0;
#ifdef LOGIT_BENCH_LEGACY_REGISTRY
            StrategyList legacy_snapshot;
            {
                LoggerReadLock legacy_lock(m_loggers_mx);
                if (targeted) {
                    if (record.logger_index < static_cast<int>(m_loggers.size())) {
                        legacy_snapshot.push_back(m_loggers[record.logger_index]);
                    }
                } else {
                    legacy_snapshot = m_loggers;
                }
            }
            const StrategyList* strategies = &legacy_snapshot;
            // The targeted legacy snapshot contains only the selected strategy,
            // so dispatch must address its sole element rather than the original
            // registry index.  The production snapshot retains the full registry
            // and continues to use record.logger_index below.
            const int strategy_index = targeted ? 0 : record.logger_index;
#else
            const auto snapshot = std::atomic_load_explicit(
                    &m_loggers_snapshot, std::memory_order_acquire);
            const StrategyList* strategies = snapshot ? snapshot.get() : nullptr;
            const int strategy_index = record.logger_index;
#endif
            if (!strategies) return;

            if (targeted) {
                if (strategy_index < 0 ||
                    strategy_index >= static_cast<int>(strategies->size())) return;
                const auto& strategy = (*strategies)[strategy_index];
                if (!strategy) return;

                dispatch_to_strategy_if_allowed(*strategy, record, true);
                return;
            }

            for (const auto& strategy : *strategies) {
                if (!strategy) continue;

                dispatch_to_strategy_if_allowed(*strategy, record, false);
            }
        }

        /// \brief Retrieves a string parameter from a logger.
        /// \param logger_index Index of logger.
        /// \param param Logger parameter to retrieve.
        /// \return Requested parameter as a string, or empty string if unsupported.
        std::string get_string_param(int logger_index, const LoggerParam& param) const {
            if (m_shutdown) return std::string();
            LoggerReadLock lock(m_loggers_mx);
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                const auto& strategy = m_loggers[logger_index];
                return strategy->logger->get_string_param(param);
            }
            return std::string();
        }

        /// \brief Retrieves an integer parameter from a logger.
        /// \param logger_index Index of logger.
        /// \param param Logger parameter to retrieve.
        /// \return Requested parameter as an integer, or 0 if unsupported.
        int64_t get_int_param(int logger_index, const LoggerParam& param) const {
            if (m_shutdown) return 0;
            LoggerReadLock lock(m_loggers_mx);
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                const auto& strategy = m_loggers[logger_index];
                return strategy->logger->get_int_param(param);
            }
            return 0;
        }

        /// \brief Retrieves a floating-point parameter from a logger.
        /// \param logger_index Index of logger.
        /// \param param Logger parameter to retrieve.
        /// \return Requested parameter as a double, or 0.0 if unsupported.
        double get_float_param(int logger_index, const LoggerParam& param) const {
            if (m_shutdown) return 0.0;
            LoggerReadLock lock(m_loggers_mx);
            if (logger_index >= 0 && logger_index < static_cast<int>(m_loggers.size()) && m_loggers[logger_index]) {
                const auto& strategy = m_loggers[logger_index];
                return strategy->logger->get_float_param(param);
            }
            return 0.0;
        }

        /// \brief Retrieves buffered formatted messages from a logger.
        /// \param logger_index Index of logger.
        /// \return Buffered messages in chronological order, or an empty vector if unsupported.
        std::vector<std::string> get_buffered_strings(int logger_index) const {
            if (m_shutdown) return std::vector<std::string>();

            auto strategy = get_strategy_snapshot(logger_index);
            if (!strategy) {
                return std::vector<std::string>();
            }

            return strategy->logger->get_buffered_strings();
        }

        /// \brief Retrieves buffered structured entries from a logger.
        /// \param logger_index Index of logger.
        /// \return Buffered entries in chronological order, or an empty vector if unsupported.
        std::vector<BufferedLogEntry> get_buffered_entries(int logger_index) const {
            if (m_shutdown) return std::vector<BufferedLogEntry>();

            auto strategy = get_strategy_snapshot(logger_index);
            if (!strategy) {
                return std::vector<BufferedLogEntry>();
            }

            return strategy->logger->get_buffered_entries();
        }

        /// \brief Lists persisted log files exposed by a logger backend.
        /// \param logger_index Index of logger.
        /// \return Persisted file metadata, or an empty vector if unsupported.
        std::vector<LogFileInfo> list_log_files(int logger_index) const {
            if (m_shutdown) return std::vector<LogFileInfo>();

            auto strategy = get_strategy_snapshot(logger_index);
            if (!strategy) {
                return std::vector<LogFileInfo>();
            }

            return strategy->logger->list_log_files();
        }

        /// \brief Reads one persisted log file exposed by a logger backend.
        /// \param logger_index Index of logger.
        /// \param path Full path returned by `list_log_files()`.
        /// \return Read result with `ok=false` when unavailable or unsupported.
        LogFileReadResult read_log_file(int logger_index, const std::string& path) const {
            if (m_shutdown) {
                LogFileReadResult result;
                result.file.path = path;
                result.ok = false;
                return result;
            }

            auto strategy = get_strategy_snapshot(logger_index);
            if (!strategy) {
                LogFileReadResult result;
                result.file.path = path;
                result.ok = false;
                return result;
            }

            return strategy->logger->read_log_file(path);
        }

        /// \brief Reads several persisted log files exposed by a logger backend.
        /// \param logger_index Index of logger.
        /// \param paths Full paths returned by `list_log_files()`.
        /// \return Per-file read results in request order.
        std::vector<LogFileReadResult> read_log_files(int logger_index, const std::vector<std::string>& paths) const {
            if (m_shutdown) {
                std::vector<LogFileReadResult> results;
                results.reserve(paths.size());
                for (size_t i = 0; i < paths.size(); ++i) {
                    LogFileReadResult result;
                    result.file.path = paths[i];
                    result.ok = false;
                    results.push_back(result);
                }
                return results;
            }

            auto strategy = get_strategy_snapshot(logger_index);
            if (!strategy) {
                std::vector<LogFileReadResult> results;
                results.reserve(paths.size());
                for (size_t i = 0; i < paths.size(); ++i) {
                    LogFileReadResult result;
                    result.file.path = paths[i];
                    result.ok = false;
                    results.push_back(result);
                }
                return results;
            }

            return strategy->logger->read_log_files(paths);
        }

        /// \brief Retrieves the current minimal log level for a logger.
        /// \param logger_index Index of logger.
        /// \return Current minimal log level, or TRACE when the logger index is invalid.
        LogLevel get_log_level(int logger_index) const {
            if (m_shutdown) return LogLevel::LOG_LVL_TRACE;

            auto strategy = get_strategy_snapshot(logger_index);
            if (!strategy) {
                return LogLevel::LOG_LVL_TRACE;
            }

            return strategy->logger->get_log_level();
        }

        /// \brief Logs message and returns tuple of arguments.
        /// \tparam Ts Types of arguments.
        /// \param record Log record.
        /// \param args Arguments to be logged.
        /// \return Tuple containing logged arguments.
        template <typename... Ts>
        auto log_and_return(const LogRecord& record, Ts&&... args) -> decltype(std::forward_as_tuple(std::forward<Ts>(args)...)) {
            this->print(record, args...);
            return std::forward_as_tuple(std::forward<Ts>(args)...);
        }

        /// \brief Logs message and returns argument.
        ///
        /// Logs provided argument and returns it.
        /// \tparam T Type of argument.
        /// \param record Log record.
        /// \param args Argument to be logged.
        /// \return Logged argument.
        template <typename T>
        auto log_and_return(const LogRecord& record, T&& args) -> decltype(args) {
            this->print(record, args);
            return std::forward<decltype(args)>(args);
        }

        /// \brief Logs message without arguments and returns empty tuple.
        /// \param record Log record.
        /// \return Empty tuple.
        auto log_and_return(const LogRecord& record) -> std::tuple<> {
            this->print(record);
            return {};
        }

        /// \brief Waits for all asynchronous loggers to finish processing.
        ///
        /// Ensures that all log messages are fully processed before continuing.
        void wait() {
            const auto snapshot = get_all_strategy_snapshots();
            for (const auto& strategy : snapshot) {
                if (!strategy) continue;
                strategy->logger->wait();
            }
        }

        /// \brief Clears logger-owned records for a specific logger.
        /// \param logger_index Index of logger.
        /// \param options Data categories to clear.
        /// \return Cleanup result for the selected logger.
        LogClearResult clear_logger(int logger_index, const LogClearOptions& options = LogClearOptions()) {
            LogClearResult result;
            if (m_shutdown.load(std::memory_order_acquire)) {
                result.ok = false;
                result.status = LogClearStatus::Failed;
                result.message = "logger system is shut down";
                return result;
            }

            auto strategy = get_strategy_snapshot(logger_index);
            if (!strategy) {
                result.ok = false;
                result.status = LogClearStatus::Failed;
                result.message = "logger index not found";
                return result;
            }

            std::lock_guard<std::mutex> exec_lock(strategy->exec_mx);
            if (m_shutdown.load(std::memory_order_acquire)) {
                result.ok = false;
                result.status = LogClearStatus::Failed;
                result.message = "logger system is shut down";
                return result;
            }
            return strategy->logger->clear_logs(options);
        }

        /// \brief Clears logger-owned records for all registered loggers.
        /// \param options Data categories to clear.
        /// \return Aggregated cleanup result. Unsupported loggers are skipped.
        LogClearResult clear_all_loggers(const LogClearOptions& options = LogClearOptions()) {
            LogClearResult result;
            if (m_shutdown.load(std::memory_order_acquire)) {
                result.ok = false;
                result.status = LogClearStatus::Failed;
                result.message = "logger system is shut down";
                return result;
            }

            const auto snapshot = get_all_strategy_snapshots();
            std::size_t supported = 0;
            std::size_t failed = 0;
            std::size_t unsupported = 0;
            for (const auto& strategy : snapshot) {
                if (!strategy) continue;
                std::lock_guard<std::mutex> exec_lock(strategy->exec_mx);
                if (m_shutdown.load(std::memory_order_acquire)) {
                    result.ok = false;
                    result.status = LogClearStatus::Failed;
                    result.message = "logger system is shut down";
                    return result;
                }
                const LogClearResult one = strategy->logger->clear_logs(options);
                if (one.status == LogClearStatus::Cleared && one.ok) {
                    ++supported;
                    result.cleared_records += one.cleared_records;
                } else if (one.status == LogClearStatus::Unsupported) {
                    ++unsupported;
                } else {
                    ++failed;
                    if (result.message.empty()) {
                        result.message = one.message;
                    }
                }
            }

            result.ok = supported > 0 && failed == 0;
            if (result.ok) {
                result.status = LogClearStatus::Cleared;
                result.message = unsupported > 0 ? "cleared supported loggers; skipped unsupported loggers" : "cleared";
            } else {
                result.status = unsupported > 0 && failed == 0 ? LogClearStatus::Unsupported : LogClearStatus::Failed;
                if (result.message.empty()) {
                    result.message = unsupported > 0 ? "no logger supports clearing" : "no loggers to clear";
                }
            }
            return result;
        }

        /// \brief Returns the number of registered logger strategies.
        std::size_t logger_count() const {
            LoggerReadLock lock(m_loggers_mx);
            return m_loggers.size();
        }

        /// \brief Retrieves a typed backend pointer from a logger by index.
        template <typename LoggerT>
        LoggerT* get_logger_as(int logger_index) {
            auto strategy = get_strategy_snapshot(logger_index);
            return (strategy && strategy->logger)
                ? dynamic_cast<LoggerT*>(strategy->logger.get())
                : nullptr;
        }

        /// \brief Retrieves a typed backend pointer from a logger by index.
        template <typename LoggerT>
        const LoggerT* get_logger_as(int logger_index) const {
            auto strategy = get_strategy_snapshot(logger_index);
            return (strategy && strategy->logger)
                ? dynamic_cast<const LoggerT*>(strategy->logger.get())
                : nullptr;
        }

        /// \brief Shuts down logger system.
        ///
        /// Disables further logging, waits for asynchronous tasks to complete,
        /// and shuts down TaskExecutor.
        void shutdown() {
            std::shared_ptr<const StrategyList> snapshot;
            {
                LoggerWriteLock lock(m_loggers_mx);
                if (m_shutdown.load(std::memory_order_acquire)) return;
                snapshot = std::atomic_load_explicit(
                        &m_loggers_snapshot, std::memory_order_acquire);
                // Shutdown and registry writers share this linearization boundary.
                m_shutdown.store(true, std::memory_order_release);
            }
            for (const auto& strategy : *snapshot) {
                if (!strategy || !strategy->logger) continue;
                std::lock_guard<std::mutex> exec_lock(strategy->exec_mx);
                strategy->logger->shutdown();
            }
            detail::TaskExecutor::get_instance().shutdown();
        }

    private:

        friend struct detail::LoggerRegistrationTestAccess;

        enum class RegistrationAllocationStage { None, Snapshot, Result, ResultMap };

        /// \struct LoggerStrategy
        /// \brief Structure to hold a logger-formatter pair.
        struct LoggerStrategy {
            std::shared_ptr<ILogger> logger;            ///< The logger instance.
            std::shared_ptr<ILogFormatter> formatter;   ///< The formatter instance.
            const void* logger_identity = nullptr;     ///< Most-derived backend object address.
            const void* formatter_identity = nullptr;  ///< Most-derived formatter object address.
            std::atomic<bool> single_mode{false};       ///< Flag indicating if the logger is in single mode.
            std::atomic<bool> enabled{true};            ///< Flag indicating if the logger is enabled.
            bool concurrent_dispatch = false;           ///< Explicit formatter/backend lock-elision capability.
            mutable std::mutex exec_mx;                 ///< Protects formatter+logger invocation.
        };

        void dispatch_to_strategy_if_allowed(
                LoggerStrategy& strategy,
                const LogRecord& record,
                bool targeted) {
            if (strategy.concurrent_dispatch) {
                dispatch_to_strategy_if_allowed_unlocked(strategy, record, targeted);
                return;
            }

            std::lock_guard<std::mutex> exec_lock(strategy.exec_mx);
            dispatch_to_strategy_if_allowed_unlocked(strategy, record, targeted);
        }

        void dispatch_to_strategy_if_allowed_unlocked(
                LoggerStrategy& strategy,
                const LogRecord& record,
                bool targeted) {
            if (m_shutdown.load(std::memory_order_acquire)) return;
            if (!targeted && strategy.single_mode.load(std::memory_order_relaxed)) return;
            if (!strategy.enabled.load(std::memory_order_relaxed)) return;
            if (!record.raw_mode &&
                static_cast<int>(record.log_level) < static_cast<int>(strategy.logger->get_log_level())) return;
            dispatch_to_strategy(strategy, record);
        }

        void dispatch_to_strategy(LoggerStrategy& strategy, const LogRecord& record) {
            if (record.raw_mode) {
                strategy.logger->log(record, record.format);
                return;
            }
            if (strategy.formatter && strategy.formatter->is_passthrough()) {
                strategy.logger->log(record, record.format);
                return;
            }
            const std::string msg = strategy.formatter ? strategy.formatter->format(record) : std::string();
            strategy.logger->log(record, msg);
        }

        std::shared_ptr<LoggerStrategy> get_strategy_snapshot(int logger_index) const {
            const auto snapshot = std::atomic_load_explicit(
                    &m_loggers_snapshot, std::memory_order_acquire);
            if (logger_index >= 0 &&
                snapshot && logger_index < static_cast<int>(snapshot->size()) &&
                (*snapshot)[logger_index]) {
                return (*snapshot)[logger_index];
            }
            return std::shared_ptr<LoggerStrategy>();
        }

        static bool registration_capacity_available(
                std::size_t current_count, std::size_t batch_count) noexcept {
            const std::size_t limit = static_cast<std::size_t>(INT_MAX);
            return current_count <= limit && batch_count <= limit - current_count;
        }

        static bool registration_objects_overlap(
                const LoggerStrategy& left, const LoggerStrategy& right) noexcept {
            return (left.logger_identity &&
                    (left.logger_identity == right.logger_identity ||
                     left.logger_identity == right.formatter_identity)) ||
                   (left.formatter_identity &&
                    (left.formatter_identity == right.logger_identity ||
                     left.formatter_identity == right.formatter_identity));
        }

        // Called only while a registry lock is held, before capability queries and
        // again at commit. No virtual methods or user-owned cleanup run here.
        bool validate_registration_state(
                const ConfigurationPreparedSet& prepared,
                const StrategyList& additions,
                ConfigurationDiagnostics& diagnostics) const {
            if (m_shutdown.load(std::memory_order_acquire)) {
                diagnostics.add(ConfigurationErrorCode::RegistrationFailed,
                                "loggers", "Logger system is shut down");
                return false;
            }
            if (!registration_capacity_available(m_loggers.size(), additions.size())) {
                diagnostics.add(ConfigurationErrorCode::RegistrationFailed,
                                "loggers", "Logger index capacity exhausted");
                return false;
            }
            for (std::size_t index = 0; index < additions.size(); ++index) {
                if (m_configured_ids.find(prepared.loggers[index].id) != m_configured_ids.end()) {
                    diagnostics.add(ConfigurationErrorCode::DuplicateLoggerId,
                                    "loggers[" + std::to_string(index) + "].id",
                                    "Logger ID is already registered");
                    return false;
                }
                for (const auto& existing : m_loggers) {
                    if (existing && registration_objects_overlap(*additions[index], *existing)) {
                        diagnostics.add(ConfigurationErrorCode::RegistrationFailed,
                                        "loggers[" + std::to_string(index) + "]",
                                        "Runtime object is already registered in another strategy");
                        return false;
                    }
                }
            }
            return true;
        }

        void fail_registration_allocation_if_requested(RegistrationAllocationStage stage) {
            if (m_registration_allocation_failure == stage) {
                m_registration_allocation_failure = RegistrationAllocationStage::None;
                throw std::bad_alloc();
            }
        }

        std::shared_ptr<const StrategyList> make_registration_snapshot(const StrategyList& candidate) {
            fail_registration_allocation_if_requested(RegistrationAllocationStage::Snapshot);
            return std::shared_ptr<const StrategyList>(new StrategyList(candidate));
        }

        std::vector<std::shared_ptr<LoggerStrategy>> get_all_strategy_snapshots() const {
            LoggerReadLock lock(m_loggers_mx);
            return m_loggers;
        }

        std::vector<std::shared_ptr<LoggerStrategy>> m_loggers;        ///< Container for logger-formatter pairs.
        std::map<std::string, int> m_configured_ids; ///< Stable indices of configured entries only.
        std::shared_ptr<const StrategyList> m_loggers_snapshot; ///< Immutable read-mostly strategy list.
        mutable LoggerMutex m_loggers_mx;                        ///< Protects access to logger strategies.
        std::atomic<bool> m_shutdown = ATOMIC_VAR_INIT(false); ///< Flag indicating if shutdown was requested.
        RegistrationAllocationStage m_registration_allocation_failure =
                RegistrationAllocationStage::None; ///< Private cold-path allocation-failure seam.

        void print(const LogRecord& record) {
            log(record);
        }
        
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4127) // conditional expression is constant
#endif

        /// \brief Logs a record with given arguments.
        /// \tparam Ts Types of arguments.
        /// \param record Log record.
        /// \param args Arguments to be logged.
        template <typename... Ts>
        void print(const LogRecord& record, Ts const&... args) {
            if (sizeof...(Ts) == 0) {
                log(record);
                return;
            }
            // args_array is mutable cache inside LogRecord
            auto var_names = split_arguments(record.arg_names);
            record.args_array = args_to_array(var_names.begin(), args...);
            log(record);
        }
        
#ifdef _MSC_VER
#	pragma warning(pop)
#endif

        Logger() {
            std::atomic_store_explicit(
                    &m_loggers_snapshot,
                    std::shared_ptr<const StrategyList>(new StrategyList()),
                    std::memory_order_release);
            std::atexit(Logger::on_exit_handler);
        }

        ~Logger() {
            shutdown();
        }

        // Deleting copy and move constructors and assignment operators to enforce singleton.
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        /// \brief Atexit shutdown handler for Logger and TaskExecutor.
        /// \details Called automatically at program exit.
        static void on_exit_handler() {
            Logger::get_instance().shutdown();
        }
    };

}; // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGER_HPP_INCLUDED
