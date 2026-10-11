#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_BUILTINS_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_BUILTINS_HPP_INCLUDED

/// \file configuration_builtins.hpp
/// \brief Side-effect-free adapters for built-in backend configuration schemas.

#include "configuration.hpp"

namespace logit {

    /// \class BuiltinConfigurationMaterializer
    /// \brief Validates and materializes data-only plans for built-in backends.
    ///
    /// This adapter deliberately does not include backend implementation headers,
    /// allocate resources, invoke callbacks, or instantiate loggers. It applies
    /// the defaults documented by the public backend Config structures and leaves
    /// executable bindings to a later instantiation phase.
    class BuiltinConfigurationMaterializer : public ConfigurationMaterializer {
    public:
        bool validate_backend(const ConfigurationLoggerSpec& logger,
                              ConfigurationDiagnostics& diagnostics) const override {
            if (!is_known_configuration_backend(logger.type)) {
                diagnostics.add(ConfigurationErrorCode::UnknownBackendType,
                                "type",
                                "Built-in materializer does not know this backend");
                return false;
            }

            if (!backend_available(logger.type)) {
                diagnostics.add(ConfigurationErrorCode::UnavailableBackend,
                                "type",
                                "The selected optional backend is not enabled in this build");
                return false;
            }

            if (!adapter_supported(logger.type)) {
                diagnostics.add(ConfigurationErrorCode::UnavailableBackend,
                                "type",
                                "No built-in semantic adapter is defined for this backend yet");
                return false;
            }

            bool valid = true;
            for (ConfigurationFieldMap::const_iterator it = logger.backend.begin();
                 it != logger.backend.end(); ++it) {
                if (!field_allowed(logger.type, it->first)) {
                    diagnostics.add(ConfigurationErrorCode::UnknownField,
                                    "backend." + it->first,
                                    "Field is not part of the selected built-in backend");
                    valid = false;
                }
            }

            valid = validate_common_types(logger, diagnostics) && valid;
            if (logger.type == "file" || logger.type == "unique_file") {
                valid = validate_file_types(logger, diagnostics) && valid;
            } else if (logger.type == "memory") {
                valid = validate_memory_types(logger, diagnostics) && valid;
            } else if (logger.type == "mdbx") {
                valid = validate_mdbx_types(logger, diagnostics) && valid;
            } else if (logger.type == "otlp_http" || logger.type == "otlp_payload") {
                valid = validate_otlp_types(logger, diagnostics) && valid;
            } else if (logger.type == "prometheus_http" ||
                       logger.type == "prometheus_payload") {
                valid = validate_prometheus_types(logger, diagnostics) && valid;
            }
            return valid;
        }

        bool materialize_backend(const ConfigurationLoggerSpec& logger,
                                 ConfigurationMaterializedLogger& output,
                                 ConfigurationDiagnostics& diagnostics) const override {
            if (!validate_backend(logger, diagnostics)) {
                return false;
            }

            output.id = logger.id;
            output.type = logger.type;
            output.strategy = logger.strategy;
            output.backend = logger.backend;
            apply_defaults(logger.type, output.backend);
            if (logger.type == "otlp_payload" || logger.type == "prometheus_payload") {
                output.required_bindings.push_back("on_payload");
            }
            return true;
        }

    private:
        static bool backend_available(const std::string& type) {
            if (type == "mdbx") {
#if defined(LOGIT_WITH_MDBX)
                return true;
#else
                return false;
#endif
            }
            if (type == "otlp_http" || type == "otlp_payload") {
#if defined(LOGIT_WITH_OTLP)
                return true;
#else
                return false;
#endif
            }
            if (type == "prometheus_http") {
#if defined(LOGIT_WITH_PROMETHEUS_SERVER)
                return true;
#else
                return false;
#endif
            }
            if (type == "prometheus_payload") {
#if defined(LOGIT_WITH_PROMETHEUS)
                return true;
#else
                return false;
#endif
            }
            return true;
        }

        static bool adapter_supported(const std::string& type) {
            return type == "file" || type == "unique_file" || type == "memory" ||
                   type == "mdbx" || type == "otlp_http" ||
                   type == "otlp_payload" || type == "prometheus_http" ||
                   type == "prometheus_payload";
        }

        static bool field_allowed(const std::string& type, const std::string& field) {
            if (type == "file") {
                return field == "directory" || field == "async" ||
                       field == "auto_delete_days" || field == "max_file_size_bytes" ||
                       field == "max_rotated_files" || field == "compress" ||
                       field == "compress_level" || field == "compress_async" ||
                       field == "external_cmd" || field == "naming" ||
                       field == "seq_width" || field == "use_dedicated_executor" ||
                       field == "queue_capacity" || field == "queue_policy";
            }
            if (type == "unique_file") {
                return field == "directory" || field == "async" ||
                       field == "auto_delete_days" || field == "hash_length" ||
                       field == "use_dedicated_executor" || field == "queue_capacity" ||
                       field == "queue_policy";
            }
            if (type == "memory") {
                return field == "max_records" || field == "max_bytes" ||
                       field == "max_age_ms";
            }
            if (type == "mdbx") {
                return field == "path" || field == "app_name" || field == "session_id" ||
                       field == "async" || field == "drop_on_overflow" ||
                       field == "max_queue_size" || field == "max_batch_size" ||
                       field == "flush_interval_ms" || field == "large_payload_threshold" ||
                       field == "payload_preview_size" ||
                       field == "store_large_payloads_separately" ||
                       field == "payload_compression" ||
                       field == "payload_compression_level";
            }
            if (type == "otlp_http") {
                return field == "format" || field == "host" || field == "path" ||
                       field == "max_queue_size" || field == "max_batch_size" ||
                       field == "max_in_flight_requests" || field == "export_interval_ms" ||
                       field == "request_timeout_sec" || field == "retry_attempts" ||
                       field == "retry_delay_ms" || field == "max_payload_bytes" ||
                       field == "compression" || field == "compression_level" ||
                       field == "drop_on_overflow" || field == "async" ||
                       field == "cancel_on_shutdown";
            }
            if (type == "otlp_payload") {
                return field == "format" || field == "async" ||
                       field == "max_payload_bytes" || field == "max_batch_size" ||
                       field == "max_queue_size" || field == "drop_on_overflow" ||
                       field == "export_interval_ms";
            }
            if (type == "prometheus_http") {
                return field == "format" || field == "address" || field == "port" ||
                       field == "path" || field == "health_path" ||
                       field == "enable_health_endpoint" || field == "start_immediately";
            }
            if (type == "prometheus_payload") {
                return field == "format";
            }
            return false;
        }

        static bool has(const ConfigurationFieldMap& fields, const char* name) {
            return fields.find(name) != fields.end();
        }

        static bool type_is(const ConfigurationFieldMap& fields, const char* name,
                            ConfigurationValue::Type type) {
            ConfigurationFieldMap::const_iterator it = fields.find(name);
            return it == fields.end() || it->second.type() == type;
        }

        static bool validate_type(const ConfigurationLoggerSpec& logger,
                                  const char* name, ConfigurationValue::Type type,
                                  ConfigurationDiagnostics& diagnostics) {
            if (type_is(logger.backend, name, type)) return true;
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            "backend." + std::string(name),
                            "Field has an invalid value type");
            return false;
        }

        static bool validate_common_types(const ConfigurationLoggerSpec& logger,
                                          ConfigurationDiagnostics& diagnostics) {
            bool valid = true;
            valid = validate_type(logger, "async", ConfigurationValue::Type::Boolean,
                                  diagnostics) && valid;
            valid = validate_type(logger, "use_dedicated_executor",
                                  ConfigurationValue::Type::Boolean, diagnostics) && valid;
            valid = validate_type(logger, "queue_capacity",
                                  ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "queue_policy", ConfigurationValue::Type::String,
                                  diagnostics) && valid;
            return valid;
        }

        static bool validate_file_types(const ConfigurationLoggerSpec& logger,
                                        ConfigurationDiagnostics& diagnostics) {
            bool valid = true;
            valid = validate_type(logger, "directory", ConfigurationValue::Type::String,
                                  diagnostics) && valid;
            valid = validate_type(logger, "auto_delete_days", ConfigurationValue::Type::SignedInteger,
                                  diagnostics) && valid;
            valid = validate_type(logger, "max_file_size_bytes",
                                  ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "max_rotated_files",
                                  ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "compress", ConfigurationValue::Type::String,
                                  diagnostics) && valid;
            valid = validate_type(logger, "compress_level", ConfigurationValue::Type::SignedInteger,
                                  diagnostics) && valid;
            valid = validate_type(logger, "compress_async", ConfigurationValue::Type::Boolean,
                                  diagnostics) && valid;
            valid = validate_type(logger, "external_cmd", ConfigurationValue::Type::String,
                                  diagnostics) && valid;
            valid = validate_type(logger, "naming", ConfigurationValue::Type::String,
                                  diagnostics) && valid;
            valid = validate_type(logger, "seq_width", ConfigurationValue::Type::UnsignedInteger,
                                  diagnostics) && valid;
            valid = validate_enum(logger, "compress", "none", "gzip", "zstd", "external_cmd",
                                  diagnostics) && valid;
            valid = validate_enum(logger, "naming", "sequence", "timestamp", "timestamp_ms",
                                  diagnostics) && valid;
            if (logger.type == "unique_file") {
                valid = validate_type(logger, "hash_length", ConfigurationValue::Type::UnsignedInteger,
                                      diagnostics) && valid;
            }
            return valid;
        }

        static bool validate_memory_types(const ConfigurationLoggerSpec& logger,
                                          ConfigurationDiagnostics& diagnostics) {
            bool valid = true;
            valid = validate_type(logger, "max_records", ConfigurationValue::Type::UnsignedInteger,
                                  diagnostics) && valid;
            valid = validate_type(logger, "max_bytes", ConfigurationValue::Type::UnsignedInteger,
                                  diagnostics) && valid;
            valid = validate_type(logger, "max_age_ms", ConfigurationValue::Type::SignedInteger,
                                  diagnostics) && valid;
            return valid;
        }

        static bool validate_mdbx_types(const ConfigurationLoggerSpec& logger,
                                        ConfigurationDiagnostics& diagnostics) {
            bool valid = true;
            valid = validate_type(logger, "path", ConfigurationValue::Type::String, diagnostics) && valid;
            valid = validate_type(logger, "app_name", ConfigurationValue::Type::String, diagnostics) && valid;
            valid = validate_type(logger, "session_id", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "async", ConfigurationValue::Type::Boolean, diagnostics) && valid;
            valid = validate_type(logger, "drop_on_overflow", ConfigurationValue::Type::Boolean, diagnostics) && valid;
            valid = validate_type(logger, "max_queue_size", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "max_batch_size", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "flush_interval_ms", ConfigurationValue::Type::SignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "large_payload_threshold", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "payload_preview_size", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "store_large_payloads_separately", ConfigurationValue::Type::Boolean, diagnostics) && valid;
            valid = validate_type(logger, "payload_compression", ConfigurationValue::Type::String, diagnostics) && valid;
            valid = validate_type(logger, "payload_compression_level", ConfigurationValue::Type::SignedInteger, diagnostics) && valid;
            valid = validate_enum(logger, "payload_compression", "none", "gzip", "zstd",
                                  diagnostics) && valid;
            return valid;
        }

        static bool validate_otlp_types(const ConfigurationLoggerSpec& logger,
                                        ConfigurationDiagnostics& diagnostics) {
            bool valid = true;
            valid = validate_otlp_format(logger, diagnostics) && valid;
            valid = validate_type(logger, "host", ConfigurationValue::Type::String, diagnostics) && valid;
            valid = validate_type(logger, "path", ConfigurationValue::Type::String, diagnostics) && valid;
            valid = validate_type(logger, "max_queue_size", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "max_batch_size", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "max_in_flight_requests", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "export_interval_ms",
                                  logger.type == "otlp_payload"
                                      ? ConfigurationValue::Type::UnsignedInteger
                                      : ConfigurationValue::Type::SignedInteger,
                                  diagnostics) && valid;
            valid = validate_type(logger, "request_timeout_sec", ConfigurationValue::Type::SignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "retry_attempts", ConfigurationValue::Type::SignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "retry_delay_ms", ConfigurationValue::Type::SignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "max_payload_bytes", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "compression", ConfigurationValue::Type::String, diagnostics) && valid;
            valid = validate_type(logger, "compression_level", ConfigurationValue::Type::SignedInteger, diagnostics) && valid;
            valid = validate_type(logger, "drop_on_overflow", ConfigurationValue::Type::Boolean, diagnostics) && valid;
            valid = validate_type(logger, "async", ConfigurationValue::Type::Boolean, diagnostics) && valid;
            valid = validate_type(logger, "cancel_on_shutdown", ConfigurationValue::Type::Boolean, diagnostics) && valid;
            valid = validate_enum(logger, "compression", "none", "gzip", "zstd", diagnostics) && valid;
            return valid;
        }

        static bool validate_enum(const ConfigurationLoggerSpec& logger, const char* name,
                                  const char* first, const char* second, const char* third,
                                  const char* fourth, ConfigurationDiagnostics& diagnostics) {
            ConfigurationFieldMap::const_iterator it = logger.backend.find(name);
            if (it == logger.backend.end()) return true;
            std::string value;
            if (!it->second.get_string(value) ||
                (value != first && value != second && value != third && value != fourth)) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                "backend." + std::string(name),
                                "Field has an unsupported named value");
                return false;
            }
            return true;
        }

        static bool validate_enum(const ConfigurationLoggerSpec& logger, const char* name,
                                  const char* first, const char* second, const char* third,
                                  ConfigurationDiagnostics& diagnostics) {
            ConfigurationFieldMap::const_iterator it = logger.backend.find(name);
            if (it == logger.backend.end()) return true;
            std::string value;
            if (!it->second.get_string(value) ||
                (value != first && value != second && value != third)) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                "backend." + std::string(name),
                                "Field has an unsupported named value");
                return false;
            }
            return true;
        }

        static bool validate_prometheus_types(const ConfigurationLoggerSpec& logger,
                                              ConfigurationDiagnostics& diagnostics) {
            bool valid = validate_prometheus_format(logger, diagnostics);
            if (logger.type == "prometheus_http") {
                valid = validate_type(logger, "address", ConfigurationValue::Type::String, diagnostics) && valid;
                valid = validate_type(logger, "port", ConfigurationValue::Type::UnsignedInteger, diagnostics) && valid;
                valid = validate_type(logger, "path", ConfigurationValue::Type::String, diagnostics) && valid;
                valid = validate_type(logger, "health_path", ConfigurationValue::Type::String, diagnostics) && valid;
                valid = validate_type(logger, "enable_health_endpoint", ConfigurationValue::Type::Boolean, diagnostics) && valid;
                valid = validate_type(logger, "start_immediately", ConfigurationValue::Type::Boolean, diagnostics) && valid;
            }
            return valid;
        }

        static bool validate_otlp_format(const ConfigurationLoggerSpec& logger,
                                         ConfigurationDiagnostics& diagnostics) {
            ConfigurationFieldMap::const_iterator it = logger.backend.find("format");
            if (it == logger.backend.end()) return true;
            if (it->second.type() != ConfigurationValue::Type::Object) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                "backend.format",
                                "format must be an object");
                return false;
            }
            std::map<std::string, ConfigurationValue> fields;
            it->second.get_object(fields);
            bool valid = true;
            for (std::map<std::string, ConfigurationValue>::const_iterator field = fields.begin();
                 field != fields.end(); ++field) {
                const bool string_field = field->first == "service_name" ||
                    field->first == "service_namespace" || field->first == "service_instance_id" ||
                    field->first == "deployment_environment" || field->first == "args_prefix";
                const bool bool_field = field->first == "include_source" ||
                    field->first == "include_thread_id" || field->first == "include_format" ||
                    field->first == "include_arg_names" || field->first == "include_args";
                if (!string_field && !bool_field) {
                    diagnostics.add(ConfigurationErrorCode::UnknownField,
                                    "backend.format." + field->first,
                                    "Field is not part of OtlpJsonFormatConfig");
                    valid = false;
                } else if ((string_field && field->second.type() != ConfigurationValue::Type::String) ||
                           (bool_field && field->second.type() != ConfigurationValue::Type::Boolean)) {
                    diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                    "backend.format." + field->first,
                                    "Field has an invalid value type");
                    valid = false;
                }
            }
            return valid;
        }

        static bool validate_prometheus_format(const ConfigurationLoggerSpec& logger,
                                               ConfigurationDiagnostics& diagnostics) {
            ConfigurationFieldMap::const_iterator it = logger.backend.find("format");
            if (it == logger.backend.end()) return true;
            if (it->second.type() != ConfigurationValue::Type::Object) {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                "backend.format",
                                "format must be an object");
                return false;
            }
            std::map<std::string, ConfigurationValue> fields;
            it->second.get_object(fields);
            bool valid = true;
            for (std::map<std::string, ConfigurationValue>::const_iterator field = fields.begin();
                 field != fields.end(); ++field) {
                const bool string_field = field->first == "metric_prefix" ||
                    field->first == "logger_label_name" || field->first == "instance_label_name" ||
                    field->first == "instance_label_value";
                const bool bool_field = field->first == "include_help" ||
                    field->first == "include_type" || field->first == "include_timestamp" ||
                    field->first == "include_logger_label" || field->first == "include_instance_label" ||
                    field->first == "include_build_info";
                const bool labels_field = field->first == "const_labels";
                if (!string_field && !bool_field && !labels_field) {
                    diagnostics.add(ConfigurationErrorCode::UnknownField,
                                    "backend.format." + field->first,
                                    "Field is not part of PrometheusTextFormatConfig");
                    valid = false;
                } else if ((string_field && field->second.type() != ConfigurationValue::Type::String) ||
                           (bool_field && field->second.type() != ConfigurationValue::Type::Boolean) ||
                           (labels_field && field->second.type() != ConfigurationValue::Type::Array)) {
                    diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                    "backend.format." + field->first,
                                    "Field has an invalid value type");
                    valid = false;
                }
                if (labels_field && field->second.type() == ConfigurationValue::Type::Array) {
                    std::vector<ConfigurationValue> labels;
                    field->second.get_array(labels);
                    for (std::size_t index = 0; index < labels.size(); ++index) {
                        std::map<std::string, ConfigurationValue> label;
                        if (!labels[index].get_object(label)) {
                            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                            "backend.format.const_labels[" +
                                                std::to_string(index) + "]",
                                            "Each constant label must be an object");
                            valid = false;
                            continue;
                        }
                        for (std::map<std::string, ConfigurationValue>::const_iterator member = label.begin();
                             member != label.end(); ++member) {
                            if (member->first != "name" && member->first != "value") {
                                diagnostics.add(ConfigurationErrorCode::UnknownField,
                                                "backend.format.const_labels[" +
                                                    std::to_string(index) + "]." + member->first,
                                                "Field is not part of PrometheusLabel");
                                valid = false;
                            }
                        }
                        if (label.find("name") == label.end() ||
                            label.find("value") == label.end() ||
                            label["name"].type() != ConfigurationValue::Type::String ||
                            label["value"].type() != ConfigurationValue::Type::String) {
                            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                            "backend.format.const_labels[" +
                                                std::to_string(index) + "]",
                                            "Constant labels require string name and value");
                            valid = false;
                        }
                    }
                }
            }
            return valid;
        }

        static void set_default(ConfigurationFieldMap& fields, const char* name,
                                const ConfigurationValue& value) {
            if (!has(fields, name)) fields[name] = value;
        }

        static void merge_object_defaults(ConfigurationFieldMap& fields, const char* name,
                                          const ConfigurationFieldMap& defaults) {
            ConfigurationFieldMap::iterator it = fields.find(name);
            if (it == fields.end()) {
                fields[name] = ConfigurationValue::make_object(defaults);
                return;
            }
            ConfigurationFieldMap observed;
            if (!it->second.get_object(observed)) return;
            for (ConfigurationFieldMap::const_iterator field = defaults.begin();
                 field != defaults.end(); ++field) {
                if (observed.find(field->first) == observed.end()) {
                    observed[field->first] = field->second;
                }
            }
            it->second = ConfigurationValue::make_object(observed);
        }

        static void apply_defaults(const std::string& type, ConfigurationFieldMap& fields) {
            if (type == "file") {
                set_default(fields, "directory", ConfigurationValue::make_string("logs"));
                set_default(fields, "async", ConfigurationValue::make_boolean(true));
                set_default(fields, "auto_delete_days", ConfigurationValue::make_signed(30));
                set_default(fields, "max_file_size_bytes", ConfigurationValue::make_unsigned(0));
                set_default(fields, "max_rotated_files", ConfigurationValue::make_unsigned(0));
                set_default(fields, "compress", ConfigurationValue::make_string("none"));
                set_default(fields, "compress_level", ConfigurationValue::make_signed(1));
                set_default(fields, "compress_async", ConfigurationValue::make_boolean(true));
                set_default(fields, "external_cmd", ConfigurationValue::make_string(""));
                set_default(fields, "naming", ConfigurationValue::make_string("sequence"));
                set_default(fields, "seq_width", ConfigurationValue::make_unsigned(3));
                set_default(fields, "use_dedicated_executor", ConfigurationValue::make_boolean(false));
                set_default(fields, "queue_capacity", ConfigurationValue::make_unsigned(0));
                set_default(fields, "queue_policy", ConfigurationValue::make_string("block"));
            } else if (type == "unique_file") {
                set_default(fields, "directory", ConfigurationValue::make_string("unique_logs"));
                set_default(fields, "async", ConfigurationValue::make_boolean(true));
                set_default(fields, "auto_delete_days", ConfigurationValue::make_signed(30));
                set_default(fields, "hash_length", ConfigurationValue::make_unsigned(8));
                set_default(fields, "use_dedicated_executor", ConfigurationValue::make_boolean(false));
                set_default(fields, "queue_capacity", ConfigurationValue::make_unsigned(0));
                set_default(fields, "queue_policy", ConfigurationValue::make_string("block"));
            } else if (type == "memory") {
                set_default(fields, "max_records", ConfigurationValue::make_unsigned(1000));
                set_default(fields, "max_bytes", ConfigurationValue::make_unsigned(1024 * 1024));
                set_default(fields, "max_age_ms", ConfigurationValue::make_signed(24LL * 60 * 60 * 1000));
            } else if (type == "mdbx") {
                set_default(fields, "path", ConfigurationValue::make_string("logs.mdbx"));
                set_default(fields, "app_name", ConfigurationValue::make_string(""));
                set_default(fields, "session_id", ConfigurationValue::make_unsigned(0));
                set_default(fields, "async", ConfigurationValue::make_boolean(true));
                set_default(fields, "drop_on_overflow", ConfigurationValue::make_boolean(true));
                set_default(fields, "max_queue_size", ConfigurationValue::make_unsigned(8192));
                set_default(fields, "max_batch_size", ConfigurationValue::make_unsigned(256));
                set_default(fields, "flush_interval_ms", ConfigurationValue::make_signed(100));
                set_default(fields, "large_payload_threshold", ConfigurationValue::make_unsigned(4096));
                set_default(fields, "payload_preview_size", ConfigurationValue::make_unsigned(512));
                set_default(fields, "store_large_payloads_separately", ConfigurationValue::make_boolean(true));
                set_default(fields, "payload_compression", ConfigurationValue::make_string("none"));
                set_default(fields, "payload_compression_level", ConfigurationValue::make_signed(6));
            } else if (type == "otlp_http") {
                ConfigurationFieldMap format;
                format["service_name"] = ConfigurationValue::make_string("logit-app");
                format["service_namespace"] = ConfigurationValue::make_string("");
                format["service_instance_id"] = ConfigurationValue::make_string("");
                format["deployment_environment"] = ConfigurationValue::make_string("");
                format["include_source"] = ConfigurationValue::make_boolean(true);
                format["include_thread_id"] = ConfigurationValue::make_boolean(true);
                format["include_format"] = ConfigurationValue::make_boolean(true);
                format["include_arg_names"] = ConfigurationValue::make_boolean(false);
                format["include_args"] = ConfigurationValue::make_boolean(true);
                format["args_prefix"] = ConfigurationValue::make_string("logit.arg.");
                merge_object_defaults(fields, "format", format);
                set_default(fields, "host", ConfigurationValue::make_string("http://localhost:4318"));
                set_default(fields, "path", ConfigurationValue::make_string("/v1/logs"));
                set_default(fields, "max_queue_size", ConfigurationValue::make_unsigned(8192));
                set_default(fields, "max_batch_size", ConfigurationValue::make_unsigned(256));
                set_default(fields, "max_in_flight_requests", ConfigurationValue::make_unsigned(1));
                set_default(fields, "export_interval_ms", ConfigurationValue::make_signed(1000));
                set_default(fields, "request_timeout_sec", ConfigurationValue::make_signed(3));
                set_default(fields, "retry_attempts", ConfigurationValue::make_signed(2));
                set_default(fields, "retry_delay_ms", ConfigurationValue::make_signed(250));
                set_default(fields, "max_payload_bytes", ConfigurationValue::make_unsigned(1024 * 1024));
                set_default(fields, "compression", ConfigurationValue::make_string("none"));
                set_default(fields, "compression_level", ConfigurationValue::make_signed(6));
                set_default(fields, "drop_on_overflow", ConfigurationValue::make_boolean(true));
                set_default(fields, "async", ConfigurationValue::make_boolean(true));
                set_default(fields, "cancel_on_shutdown", ConfigurationValue::make_boolean(false));
            } else if (type == "otlp_payload") {
                ConfigurationFieldMap format;
                format["service_name"] = ConfigurationValue::make_string("logit-app");
                format["service_namespace"] = ConfigurationValue::make_string("");
                format["service_instance_id"] = ConfigurationValue::make_string("");
                format["deployment_environment"] = ConfigurationValue::make_string("");
                format["include_source"] = ConfigurationValue::make_boolean(true);
                format["include_thread_id"] = ConfigurationValue::make_boolean(true);
                format["include_format"] = ConfigurationValue::make_boolean(true);
                format["include_arg_names"] = ConfigurationValue::make_boolean(false);
                format["include_args"] = ConfigurationValue::make_boolean(true);
                format["args_prefix"] = ConfigurationValue::make_string("logit.arg.");
                merge_object_defaults(fields, "format", format);
                set_default(fields, "async", ConfigurationValue::make_boolean(true));
                set_default(fields, "max_payload_bytes", ConfigurationValue::make_unsigned(1024 * 1024));
                set_default(fields, "max_batch_size", ConfigurationValue::make_unsigned(256));
                set_default(fields, "max_queue_size", ConfigurationValue::make_unsigned(1024));
                set_default(fields, "drop_on_overflow", ConfigurationValue::make_boolean(true));
                set_default(fields, "export_interval_ms", ConfigurationValue::make_unsigned(100));
            } else if (type == "prometheus_http") {
                ConfigurationFieldMap format;
                format["include_help"] = ConfigurationValue::make_boolean(true);
                format["include_type"] = ConfigurationValue::make_boolean(true);
                format["include_timestamp"] = ConfigurationValue::make_boolean(false);
                format["metric_prefix"] = ConfigurationValue::make_string("logit_");
                format["const_labels"] = ConfigurationValue::make_array(
                    std::vector<ConfigurationValue>());
                format["include_logger_label"] = ConfigurationValue::make_boolean(true);
                format["logger_label_name"] = ConfigurationValue::make_string("logger");
                format["include_instance_label"] = ConfigurationValue::make_boolean(false);
                format["instance_label_name"] = ConfigurationValue::make_string("instance");
                format["instance_label_value"] = ConfigurationValue::make_string("");
                format["include_build_info"] = ConfigurationValue::make_boolean(true);
                merge_object_defaults(fields, "format", format);
                set_default(fields, "address", ConfigurationValue::make_string("0.0.0.0"));
                set_default(fields, "port", ConfigurationValue::make_unsigned(9090));
                set_default(fields, "path", ConfigurationValue::make_string("/metrics"));
                set_default(fields, "health_path", ConfigurationValue::make_string("/health"));
                set_default(fields, "enable_health_endpoint", ConfigurationValue::make_boolean(true));
                set_default(fields, "start_immediately", ConfigurationValue::make_boolean(true));
            } else if (type == "prometheus_payload") {
                ConfigurationFieldMap format;
                format["include_help"] = ConfigurationValue::make_boolean(true);
                format["include_type"] = ConfigurationValue::make_boolean(true);
                format["include_timestamp"] = ConfigurationValue::make_boolean(false);
                format["metric_prefix"] = ConfigurationValue::make_string("logit_");
                format["const_labels"] = ConfigurationValue::make_array(
                    std::vector<ConfigurationValue>());
                format["include_logger_label"] = ConfigurationValue::make_boolean(true);
                format["logger_label_name"] = ConfigurationValue::make_string("logger");
                format["include_instance_label"] = ConfigurationValue::make_boolean(false);
                format["instance_label_name"] = ConfigurationValue::make_string("instance");
                format["instance_label_value"] = ConfigurationValue::make_string("");
                format["include_build_info"] = ConfigurationValue::make_boolean(true);
                merge_object_defaults(fields, "format", format);
                set_default(fields, "emit_on_log", ConfigurationValue::make_boolean(false));
                set_default(fields, "emit_on_wait", ConfigurationValue::make_boolean(true));
            }
        }
    };

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_BUILTINS_HPP_INCLUDED
