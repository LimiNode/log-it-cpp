#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_JSON_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_JSON_HPP_INCLUDED

/// \file configuration_json.hpp
/// \brief Optional nlohmann::json frontend for the parser-neutral configuration model.

#ifndef LOGIT_WITH_JSON_CONFIG
#   error "configuration_json.hpp requires LOGIT_WITH_JSON_CONFIG=1"
#endif

#include "configuration.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace logit {
namespace configuration_json_detail {

    inline void reject_unknown_fields(const nlohmann::json& object,
                                      const std::set<std::string>& allowed,
                                      const std::string& path,
                                      ConfigurationDiagnostics& diagnostics) {
        for (nlohmann::json::const_iterator it = object.begin(); it != object.end(); ++it) {
            if (allowed.find(it.key()) == allowed.end()) {
                diagnostics.add(ConfigurationErrorCode::UnknownField,
                                path.empty() ? it.key() : path + "." + it.key(),
                                "Unknown JSON configuration field");
            }
        }
    }

    inline bool to_configuration_value(const nlohmann::json& input,
                                       const std::string& path,
                                       ConfigurationValue& output,
                                       ConfigurationDiagnostics& diagnostics,
                                       bool prefer_signed_integer = false) {
        if (input.is_null()) {
            output = ConfigurationValue();
            return true;
        }
        if (input.is_boolean()) {
            output = ConfigurationValue::make_boolean(input.get<bool>());
            return true;
        }
        if (input.is_number_unsigned()) {
            const std::uint64_t value = input.get<std::uint64_t>();
            if (prefer_signed_integer) {
                if (value > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
                    diagnostics.add(ConfigurationErrorCode::InvalidValue, path,
                                    "Integer value exceeds the signed 64-bit range");
                    return false;
                }
                output = ConfigurationValue::make_signed(static_cast<std::int64_t>(value));
            } else {
                output = ConfigurationValue::make_unsigned(value);
            }
            return true;
        }
        if (input.is_number_integer()) {
            const std::int64_t value = input.get<std::int64_t>();
            if (value >= 0 && !prefer_signed_integer) {
                output = ConfigurationValue::make_unsigned(static_cast<std::uint64_t>(value));
            } else {
                output = ConfigurationValue::make_signed(value);
            }
            return true;
        }
        if (input.is_number_float()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            path,
                            "Floating-point values are not supported by configuration schema v1");
            return false;
        }
        if (input.is_string()) {
            output = ConfigurationValue::make_string(input.get<std::string>());
            return true;
        }
        if (input.is_array()) {
            std::vector<ConfigurationValue> values;
            values.reserve(input.size());
            bool valid = true;
            for (std::size_t index = 0; index < input.size(); ++index) {
                ConfigurationValue value;
                if (!to_configuration_value(input[index],
                                            path + "[" + std::to_string(index) + "]",
                                            value,
                                            diagnostics)) {
                    valid = false;
                }
                values.push_back(value);
            }
            output = ConfigurationValue::make_array(values);
            return valid;
        }
        if (input.is_object()) {
            ConfigurationFieldMap fields;
            bool valid = true;
            for (nlohmann::json::const_iterator it = input.begin(); it != input.end(); ++it) {
                ConfigurationValue value;
                if (!to_configuration_value(it.value(), path + "." + it.key(),
                                            value, diagnostics)) {
                    valid = false;
                }
                fields[it.key()] = value;
            }
            output = ConfigurationValue::make_object(fields);
            return valid;
        }
        diagnostics.add(ConfigurationErrorCode::InvalidValue,
                        path,
                        "Unsupported JSON value");
        return false;
    }

    inline bool read_formatter(const nlohmann::json& input,
                               const std::string& path,
                               ConfigurationFormatterSpec& output,
                               ConfigurationDiagnostics& diagnostics) {
        if (!input.is_object()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path,
                            "formatter must be an object");
            return false;
        }
        reject_unknown_fields(input, std::set<std::string>{"type", "pattern", "json"},
                              path, diagnostics);
        bool valid = true;
        nlohmann::json::const_iterator it = input.find("type");
        if (it != input.end()) {
            if (!it->is_string()) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".type",
                                "formatter type must be a string");
                valid = false;
            } else {
                output.type = it->get<std::string>();
            }
        }
        it = input.find("pattern");
        if (it != input.end()) {
            if (!it->is_string()) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".pattern",
                                "formatter pattern must be a string");
                valid = false;
            } else {
                output.has_pattern = true;
                output.pattern = it->get<std::string>();
            }
        }
        it = input.find("json");
        if (it != input.end()) {
            if (!it->is_boolean()) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".json",
                                "formatter json must be boolean");
                valid = false;
            } else {
                output.has_json_format = true;
                output.json_format = it->get<bool>();
            }
        }
        return valid;
    }

    inline bool read_level(const nlohmann::json& input, const std::string& path,
                           ConfigurationStrategySpec& output,
                           ConfigurationDiagnostics& diagnostics) {
        if (!input.is_string()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path,
                            "level must be a named string value");
            return false;
        }
        const std::string value = input.get<std::string>();
        if (value == "trace") output.level = LogLevel::LOG_LVL_TRACE;
        else if (value == "debug") output.level = LogLevel::LOG_LVL_DEBUG;
        else if (value == "info") output.level = LogLevel::LOG_LVL_INFO;
        else if (value == "warn") output.level = LogLevel::LOG_LVL_WARN;
        else if (value == "error") output.level = LogLevel::LOG_LVL_ERROR;
        else if (value == "fatal") output.level = LogLevel::LOG_LVL_FATAL;
        else {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path,
                            "Unknown log level");
            return false;
        }
        output.has_level = true;
        return true;
    }

    inline bool read_strategy(const nlohmann::json& input, const std::string& path,
                              ConfigurationStrategySpec& output,
                              ConfigurationDiagnostics& diagnostics) {
        if (!input.is_object()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path,
                            "strategy must be an object");
            return false;
        }
        reject_unknown_fields(input, std::set<std::string>{"level", "formatter"},
                              path, diagnostics);
        bool valid = true;
        nlohmann::json::const_iterator it = input.find("level");
        if (it != input.end()) valid = read_level(*it, path + ".level", output, diagnostics) && valid;
        it = input.find("formatter");
        if (it != input.end()) {
            output.has_formatter = true;
            valid = read_formatter(*it, path + ".formatter", output.formatter, diagnostics) && valid;
        }
        return valid;
    }

    inline bool read_logger(const nlohmann::json& input, std::size_t index,
                            ConfigurationLoggerSpec& output,
                            ConfigurationDiagnostics& diagnostics) {
        const std::string path = "loggers[" + std::to_string(index) + "]";
        if (!input.is_object()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path,
                            "logger entry must be an object");
            return false;
        }
        reject_unknown_fields(input,
                              std::set<std::string>{"id", "type", "strategy", "backend"},
                              path, diagnostics);
        bool valid = true;
        nlohmann::json::const_iterator it = input.find("id");
        if (it == input.end() || !it->is_string()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".id",
                            "logger id is required and must be a string");
            valid = false;
        } else {
            output.id = it->get<std::string>();
        }
        it = input.find("type");
        if (it == input.end() || !it->is_string()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".type",
                            "logger type is required and must be a string");
            valid = false;
        } else {
            output.type = it->get<std::string>();
        }
        it = input.find("strategy");
        if (it != input.end()) valid = read_strategy(*it, path + ".strategy", output.strategy, diagnostics) && valid;
        it = input.find("backend");
        if (it != input.end()) {
            if (!it->is_object()) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".backend",
                                "backend must be an object");
                valid = false;
            } else {
                for (nlohmann::json::const_iterator field = it->begin(); field != it->end(); ++field) {
                    ConfigurationValue value;
                    if (!to_configuration_value(field.value(),
                                                path + ".backend." + field.key(),
                                                value, diagnostics,
                                                configuration_backend_field_uses_signed_integer(
                                                    output.type, field.key()))) {
                        valid = false;
                    }
                    output.backend[field.key()] = value;
                }
            }
        }
        return valid;
    }

} // namespace configuration_json_detail

    /// \brief Convert a parsed JSON value into the parser-neutral semantic model.
    ///
    /// The output is replaced only after the complete JSON document maps
    /// successfully. Semantic validation and backend materialization remain
    /// separate phases.
    inline bool parse_configuration_json(const nlohmann::json& input,
                                         ConfigurationDocument& output,
                                         ConfigurationDiagnostics& diagnostics) {
        diagnostics = ConfigurationDiagnostics();
        if (!input.is_object()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, "",
                            "Configuration JSON root must be an object");
            return false;
        }

        configuration_json_detail::reject_unknown_fields(
            input, std::set<std::string>{"schema_version", "loggers"}, "", diagnostics);
        ConfigurationDocument candidate;
        bool valid = true;
        nlohmann::json::const_iterator version = input.find("schema_version");
        std::uint64_t version_value = 0;
        bool version_valid = false;
        if (version != input.end() && version->is_number_unsigned()) {
            version_value = version->get<std::uint64_t>();
            version_valid = true;
        } else if (version != input.end() && version->is_number_integer()) {
            const std::int64_t signed_version = version->get<std::int64_t>();
            if (signed_version >= 0) {
                version_value = static_cast<std::uint64_t>(signed_version);
                version_valid = true;
            }
        }
        if (!version_valid ||
            version_value > (std::numeric_limits<std::uint32_t>::max)()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, "schema_version",
                            "schema_version is required and must be an unsigned 32-bit integer");
            valid = false;
        } else {
            candidate.schema_version = static_cast<std::uint32_t>(version_value);
        }

        nlohmann::json::const_iterator loggers = input.find("loggers");
        if (loggers == input.end() || !loggers->is_array()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, "loggers",
                            "loggers is required and must be an array");
            valid = false;
        } else {
            candidate.loggers.reserve(loggers->size());
            for (std::size_t index = 0; index < loggers->size(); ++index) {
                ConfigurationLoggerSpec logger;
                if (!configuration_json_detail::read_logger((*loggers)[index], index,
                                                            logger, diagnostics)) {
                    valid = false;
                }
                candidate.loggers.push_back(logger);
            }
        }

        if (!diagnostics.empty()) valid = false;
        if (!valid) return false;
        output = candidate;
        return true;
    }

    /// \brief Parse JSON text and convert it into the semantic configuration model.
    inline bool parse_configuration_json(const std::string& input,
                                         ConfigurationDocument& output,
                                         ConfigurationDiagnostics& diagnostics) {
        try {
            return parse_configuration_json(nlohmann::json::parse(input), output, diagnostics);
        } catch (const nlohmann::json::parse_error& error) {
            diagnostics = ConfigurationDiagnostics();
            diagnostics.add(ConfigurationErrorCode::ConfigurationParseFailed,
                            "",
                            std::string("Invalid configuration JSON: ") + error.what());
            return false;
        }
    }

    /// \brief Parse null-terminated JSON text into the semantic model.
    inline bool parse_configuration_json(const char* input,
                                         ConfigurationDocument& output,
                                         ConfigurationDiagnostics& diagnostics) {
        return parse_configuration_json(std::string(input ? input : ""),
                                        output, diagnostics);
    }

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_JSON_HPP_INCLUDED
