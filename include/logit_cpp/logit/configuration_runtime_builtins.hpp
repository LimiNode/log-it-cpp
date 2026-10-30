#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_BUILTINS_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_BUILTINS_HPP_INCLUDED

/// \file configuration_runtime_builtins.hpp
/// \brief Runtime factory for already-materialized built-in logger plans.

#include "configuration_runtime.hpp"

#include <cstdint>
#include <exception>
#include <functional>
#include <string>

namespace logit {
namespace configuration_runtime_detail {

    inline bool read_string(const ConfigurationMaterializedLogger& logger,
                            const char* name, std::string& value,
                            ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap::const_iterator it = logger.backend.find(name);
        if (it == logger.backend.end() || !it->second.get_string(value)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            std::string("backend.") + name,
                            "Materialized field must be a string");
            return false;
        }
        return true;
    }

    inline bool read_bool(const ConfigurationMaterializedLogger& logger,
                          const char* name, bool& value,
                          ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap::const_iterator it = logger.backend.find(name);
        if (it == logger.backend.end() || !it->second.get_boolean(value)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            std::string("backend.") + name,
                            "Materialized field must be boolean");
            return false;
        }
        return true;
    }

    inline bool read_unsigned(const ConfigurationMaterializedLogger& logger,
                              const char* name, std::uint64_t& value,
                              ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap::const_iterator it = logger.backend.find(name);
        if (it == logger.backend.end() || !it->second.get_unsigned(value)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            std::string("backend.") + name,
                            "Materialized field must be an unsigned integer");
            return false;
        }
        return true;
    }

    inline bool read_signed(const ConfigurationMaterializedLogger& logger,
                            const char* name, std::int64_t& value,
                            ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap::const_iterator it = logger.backend.find(name);
        if (it == logger.backend.end() || !it->second.get_signed(value)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            std::string("backend.") + name,
                            "Materialized field must be a signed integer");
            return false;
        }
        return true;
    }

    inline bool read_object(const ConfigurationMaterializedLogger& logger,
                            const char* name, ConfigurationFieldMap& value,
                            ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap::const_iterator it = logger.backend.find(name);
        if (it == logger.backend.end() || !it->second.get_object(value)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            std::string("backend.") + name,
                            "Materialized field must be an object");
            return false;
        }
        return true;
    }

    inline bool object_string(const ConfigurationFieldMap& object, const char* name,
                              std::string& value, const std::string& path,
                              ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap::const_iterator it = object.find(name);
        if (it == object.end() || !it->second.get_string(value)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            path + "." + name,
                            "Materialized nested field must be a string");
            return false;
        }
        return true;
    }

    inline bool object_bool(const ConfigurationFieldMap& object, const char* name,
                            bool& value, const std::string& path,
                            ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap::const_iterator it = object.find(name);
        if (it == object.end() || !it->second.get_boolean(value)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            path + "." + name,
                            "Materialized nested field must be boolean");
            return false;
        }
        return true;
    }

    inline bool read_queue_policy(const ConfigurationMaterializedLogger& logger,
                                  QueuePolicy& value,
                                  ConfigurationDiagnostics& diagnostics) {
        std::string policy;
        if (!read_string(logger, "queue_policy", policy, diagnostics)) return false;
        if (policy == "block") value = QueuePolicy::Block;
        else if (policy == "drop_newest") value = QueuePolicy::DropNewest;
        else if (policy == "drop_oldest") value = QueuePolicy::DropOldest;
        else {
            diagnostics.add(ConfigurationErrorCode::InvalidValue,
                            "backend.queue_policy",
                            "Unknown queue policy");
            return false;
        }
        return true;
    }

    inline bool read_file_config(const ConfigurationMaterializedLogger& logger,
                                 FileLogger::Config& config,
                                 ConfigurationDiagnostics& diagnostics) {
        bool valid = true;
        std::string string_value;
        std::uint64_t unsigned_value = 0;
        std::int64_t signed_value = 0;
        QueuePolicy queue_policy = QueuePolicy::Block;
        valid = read_string(logger, "directory", config.directory, diagnostics) && valid;
        valid = read_bool(logger, "async", config.async, diagnostics) && valid;
        if (read_signed(logger, "auto_delete_days", signed_value, diagnostics))
            config.auto_delete_days = static_cast<int>(signed_value);
        else valid = false;
        if (read_unsigned(logger, "max_file_size_bytes", unsigned_value, diagnostics))
            config.max_file_size_bytes = unsigned_value;
        else valid = false;
        if (read_unsigned(logger, "max_rotated_files", unsigned_value, diagnostics))
            config.max_rotated_files = static_cast<std::uint32_t>(unsigned_value);
        else valid = false;
        if (read_string(logger, "compress", string_value, diagnostics)) {
            if (string_value == "none") config.compress = CompressType::NONE;
            else if (string_value == "gzip") config.compress = CompressType::GZIP;
            else if (string_value == "zstd") config.compress = CompressType::ZSTD;
            else if (string_value == "external_cmd") config.compress = CompressType::EXTERNAL_CMD;
            else { diagnostics.add(ConfigurationErrorCode::InvalidValue, "backend.compress", "Unknown compression"); valid = false; }
        } else valid = false;
        if (read_signed(logger, "compress_level", signed_value, diagnostics)) config.compress_level = static_cast<int>(signed_value); else valid = false;
        valid = read_bool(logger, "compress_async", config.compress_async, diagnostics) && valid;
        valid = read_string(logger, "external_cmd", config.external_cmd, diagnostics) && valid;
        if (read_string(logger, "naming", string_value, diagnostics)) {
            if (string_value == "sequence") config.naming = RotationNaming::Sequence;
            else if (string_value == "timestamp") config.naming = RotationNaming::Timestamp;
            else if (string_value == "timestamp_ms") config.naming = RotationNaming::TimestampMs;
            else { diagnostics.add(ConfigurationErrorCode::InvalidValue, "backend.naming", "Unknown rotation naming"); valid = false; }
        } else valid = false;
        if (read_unsigned(logger, "seq_width", unsigned_value, diagnostics)) config.seq_width = static_cast<std::uint32_t>(unsigned_value); else valid = false;
        valid = read_bool(logger, "use_dedicated_executor", config.use_dedicated_executor, diagnostics) && valid;
        if (read_unsigned(logger, "queue_capacity", unsigned_value, diagnostics)) config.queue_capacity = static_cast<std::size_t>(unsigned_value); else valid = false;
        valid = read_queue_policy(logger, queue_policy, diagnostics) && valid;
        config.queue_policy = queue_policy;
        return valid;
    }

    inline bool read_unique_file_config(const ConfigurationMaterializedLogger& logger,
                                        UniqueFileLogger::Config& config,
                                        ConfigurationDiagnostics& diagnostics) {
        bool valid = true;
        std::uint64_t unsigned_value = 0;
        std::int64_t signed_value = 0;
        QueuePolicy queue_policy = QueuePolicy::Block;
        valid = read_string(logger, "directory", config.directory, diagnostics) && valid;
        valid = read_bool(logger, "async", config.async, diagnostics) && valid;
        if (read_signed(logger, "auto_delete_days", signed_value, diagnostics)) config.auto_delete_days = static_cast<int>(signed_value); else valid = false;
        if (read_unsigned(logger, "hash_length", unsigned_value, diagnostics)) config.hash_length = static_cast<std::size_t>(unsigned_value); else valid = false;
        valid = read_bool(logger, "use_dedicated_executor", config.use_dedicated_executor, diagnostics) && valid;
        if (read_unsigned(logger, "queue_capacity", unsigned_value, diagnostics)) config.queue_capacity = static_cast<std::size_t>(unsigned_value); else valid = false;
        valid = read_queue_policy(logger, queue_policy, diagnostics) && valid;
        config.queue_policy = queue_policy;
        return valid;
    }

    inline bool read_memory_config(const ConfigurationMaterializedLogger& logger,
                                   MemoryLogger::Config& config,
                                   ConfigurationDiagnostics& diagnostics) {
        bool valid = true;
        std::uint64_t unsigned_value = 0;
        std::int64_t signed_value = 0;
        if (read_unsigned(logger, "max_records", unsigned_value, diagnostics)) config.max_records = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_unsigned(logger, "max_bytes", unsigned_value, diagnostics)) config.max_bytes = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_signed(logger, "max_age_ms", signed_value, diagnostics)) config.max_age_ms = signed_value; else valid = false;
        return valid;
    }

#ifdef LOGIT_WITH_MDBX
    inline bool read_mdbx_config(const ConfigurationMaterializedLogger& logger,
                                 const ConfigurationBindingSet& bindings,
                                 MdbxLogger::Config& config,
                                 ConfigurationDiagnostics& diagnostics) {
        bool valid = true;
        std::uint64_t unsigned_value = 0;
        std::int64_t signed_value = 0;
        std::string compression;
        valid = read_string(logger, "path", config.path, diagnostics) && valid;
        valid = read_string(logger, "app_name", config.app_name, diagnostics) && valid;
        if (read_unsigned(logger, "session_id", unsigned_value, diagnostics)) config.session_id = unsigned_value; else valid = false;
        valid = read_bool(logger, "async", config.async, diagnostics) && valid;
        valid = read_bool(logger, "drop_on_overflow", config.drop_on_overflow, diagnostics) && valid;
        if (read_unsigned(logger, "max_queue_size", unsigned_value, diagnostics)) config.max_queue_size = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_unsigned(logger, "max_batch_size", unsigned_value, diagnostics)) config.max_batch_size = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_signed(logger, "flush_interval_ms", signed_value, diagnostics)) config.flush_interval_ms = static_cast<int>(signed_value); else valid = false;
        if (read_unsigned(logger, "large_payload_threshold", unsigned_value, diagnostics)) config.large_payload_threshold = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_unsigned(logger, "payload_preview_size", unsigned_value, diagnostics)) config.payload_preview_size = static_cast<std::size_t>(unsigned_value); else valid = false;
        valid = read_bool(logger, "store_large_payloads_separately", config.store_large_payloads_separately, diagnostics) && valid;
        if (read_string(logger, "payload_compression", compression, diagnostics)) {
            if (compression == "none") config.payload_compression = MdbxPayloadCompression::None;
            else if (compression == "gzip") config.payload_compression = MdbxPayloadCompression::Gzip;
            else if (compression == "zstd") config.payload_compression = MdbxPayloadCompression::Zstd;
            else {
                diagnostics.add(ConfigurationErrorCode::InvalidValue,
                                "backend.payload_compression",
                                "Unknown MDBX payload compression");
                valid = false;
            }
        } else {
            valid = false;
        }
        if (read_signed(logger, "payload_compression_level", signed_value, diagnostics)) config.payload_compression_level = static_cast<int>(signed_value); else valid = false;
        if (bindings.contains_name("on_error")) {
            std::shared_ptr<std::function<void(const std::string&)> > callback =
                bindings.get<std::function<void(const std::string&)> >("on_error");
            if (!callback) {
                diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                "binding.on_error",
                                "MDBX on_error binding has an incompatible type");
                valid = false;
            } else {
                config.on_error = *callback;
            }
        }
        return valid;
    }
#endif

#ifdef LOGIT_WITH_OTLP
    inline bool read_otlp_format(const ConfigurationMaterializedLogger& logger,
                                 OtlpJsonFormatConfig& config,
                                 ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap object;
        if (!read_object(logger, "format", object, diagnostics)) return false;
        const std::string path = "backend.format";
        bool valid = true;
        valid = object_string(object, "service_name", config.service_name, path, diagnostics) && valid;
        valid = object_string(object, "service_namespace", config.service_namespace, path, diagnostics) && valid;
        valid = object_string(object, "service_instance_id", config.service_instance_id, path, diagnostics) && valid;
        valid = object_string(object, "deployment_environment", config.deployment_environment, path, diagnostics) && valid;
        valid = object_bool(object, "include_source", config.include_source, path, diagnostics) && valid;
        valid = object_bool(object, "include_thread_id", config.include_thread_id, path, diagnostics) && valid;
        valid = object_bool(object, "include_format", config.include_format, path, diagnostics) && valid;
        valid = object_bool(object, "include_arg_names", config.include_arg_names, path, diagnostics) && valid;
        valid = object_bool(object, "include_args", config.include_args, path, diagnostics) && valid;
        valid = object_string(object, "args_prefix", config.args_prefix, path, diagnostics) && valid;
        return valid;
    }

    inline bool read_otlp_compression(const ConfigurationMaterializedLogger& logger,
                                      OtlpCompression& compression,
                                      ConfigurationDiagnostics& diagnostics) {
        std::string value;
        if (!read_string(logger, "compression", value, diagnostics)) return false;
        if (value == "none") compression = OtlpCompression::None;
        else if (value == "gzip") compression = OtlpCompression::Gzip;
        else if (value == "zstd") compression = OtlpCompression::Zstd;
        else {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, "backend.compression", "Unknown OTLP compression");
            return false;
        }
        return true;
    }

    inline bool read_otlp_http_config(const ConfigurationMaterializedLogger& logger,
                                      OtlpHttpLogger::Config& config,
                                      ConfigurationDiagnostics& diagnostics) {
        bool valid = read_otlp_format(logger, config.format, diagnostics);
        std::uint64_t unsigned_value = 0;
        std::int64_t signed_value = 0;
        valid = read_string(logger, "host", config.host, diagnostics) && valid;
        valid = read_string(logger, "path", config.path, diagnostics) && valid;
        if (read_unsigned(logger, "max_queue_size", unsigned_value, diagnostics)) config.max_queue_size = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_unsigned(logger, "max_batch_size", unsigned_value, diagnostics)) config.max_batch_size = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_unsigned(logger, "max_in_flight_requests", unsigned_value, diagnostics)) config.max_in_flight_requests = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_signed(logger, "export_interval_ms", signed_value, diagnostics)) config.export_interval_ms = static_cast<int>(signed_value); else valid = false;
        if (read_signed(logger, "request_timeout_sec", signed_value, diagnostics)) config.request_timeout_sec = static_cast<int>(signed_value); else valid = false;
        if (read_signed(logger, "retry_attempts", signed_value, diagnostics)) config.retry_attempts = static_cast<long>(signed_value); else valid = false;
        if (read_signed(logger, "retry_delay_ms", signed_value, diagnostics)) config.retry_delay_ms = static_cast<long>(signed_value); else valid = false;
        if (read_unsigned(logger, "max_payload_bytes", unsigned_value, diagnostics)) config.max_payload_bytes = static_cast<std::size_t>(unsigned_value); else valid = false;
        valid = read_otlp_compression(logger, config.compression, diagnostics) && valid;
        if (read_signed(logger, "compression_level", signed_value, diagnostics)) config.compression_level = static_cast<int>(signed_value); else valid = false;
        valid = read_bool(logger, "drop_on_overflow", config.drop_on_overflow, diagnostics) && valid;
        valid = read_bool(logger, "async", config.async, diagnostics) && valid;
        valid = read_bool(logger, "cancel_on_shutdown", config.cancel_on_shutdown, diagnostics) && valid;
        return valid;
    }

    inline bool read_otlp_payload_config(const ConfigurationMaterializedLogger& logger,
                                         const ConfigurationBindingSet& bindings,
                                         OtlpPayloadLogger::Config& config,
                                         ConfigurationDiagnostics& diagnostics) {
        bool valid = read_otlp_format(logger, config.format, diagnostics);
        std::uint64_t unsigned_value = 0;
        valid = read_bool(logger, "async", config.async, diagnostics) && valid;
        if (read_unsigned(logger, "max_payload_bytes", unsigned_value, diagnostics)) config.max_payload_bytes = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_unsigned(logger, "max_batch_size", unsigned_value, diagnostics)) config.max_batch_size = static_cast<std::size_t>(unsigned_value); else valid = false;
        if (read_unsigned(logger, "max_queue_size", unsigned_value, diagnostics)) config.max_queue_size = static_cast<std::size_t>(unsigned_value); else valid = false;
        valid = read_bool(logger, "drop_on_overflow", config.drop_on_overflow, diagnostics) && valid;
        if (read_unsigned(logger, "export_interval_ms", unsigned_value, diagnostics)) config.export_interval_ms = static_cast<unsigned>(unsigned_value); else valid = false;
        std::shared_ptr<std::function<void(std::string)>> callback = bindings.get<std::function<void(std::string)>>("on_payload");
        if (!callback) {
            diagnostics.add(ConfigurationErrorCode::MissingProgrammaticBinding,
                            "binding.on_payload",
                            "OTLP payload factory requires on_payload");
            valid = false;
        } else {
            config.on_payload = *callback;
        }
        return valid;
    }
#endif

#if defined(LOGIT_WITH_PROMETHEUS) || defined(LOGIT_WITH_PROMETHEUS_SERVER)
    inline bool read_prometheus_format(const ConfigurationMaterializedLogger& logger,
                                       PrometheusTextFormatConfig& config,
                                       ConfigurationDiagnostics& diagnostics) {
        ConfigurationFieldMap object;
        if (!read_object(logger, "format", object, diagnostics)) return false;
        const std::string path = "backend.format";
        bool valid = true;
        valid = object_bool(object, "include_help", config.include_help, path, diagnostics) && valid;
        valid = object_bool(object, "include_type", config.include_type, path, diagnostics) && valid;
        valid = object_bool(object, "include_timestamp", config.include_timestamp, path, diagnostics) && valid;
        valid = object_string(object, "metric_prefix", config.metric_prefix, path, diagnostics) && valid;
        valid = object_bool(object, "include_logger_label", config.include_logger_label, path, diagnostics) && valid;
        valid = object_string(object, "logger_label_name", config.logger_label_name, path, diagnostics) && valid;
        valid = object_bool(object, "include_instance_label", config.include_instance_label, path, diagnostics) && valid;
        valid = object_string(object, "instance_label_name", config.instance_label_name, path, diagnostics) && valid;
        valid = object_string(object, "instance_label_value", config.instance_label_value, path, diagnostics) && valid;
        valid = object_bool(object, "include_build_info", config.include_build_info, path, diagnostics) && valid;
        ConfigurationFieldMap::const_iterator labels_it = object.find("const_labels");
        if (labels_it == object.end()) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".const_labels", "Missing constant labels array");
            return false;
        }
        std::vector<ConfigurationValue> labels;
        if (!labels_it->second.get_array(labels)) {
            diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".const_labels", "Constant labels must be an array");
            return false;
        }
        for (std::size_t index = 0; index < labels.size(); ++index) {
            ConfigurationFieldMap label;
            if (!labels[index].get_object(label)) { diagnostics.add(ConfigurationErrorCode::InvalidValue, path + ".const_labels[" + std::to_string(index) + "]", "Label must be an object"); valid = false; continue; }
            PrometheusLabel output;
            if (!object_string(label, "name", output.name, path + ".const_labels[" + std::to_string(index) + "]", diagnostics)) valid = false;
            if (!object_string(label, "value", output.value, path + ".const_labels[" + std::to_string(index) + "]", diagnostics)) valid = false;
            config.const_labels.push_back(output);
        }
        return valid;
    }
#endif

} // namespace configuration_runtime_detail

    /// \class BuiltinConfigurationLoggerFactory
    /// \brief Instantiates built-in loggers from a complete materialized plan.
    ///
    /// This factory consumes backend fields exactly as materialized. It does not
    /// parse source documents or introduce a second set of defaults.
    class BuiltinConfigurationLoggerFactory : public ConfigurationLoggerFactory {
    public:
        std::shared_ptr<ILogger> instantiate(
            const ConfigurationMaterializedLogger& logger,
            const ConfigurationBindingSet& bindings,
            ConfigurationDiagnostics& diagnostics) const override {
            try {
                if (logger.type == "file") {
                    FileLogger::Config config;
                    if (!configuration_runtime_detail::read_file_config(logger, config, diagnostics)) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new FileLogger(config));
                }
                if (logger.type == "unique_file") {
                    UniqueFileLogger::Config config;
                    if (!configuration_runtime_detail::read_unique_file_config(logger, config, diagnostics)) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new UniqueFileLogger(config));
                }
                if (logger.type == "memory") {
                    MemoryLogger::Config config;
                    if (!configuration_runtime_detail::read_memory_config(logger, config, diagnostics)) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new MemoryLogger(config));
                }
#ifdef LOGIT_WITH_MDBX
                if (logger.type == "mdbx") {
                    MdbxLogger::Config config;
                    if (!configuration_runtime_detail::read_mdbx_config(logger, bindings, config, diagnostics)) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new MdbxLogger(config));
                }
#endif
#ifdef LOGIT_WITH_OTLP
                if (logger.type == "otlp_http") {
                    OtlpHttpLogger::Config config;
                    if (!configuration_runtime_detail::read_otlp_http_config(logger, config, diagnostics)) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new OtlpHttpLogger(config));
                }
                if (logger.type == "otlp_payload") {
                    OtlpPayloadLogger::Config config;
                    if (!configuration_runtime_detail::read_otlp_payload_config(logger, bindings, config, diagnostics)) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new OtlpPayloadLogger(config));
                }
#endif
#ifdef LOGIT_WITH_PROMETHEUS_SERVER
                if (logger.type == "prometheus_http") {
                    PrometheusHttpServerLogger::Config config;
                    if (!configuration_runtime_detail::read_prometheus_format(logger, config.format, diagnostics)) return std::shared_ptr<ILogger>();
                    std::uint64_t unsigned_value = 0;
                    bool valid = true;
                    valid = configuration_runtime_detail::read_string(logger, "address", config.address, diagnostics) && valid;
                    if (configuration_runtime_detail::read_unsigned(logger, "port", unsigned_value, diagnostics)) config.port = static_cast<unsigned short>(unsigned_value); else valid = false;
                    valid = configuration_runtime_detail::read_string(logger, "path", config.path, diagnostics) && valid;
                    valid = configuration_runtime_detail::read_string(logger, "health_path", config.health_path, diagnostics) && valid;
                    valid = configuration_runtime_detail::read_bool(logger, "enable_health_endpoint", config.enable_health_endpoint, diagnostics) && valid;
                    valid = configuration_runtime_detail::read_bool(logger, "start_immediately", config.start_immediately, diagnostics) && valid;
                    if (!valid) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new PrometheusHttpServerLogger(config));
                }
#endif
#ifdef LOGIT_WITH_PROMETHEUS
                if (logger.type == "prometheus_payload") {
                    PrometheusPayloadLogger::Config config;
                    bool valid = configuration_runtime_detail::read_prometheus_format(logger, config.format, diagnostics);
                    valid = configuration_runtime_detail::read_bool(logger, "emit_on_log", config.emit_on_log, diagnostics) && valid;
                    valid = configuration_runtime_detail::read_bool(logger, "emit_on_wait", config.emit_on_wait, diagnostics) && valid;
                    std::shared_ptr<std::function<void(std::string)>> callback = bindings.get<std::function<void(std::string)>>("on_payload");
                    if (!callback) {
                        diagnostics.add(ConfigurationErrorCode::MissingProgrammaticBinding,
                                        "binding.on_payload",
                                        "Prometheus payload factory requires on_payload");
                        valid = false;
                    } else {
                        config.on_payload = *callback;
                    }
                    if (!valid) return std::shared_ptr<ILogger>();
                    return std::shared_ptr<ILogger>(new PrometheusPayloadLogger(config));
                }
#endif
                diagnostics.add(ConfigurationErrorCode::UnavailableBackend,
                                "type",
                                "Built-in runtime factory does not support this backend yet");
                return std::shared_ptr<ILogger>();
            } catch (const std::exception& error) {
                diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                "type",
                                std::string("Logger construction failed: ") + error.what());
                return std::shared_ptr<ILogger>();
            } catch (...) {
                diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                "type",
                                "Logger construction failed");
                return std::shared_ptr<ILogger>();
            }
        }
    };

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_BUILTINS_HPP_INCLUDED
