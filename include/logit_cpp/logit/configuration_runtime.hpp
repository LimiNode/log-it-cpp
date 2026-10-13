#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_HPP_INCLUDED

/// \file configuration_runtime.hpp
/// \brief Binding and runtime-instantiation contract for materialized plans.

#include "configuration.hpp"
#include "loggers.hpp"

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

    /// \brief Resolve bindings and publish a complete logger set atomically.
    ///
    /// The caller-visible logger vector is updated only after every binding has
    /// resolved and every factory call has returned a logger. Factories may
    /// still perform external side effects while constructing instances; those
    /// effects are not transactional and are not rolled back if a later factory
    /// fails.
    inline bool instantiate_configuration(
        const ConfigurationPlan& plan,
        const ConfigurationBindingResolver& resolver,
        const ConfigurationLoggerFactory& factory,
        std::vector<std::shared_ptr<ILogger>>& loggers,
        ConfigurationDiagnostics& diagnostics) {
        diagnostics = ConfigurationDiagnostics();
        std::vector<ConfigurationBindingSet> resolved_bindings;
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

        if (!bindings_ok) return false;

        std::vector<std::shared_ptr<ILogger>> candidate;
        candidate.reserve(plan.loggers.size());
        for (std::size_t index = 0; index < plan.loggers.size(); ++index) {
            const ConfigurationMaterializedLogger& entry = plan.loggers[index];
            const std::string prefix = "loggers[" + std::to_string(index) + "]";

            ConfigurationDiagnostics factory_diagnostics;
            std::shared_ptr<ILogger> runtime_logger =
                factory.instantiate(entry, resolved_bindings[index], factory_diagnostics);
            if (!runtime_logger || !factory_diagnostics.empty()) {
                if (factory_diagnostics.empty()) {
                    diagnostics.add(ConfigurationErrorCode::InstantiationFailed,
                                    prefix,
                                    "Logger factory could not instantiate the plan entry");
                } else {
                    diagnostics.append_rebased(factory_diagnostics, prefix);
                }
                return false;
            }
            candidate.push_back(runtime_logger);
        }

        loggers = candidate;
        return true;
    }

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_RUNTIME_HPP_INCLUDED
