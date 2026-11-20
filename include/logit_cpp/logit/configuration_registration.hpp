#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_REGISTRATION_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_REGISTRATION_HPP_INCLUDED

/// \file configuration_registration.hpp
/// \brief Append-only registration options and immutable batch index results.

#include "configuration_runtime.hpp"

#include <map>
#include <string>

namespace logit {

    /// \struct ConfigurationRegistrationEntryOptions
    /// \brief Routing flags for one incoming prepared logger ID.
    struct ConfigurationRegistrationEntryOptions {
        bool enabled = true;       ///< Enable dispatch immediately after registration.
        bool single_mode = false;  ///< Route only explicitly targeted records to this entry.
    };

    /// \brief Per-ID overrides; IDs not present in the incoming batch are errors.
    typedef std::map<std::string, ConfigurationRegistrationEntryOptions>
        ConfigurationRegistrationOptions;

    /// \struct ConfigurationRegistrationResult
    /// \brief Stable runtime indices for one successfully appended batch.
    ///
    /// Logger publishes this through a shared_ptr<const ConfigurationRegistrationResult>.
    /// Preparing the complete result before commit makes its handoff non-throwing.
    struct ConfigurationRegistrationResult {
        std::map<std::string, int> indices; ///< Incoming configuration ID to runtime index.
    };

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_CONFIGURATION_REGISTRATION_HPP_INCLUDED
