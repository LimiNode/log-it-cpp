#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_HPP_INCLUDED

/// \file configuration.hpp
/// \brief Parser-neutral configuration model and validation contract.

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "enums.hpp"

namespace logit {

    /// \enum ConfigurationErrorCode
    /// \brief Stable categories emitted by configuration validation.
    enum class ConfigurationErrorCode {
        UnsupportedSchemaVersion,
        EmptyLoggerId,
        DuplicateLoggerId,
        EmptyBackendType,
        UnknownBackendType,
        InvalidValue,
        UnsupportedCombination,
        UnavailableBackend,
        MissingProgrammaticBinding,
        UnknownField,
        BackendValidationFailed,
        MaterializationFailed
    };

    inline const char* configuration_error_code_name(ConfigurationErrorCode code) {
        switch (code) {
            case ConfigurationErrorCode::UnsupportedSchemaVersion: return "unsupported_schema_version";
            case ConfigurationErrorCode::EmptyLoggerId: return "empty_logger_id";
            case ConfigurationErrorCode::DuplicateLoggerId: return "duplicate_logger_id";
            case ConfigurationErrorCode::EmptyBackendType: return "empty_backend_type";
            case ConfigurationErrorCode::UnknownBackendType: return "unknown_backend_type";
            case ConfigurationErrorCode::InvalidValue: return "invalid_value";
            case ConfigurationErrorCode::UnsupportedCombination: return "unsupported_combination";
            case ConfigurationErrorCode::UnavailableBackend: return "unavailable_backend";
            case ConfigurationErrorCode::MissingProgrammaticBinding: return "missing_programmatic_binding";
            case ConfigurationErrorCode::UnknownField: return "unknown_field";
            case ConfigurationErrorCode::BackendValidationFailed: return "backend_validation_failed";
            case ConfigurationErrorCode::MaterializationFailed: return "materialization_failed";
        }
        return "unknown_configuration_error";
    }

    /// \struct ConfigurationDiagnostic
    /// \brief One validation or materialization diagnostic with a document path.
    struct ConfigurationDiagnostic {
        ConfigurationErrorCode code = ConfigurationErrorCode::InvalidValue;
        std::string path;
        std::string message;
    };

    /// \class ConfigurationDiagnostics
    /// \brief Ordered diagnostics produced while validating a configuration.
    class ConfigurationDiagnostics {
    public:
        void add(ConfigurationErrorCode code,
                 const std::string& path,
                 const std::string& message) {
            ConfigurationDiagnostic diagnostic;
            diagnostic.code = code;
            diagnostic.path = path;
            diagnostic.message = message;
            m_items.push_back(diagnostic);
        }

        bool empty() const { return m_items.empty(); }
        std::size_t size() const { return m_items.size(); }
        const std::vector<ConfigurationDiagnostic>& items() const { return m_items; }

        bool has_code(ConfigurationErrorCode code) const {
            for (std::vector<ConfigurationDiagnostic>::const_iterator it = m_items.begin();
                 it != m_items.end(); ++it) {
                if (it->code == code) {
                    return true;
                }
            }
            return false;
        }

        void append_rebased(const ConfigurationDiagnostics& source,
                            const std::string& prefix) {
            for (std::vector<ConfigurationDiagnostic>::const_iterator it = source.m_items.begin();
                 it != source.m_items.end(); ++it) {
                const std::string path = it->path.empty()
                    ? prefix
                    : prefix + "." + it->path;
                add(it->code, path, it->message);
            }
        }

    private:
        std::vector<ConfigurationDiagnostic> m_items;
    };

    /// \class ConfigurationValue
    /// \brief Parser-neutral scalar or nested value used by backend field maps.
    class ConfigurationValue {
    public:
        enum class Type {
            Null,
            Boolean,
            SignedInteger,
            UnsignedInteger,
            String,
            Array,
            Object
        };

        ConfigurationValue() : m_type(Type::Null), m_bool(false), m_signed(0), m_unsigned(0) {}

        static ConfigurationValue make_boolean(bool value) {
            ConfigurationValue result;
            result.m_type = Type::Boolean;
            result.m_bool = value;
            return result;
        }

        static ConfigurationValue make_signed(std::int64_t value) {
            ConfigurationValue result;
            result.m_type = Type::SignedInteger;
            result.m_signed = value;
            return result;
        }

        static ConfigurationValue make_unsigned(std::uint64_t value) {
            ConfigurationValue result;
            result.m_type = Type::UnsignedInteger;
            result.m_unsigned = value;
            return result;
        }

        static ConfigurationValue make_string(const std::string& value) {
            ConfigurationValue result;
            result.m_type = Type::String;
            result.m_string = value;
            return result;
        }

        static ConfigurationValue make_array(const std::vector<ConfigurationValue>& value) {
            ConfigurationValue result;
            result.m_type = Type::Array;
            result.m_array.reset(new std::vector<ConfigurationValue>(value));
            return result;
        }

        static ConfigurationValue make_object(
                const std::map<std::string, ConfigurationValue>& value) {
            ConfigurationValue result;
            result.m_type = Type::Object;
            result.m_object.reset(new std::map<std::string, ConfigurationValue>(value));
            return result;
        }

        Type type() const { return m_type; }

        bool get_boolean(bool& value) const {
            if (m_type != Type::Boolean) return false;
            value = m_bool;
            return true;
        }

        bool get_signed(std::int64_t& value) const {
            if (m_type != Type::SignedInteger) return false;
            value = m_signed;
            return true;
        }

        bool get_unsigned(std::uint64_t& value) const {
            if (m_type != Type::UnsignedInteger) return false;
            value = m_unsigned;
            return true;
        }

        bool get_string(std::string& value) const {
            if (m_type != Type::String) return false;
            value = m_string;
            return true;
        }

        bool get_array(std::vector<ConfigurationValue>& value) const {
            if (m_type != Type::Array || !m_array) return false;
            value = *m_array;
            return true;
        }

        bool get_object(std::map<std::string, ConfigurationValue>& value) const {
            if (m_type != Type::Object || !m_object) return false;
            value = *m_object;
            return true;
        }

    private:
        Type m_type;
        bool m_bool;
        std::int64_t m_signed;
        std::uint64_t m_unsigned;
        std::string m_string;
        std::shared_ptr<std::vector<ConfigurationValue>> m_array;
        std::shared_ptr<std::map<std::string, ConfigurationValue>> m_object;
    };

    typedef std::map<std::string, ConfigurationValue> ConfigurationFieldMap;

    /// \struct ConfigurationFormatterSpec
    /// \brief Data-only formatter settings in the semantic model.
    struct ConfigurationFormatterSpec {
        std::string type = "simple";
        bool has_pattern = false;
        std::string pattern;
        bool has_json_format = false;
        bool json_format = false;
    };

    /// \struct ConfigurationStrategySpec
    /// \brief Logger-level settings independent of backend-native fields.
    struct ConfigurationStrategySpec {
        bool has_level = false;
        LogLevel level = LogLevel::LOG_LVL_TRACE;
        bool has_formatter = false;
        ConfigurationFormatterSpec formatter;
    };

    /// \struct ConfigurationLoggerSpec
    /// \brief One parser-neutral logger declaration.
    struct ConfigurationLoggerSpec {
        std::string id;
        std::string type;
        ConfigurationStrategySpec strategy;
        ConfigurationFieldMap backend;
    };

    /// \struct ConfigurationDocument
    /// \brief Versioned parser-neutral configuration document.
    struct ConfigurationDocument {
        std::uint32_t schema_version = 0;
        std::vector<ConfigurationLoggerSpec> loggers;
    };

    /// \struct ConfigurationMaterializedLogger
    /// \brief Side-effect-free materialized logger plan entry.
    struct ConfigurationMaterializedLogger {
        std::string id;
        std::string type;
        ConfigurationStrategySpec strategy;
        ConfigurationFieldMap backend;
        std::vector<std::string> required_bindings;
    };

    /// \struct ConfigurationPlan
    /// \brief Complete side-effect-free materialization result.
    struct ConfigurationPlan {
        std::uint32_t schema_version = 0;
        std::vector<ConfigurationMaterializedLogger> loggers;
    };

    /// \class ConfigurationMaterializer
    /// \brief Backend adapter contract; it must not instantiate live loggers.
    class ConfigurationMaterializer {
    public:
        virtual ~ConfigurationMaterializer() {}

        /// \brief Validate backend fields and required bindings without side effects.
        ///
        /// Returning true with a diagnostic is still a failed phase. Adapters
        /// must add only error diagnostics; warnings are not part of this v1
        /// contract. Diagnostic paths are relative to the current logger root
        /// (for example `backend.foo`, `type`, or `binding`); the document
        /// layer prefixes them with `loggers[N]`.
        virtual bool validate_backend(const ConfigurationLoggerSpec& logger,
                                      ConfigurationDiagnostics& diagnostics) const = 0;

        /// \brief Materialize a DTO after all document validation has succeeded.
        ///
        /// Diagnostic paths follow the same relative-to-logger-root rule as
        /// `validate_backend()`; the document layer adds the logger index.
        virtual bool materialize_backend(const ConfigurationLoggerSpec& logger,
                                         ConfigurationMaterializedLogger& output,
                                         ConfigurationDiagnostics& diagnostics) const = 0;
    };

    /// \brief Return whether a type names a built-in backend known to the contract.
    inline bool is_known_configuration_backend(const std::string& type) {
        return type == "console" ||
               type == "file" ||
               type == "unique_file" ||
               type == "memory" ||
               type == "crash" ||
               type == "syslog" ||
               type == "event_log" ||
               type == "windows_debug" ||
               type == "mdbx" ||
               type == "otlp_http" ||
               type == "otlp_payload" ||
               type == "prometheus_http" ||
               type == "prometheus_payload";
    }

    inline bool configuration_log_level_is_valid(LogLevel level) {
        switch (level) {
            case LogLevel::LOG_LVL_TRACE:
            case LogLevel::LOG_LVL_DEBUG:
            case LogLevel::LOG_LVL_INFO:
            case LogLevel::LOG_LVL_WARN:
            case LogLevel::LOG_LVL_ERROR:
            case LogLevel::LOG_LVL_FATAL:
                return true;
        }
        return false;
    }

    inline bool configuration_get_bool(const ConfigurationFieldMap& fields,
                                       const char* name,
                                       bool& value,
                                       bool& present) {
        ConfigurationFieldMap::const_iterator it = fields.find(name);
        present = it != fields.end();
        return !present || it->second.get_boolean(value);
    }

    inline bool configuration_has_field(const ConfigurationFieldMap& fields, const char* name) {
        return fields.find(name) != fields.end();
    }

    /// \brief Validate document structure and strategy-level combinations.
    inline ConfigurationDiagnostics validate_configuration(const ConfigurationDocument& document) {
        ConfigurationDiagnostics diagnostics;
        if (document.schema_version != 1) {
            diagnostics.add(ConfigurationErrorCode::UnsupportedSchemaVersion,
                            "schema_version",
                            "Only configuration schema version 1 is supported");
        }

        std::map<std::string, bool> seen_ids;
        for (std::size_t index = 0; index < document.loggers.size(); ++index) {
            const ConfigurationLoggerSpec& logger = document.loggers[index];
            const std::string path = "loggers[" + std::to_string(index) + "]";
            if (logger.id.empty()) {
                diagnostics.add(ConfigurationErrorCode::EmptyLoggerId,
                                path + ".id",
                                "Logger id must not be empty");
            } else if (seen_ids.find(logger.id) != seen_ids.end()) {
                diagnostics.add(ConfigurationErrorCode::DuplicateLoggerId,
                                path + ".id",
                                "Logger id must be unique within the document");
            } else {
                seen_ids[logger.id] = true;
            }

            if (logger.type.empty()) {
                diagnostics.add(ConfigurationErrorCode::EmptyBackendType,
                                path + ".type",
                                "Logger backend type must not be empty");
            } else if (!is_known_configuration_backend(logger.type)) {
                diagnostics.add(ConfigurationErrorCode::UnknownBackendType,
                                path + ".type",
                                "Logger backend type is not known to the v1 contract");
            }

            if (logger.strategy.has_level &&
                !configuration_log_level_is_valid(logger.strategy.level)) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                path + ".strategy.level",
                                "Logger level is not valid");
            }
            if (logger.strategy.has_formatter && logger.strategy.formatter.type != "simple") {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                path + ".strategy.formatter.type",
                                "Only the simple formatter is defined by the v1 model");
            }

            bool dedicated = false;
            bool dedicated_present = false;
            if (!configuration_get_bool(logger.backend,
                                        "use_dedicated_executor",
                                        dedicated,
                                        dedicated_present)) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                path + ".backend.use_dedicated_executor",
                                "use_dedicated_executor must be boolean");
            }

            bool async = true;
            bool async_present = false;
            if (!configuration_get_bool(logger.backend, "async", async, async_present)) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                path + ".backend.async",
                                "async must be boolean");
            }

            const bool has_executor_field =
                configuration_has_field(logger.backend, "queue_capacity") ||
                configuration_has_field(logger.backend, "queue_policy");
            if (has_executor_field && (!dedicated_present || !dedicated ||
                                       (async_present && !async))) {
                diagnostics.add(ConfigurationErrorCode::UnsupportedCombination,
                                path + ".backend",
                                "Executor queue fields require an effective dedicated executor");
            }

            ConfigurationFieldMap::const_iterator queue_policy =
                logger.backend.find("queue_policy");
            if (queue_policy != logger.backend.end()) {
                std::string value;
                if (!queue_policy->second.get_string(value) ||
                    (value != "block" && value != "drop_newest" && value != "drop_oldest")) {
                    diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                    path + ".backend.queue_policy",
                                    "queue_policy must be block, drop_newest, or drop_oldest");
                }
            }

            ConfigurationFieldMap::const_iterator queue_capacity =
                logger.backend.find("queue_capacity");
            if (queue_capacity != logger.backend.end() &&
                queue_capacity->second.type() != ConfigurationValue::Type::UnsignedInteger) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                path + ".backend.queue_capacity",
                                "queue_capacity must be an unsigned integer");
            }
        }
        return diagnostics;
    }

    /// \brief Validate then materialize a complete side-effect-free plan.
    inline bool materialize_configuration(const ConfigurationDocument& document,
                                          const ConfigurationMaterializer& materializer,
                                          ConfigurationPlan& plan,
                                          ConfigurationDiagnostics& diagnostics) {
        diagnostics = validate_configuration(document);
        if (!diagnostics.empty()) {
            return false;
        }

        for (std::size_t index = 0; index < document.loggers.size(); ++index) {
            ConfigurationDiagnostics local_diagnostics;
            const bool valid = materializer.validate_backend(document.loggers[index],
                                                              local_diagnostics);
            if (!valid || !local_diagnostics.empty()) {
                const std::string prefix = "loggers[" + std::to_string(index) + "]";
                if (local_diagnostics.empty()) {
                    diagnostics.add(ConfigurationErrorCode::BackendValidationFailed,
                                    prefix + ".backend",
                                    "Backend materializer rejected the logger configuration");
                } else {
                    diagnostics.append_rebased(local_diagnostics, prefix);
                }
                return false;
            }
        }

        ConfigurationPlan candidate;
        candidate.schema_version = document.schema_version;
        candidate.loggers.reserve(document.loggers.size());
        for (std::size_t index = 0; index < document.loggers.size(); ++index) {
            ConfigurationMaterializedLogger output;
            ConfigurationDiagnostics local_diagnostics;
            const bool materialized = materializer.materialize_backend(
                document.loggers[index], output, local_diagnostics);
            if (!materialized || !local_diagnostics.empty()) {
                const std::string prefix = "loggers[" + std::to_string(index) + "]";
                if (local_diagnostics.empty()) {
                    diagnostics.add(ConfigurationErrorCode::MaterializationFailed,
                                    prefix,
                                    "Backend materializer could not produce a logger plan");
                } else {
                    diagnostics.append_rebased(local_diagnostics, prefix);
                }
                return false;
            }
            candidate.loggers.push_back(output);
        }
        plan = candidate;
        return true;
    }

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_HPP_INCLUDED
