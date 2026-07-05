#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_CRASHLOGGER_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_CRASHLOGGER_HPP_INCLUDED

/// \\file CrashLogger.hpp
/// \\brief Platform-specific crash logger alias.

#include "CrashPosixLogger.hpp"
#include "CrashWindowsLogger.hpp"

namespace logit {

#if defined(_WIN32)
    using CrashLogger = CrashWindowsLogger;
#else
    using CrashLogger = CrashPosixLogger;
#endif

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_CRASHLOGGER_HPP_INCLUDED
