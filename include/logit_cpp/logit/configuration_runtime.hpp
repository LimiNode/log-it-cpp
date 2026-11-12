#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_HPP_INCLUDED

/// \file configuration_runtime.hpp
/// \brief Binding and runtime-instantiation contract for materialized plans.

#include "configuration.hpp"
#include "formatter.hpp"
#include "loggers.hpp"

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

namespace logit {

    /// \class ConfigurationBindingSet
    /// \brief Type-erased programmatic resources resolved for one logger.
    ///
    /// Bindings deliberately remain outside the parser-neutral document. A
    /// resolver and its factory agree on the concrete type associated with each
    /// name; the configuration layer only owns the lifetime of the shared value.
    class ConfigurationBindingSet {
    public:
        bool empty() const { return m_values.empty(); }

        bool contains(const std::string& name) const {
            std::map<std::string, Entry>::const_iterator it = m_values.find(name);
            return it != m_values.end() && static_cast<bool>(it->second.value);
        }

        /// \brief Returns whether a binding name was supplied, regardless of its type.
        ///
        /// This is useful for optional bindings: a factory can distinguish an
        /// absent callback from a present callback with an incompatible type.
        bool contains_name(const std::string& name) const {
            return m_values.find(name) != m_values.end();
        }

        template <typename T>
        void set(const std::string& name, const std::shared_ptr<T>& value) {
            m_values[name] = Entry(value, std::type_index(typeid(T)));
        }

        template <typename T>
        std::shared_ptr<T> get(const std::string& name) const {
            std::map<std::string, Entry>::const_iterator it = m_values.find(name);
            if (it == m_values.end() || !it->second.value ||
                it->second.type != std::type_index(typeid(T))) {
                return std::shared_ptr<T>();
            }
            return std::static_pointer_cast<T>(it->second.value);
        }

    private:
        struct Entry {
            Entry() : value(), type(typeid(void)) {}
            Entry(const std::shared_ptr<void>& value, const std::type_index& type)
                : value(value), type(type) {}

            std::shared_ptr<void> value;
            std::type_index type;
        };

        std::map<std::string, Entry> m_values;
    };

    /// \class ConfigurationBindingResolver
    /// \brief Resolves required programmatic bindings without constructing a logger.
    class ConfigurationBindingResolver {
    public:
        virtual ~ConfigurationBindingResolver() {}

        /// \brief Resolve bindings for one materialized logger.
        ///
        /// Diagnostic paths are relative to the logger root. The document layer
        /// prefixes them with `loggers[N]`.
        virtual bool resolve(const ConfigurationMaterializedLogger& logger,
                             ConfigurationBindingSet& bindings,
                             ConfigurationDiagnostics& diagnostics) const = 0;
    };

    /// \class ConfigurationLoggerFactory
    /// \brief Creates one runtime logger after bindings have been resolved.
    /// Factory side effects are owned by the caller and are not transactional;
    /// only publication of the caller-visible logger vector is atomic.
    /// A factory owns the concrete binding types it expects and must retrieve
    /// required values through `ConfigurationBindingSet::get<T>()`. If a
    /// required value has the wrong type, the factory must fail closed by
    /// adding a diagnostic or returning an empty logger pointer.
    class ConfigurationLoggerFactory {
    public:
        virtual ~ConfigurationLoggerFactory() {}

        /// \brief Instantiate one logger from a complete plan entry.
        ///
        /// Returning an empty pointer is a failed instantiation. Diagnostic
        /// paths are relative to the logger root and are rebased by the document
        /// layer.
        virtual std::shared_ptr<ILogger> instantiate(
            const ConfigurationMaterializedLogger& logger,
            const ConfigurationBindingSet& bindings,
            ConfigurationDiagnostics& diagnostics) const = 0;
    };

    /// \struct ConfigurationPreparedLogger
    /// \brief Complete runtime pair prepared from one materialized entry.
    ///
    /// Shared ownership matches the existing ConfigurationLoggerFactory API.
    /// This result is intentionally not accepted by `Logger::add_logger()`,
    /// which currently takes unique ownership; defining an atomic batch
    /// registration and ownership transfer API is a separate boundary.
    struct ConfigurationPreparedLogger {
        std::string id;
        std::shared_ptr<ILogger> logger;
        std::shared_ptr<ILogFormatter> formatter;
        LogLevel level = LogLevel::LOG_LVL_TRACE;
    };

    /// \struct ConfigurationPreparedSet
    /// \brief Atomically published runtime preparation result.
    struct ConfigurationPreparedSet {
        std::uint32_t schema_version = 0;
        std::vector<ConfigurationPreparedLogger> loggers;
    };

    namespace configuration_runtime_detail {

        inline std::shared_ptr<ILogFormatter> make_configuration_formatter(
                const ConfigurationMaterializedLogger& logger,
                ConfigurationDiagnostics& diagnostics) {
            try {
                SimpleLogFormatter::Config config;
                if (logger.strategy.has_formatter) {
                    const ConfigurationFormatterSpec& formatter = logger.strategy.formatter;
                    if (formatter.type != "simple") {
                        diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                        "strategy.formatter.type",
                                        "Only the simple formatter is supported in schema v1");
                        return std::shared_ptr<ILogFormatter>();
                    }
                    if (formatter.has_pattern) config.pattern = formatter.pattern;
                    if (formatter.has_json_format) config.json_format = formatter.json_format;
                }
                return std::shared_ptr<ILogFormatter>(
                        new SimpleLogFormatter(config.pattern, config.json_format));
            } catch (const std::exception& error) {
                diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                "strategy.formatter.pattern",
                                std::string("Formatter construction failed: ") + error.what());
            } catch (...) {
                diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                "strategy.formatter.pattern",
                                "Formatter construction failed");
            }
            return std::shared_ptr<ILogFormatter>();
        }

        inline bool resolve_bindings(
            const ConfigurationPlan& plan,
            const ConfigurationBindingResolver& resolver,
            std::vector<ConfigurationBindingSet>& resolved_bindings,
            ConfigurationDiagnostics& diagnostics) {
            resolved_bindings.reserve(plan.loggers.size());
            bool bindings_ok = true;
            for (std::size_t index = 0; index < plan.loggers.size(); ++index) {
                const ConfigurationMaterializedLogger& entry = plan.loggers[index];
                ConfigurationBindingSet bindings;
                ConfigurationDiagnostics local_diagnostics;
                const bool resolved = resolver.resolve(entry, bindings, local_diagnostics);
                bool missing_binding = false;
                for (std::vector<std::string>::const_iterator required =
                         entry.required_bindings.begin();
                     required != entry.required_bindings.end(); ++required) {
                    if (!bindings.contains(*required)) {
                        local_diagnostics.add(ConfigurationErrorCode::MissingProgrammaticBinding,
                                              "binding." + *required,
                                              "Required programmatic binding was not resolved");
                        missing_binding = true;
                    }
                }
                const std::string prefix = "loggers[" + std::to_string(index) + "]";
                if (!resolved || !local_diagnostics.empty() || missing_binding) {
                    bindings_ok = false;
                    if (local_diagnostics.empty()) {
                        diagnostics.add(ConfigurationErrorCode::BindingResolutionFailed,
                                        prefix + ".binding",
                                        "Programmatic bindings could not be resolved");
                    } else {
                        diagnostics.append_rebased(local_diagnostics, prefix);
                    }
                }
                resolved_bindings.push_back(bindings);
            }
            return bindings_ok;
        }

        inline std::shared_ptr<ILogger> instantiate_logger(
            const ConfigurationMaterializedLogger& entry,
            const ConfigurationBindingSet& bindings,
            const ConfigurationLoggerFactory& factory,
            const std::string& prefix,
            ConfigurationDiagnostics& diagnostics) {
            ConfigurationDiagnostics local_diagnostics;
            std::shared_ptr<ILogger> runtime_logger =
                factory.instantiate(entry, bindings, local_diagnostics);
            if (!runtime_logger || !local_diagnostics.empty()) {
                if (local_diagnostics.empty()) {
                    diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                    prefix,
                                    "Logger factory could not instantiate the plan entry");
                } else {
                    diagnostics.append_rebased(local_diagnostics, prefix);
                }
                return std::shared_ptr<ILogger>();
            }
            return runtime_logger;
        }

    } // namespace configuration_runtime_detail

    /// \brief Resolve bindings and prepare complete logger/formatter pairs.
    ///
    /// The caller-visible prepared set is updated only after every binding,
    /// backend factory call, formatter construction, and level assignment has
    /// succeeded. Factory side effects remain non-transactional.
    inline bool prepare_configuration(
        const ConfigurationPlan& plan,
        const ConfigurationBindingResolver& resolver,
        const ConfigurationLoggerFactory& factory,
        ConfigurationPreparedSet& output,
        ConfigurationDiagnostics& diagnostics) {
        diagnostics = ConfigurationDiagnostics();
        std::vector<ConfigurationBindingSet> resolved_bindings;
        if (!configuration_runtime_detail::resolve_bindings(
                plan, resolver, resolved_bindings, diagnostics)) return false;

        ConfigurationPreparedSet candidate;
        candidate.schema_version = plan.schema_version;
        candidate.loggers.reserve(plan.loggers.size());
        for (std::size_t index = 0; index < plan.loggers.size(); ++index) {
            const ConfigurationMaterializedLogger& entry = plan.loggers[index];
            const std::string prefix = "loggers[" + std::to_string(index) + "]";
            std::shared_ptr<ILogger> runtime_logger =
                configuration_runtime_detail::instantiate_logger(
                    entry, resolved_bindings[index], factory, prefix, diagnostics);
            if (!runtime_logger) return false;

            ConfigurationDiagnostics local_diagnostics;
            std::shared_ptr<ILogFormatter> formatter =
                configuration_runtime_detail::make_configuration_formatter(entry, local_diagnostics);
            if (!formatter) {
                if (local_diagnostics.empty()) {
                    diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                    prefix + ".strategy.formatter.pattern",
                                    "Formatter could not be prepared");
                } else {
                    diagnostics.append_rebased(local_diagnostics, prefix);
                }
                return false;
            }

            const LogLevel level = entry.strategy.has_level
                ? entry.strategy.level : LogLevel::LOG_LVL_TRACE;
            try {
                runtime_logger->set_log_level(level);
            } catch (const std::exception& error) {
                diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                prefix + ".strategy.level",
                                std::string("Logger level assignment failed: ") + error.what());
                return false;
            } catch (...) {
                diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                prefix + ".strategy.level",
                                "Logger level assignment failed");
                return false;
            }

            ConfigurationPreparedLogger prepared;
            prepared.id = entry.id;
            prepared.logger = runtime_logger;
            prepared.formatter = formatter;
            prepared.level = level;
            candidate.loggers.push_back(prepared);
        }
        output = candidate;
        return true;
    }

    /// \brief Resolve bindings and publish a complete logger set atomically.
    ///
    /// This is the legacy backend-instantiation API. It deliberately preserves
    /// the pre-runtime-preparation contract: the factory owns logger strategy
    /// state, so this function does not construct formatters or assign levels.
    /// Use `prepare_configuration()` when strategy-aware formatter and level
    /// preparation is required. The caller-visible logger vector is updated
    /// only after every binding has resolved and every factory call has
    /// returned a logger. Factory side effects remain non-transactional.
    inline bool instantiate_configuration(
        const ConfigurationPlan& plan,
        const ConfigurationBindingResolver& resolver,
        const ConfigurationLoggerFactory& factory,
        std::vector<std::shared_ptr<ILogger>>& loggers,
        ConfigurationDiagnostics& diagnostics) {
        diagnostics = ConfigurationDiagnostics();
        std::vector<ConfigurationBindingSet> resolved_bindings;
        if (!configuration_runtime_detail::resolve_bindings(
                plan, resolver, resolved_bindings, diagnostics)) return false;

        std::vector<std::shared_ptr<ILogger>> candidate;
        candidate.reserve(plan.loggers.size());
        for (std::size_t index = 0; index < plan.loggers.size(); ++index) {
            const ConfigurationMaterializedLogger& entry = plan.loggers[index];
            const std::string prefix = "loggers[" + std::to_string(index) + "]";
            std::shared_ptr<ILogger> runtime_logger =
                configuration_runtime_detail::instantiate_logger(
                    entry, resolved_bindings[index], factory, prefix, diagnostics);
            if (!runtime_logger) return false;
            candidate.push_back(runtime_logger);
        }

        loggers = candidate;
        return true;
    }

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_HPP_INCLUDED
