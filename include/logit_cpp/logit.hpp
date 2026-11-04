#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_HPP_INCLUDED

/// \file logit.hpp
/// \brief Unified umbrella header for the LogIt++ library.
///
/// Including this header provides a fully self-contained entry point that
/// aggregates configuration, utilities, formatters, loggers and the logging
/// façade. No additional includes are required to start using the library.

#include "logit/config.hpp"
#include "logit/enums.hpp"
#include "logit/configuration.hpp"
#include "logit/configuration_builtins.hpp"
#include "logit/utils.hpp"
#include "logit/formatter.hpp"
#include "logit/loggers.hpp"
#include "logit/configuration_runtime.hpp"
#include "logit/configuration_runtime_builtins.hpp"
#ifdef LOGIT_WITH_JSON_CONFIG
#include "logit/configuration_json.hpp"
#endif
#include "logit/Logger.hpp"
#include "logit/log_macros.hpp"

/// \namespace logit
/// \brief The primary namespace for the LogIt++ library.
namespace logit {};

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_HPP_INCLUDED
