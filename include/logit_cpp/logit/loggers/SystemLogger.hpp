#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_SYSTEMLOGGER_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_SYSTEMLOGGER_HPP_INCLUDED

#include "SyslogLogger.hpp"
#include "EventLogLogger.hpp"

/// \file SystemLogger.hpp
/// \brief Defines alias to platform system logger.

namespace logit {
#   if defined(_WIN32)
    /// \brief Windows system logger alias.
    using SystemLogger = EventLogLogger;
#   else
    /// \brief POSIX system logger alias.
    using SystemLogger = SyslogLogger;
#   endif
}

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_SYSTEMLOGGER_HPP_INCLUDED
