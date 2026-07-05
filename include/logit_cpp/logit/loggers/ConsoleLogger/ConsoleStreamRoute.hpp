#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_CONSOLELOGGER_CONSOLESTREAMROUTE_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_CONSOLELOGGER_CONSOLESTREAMROUTE_HPP_INCLUDED

/// \file ConsoleStreamRoute.hpp
/// \brief Level-based output stream routing for ConsoleLogger.

#include <ostream>

namespace logit {

    /// \enum ConsoleStreamKind
    /// \brief Identifies a target output stream for ConsoleLogger routes.
    enum class ConsoleStreamKind {
        Cout,   ///< Routes records to std::cout.
        Cerr,   ///< Routes records to std::cerr.
        Custom  ///< Routes records to a caller-provided std::ostream pointer.
    };

    /// \struct ConsoleStreamRoute
    /// \brief Maps a log-level range to a target output stream.
    struct ConsoleStreamRoute {
        LogLevel min_level = LogLevel::LOG_LVL_TRACE; ///< Inclusive lower bound.
        LogLevel max_level = LogLevel::LOG_LVL_FATAL; ///< Inclusive upper bound.
        ConsoleStreamKind kind = ConsoleStreamKind::Cout; ///< Stream identifier.
        std::ostream* custom_stream = nullptr; ///< Used when kind == Custom; null is ignored and the route falls back.

        /// \brief Builds a route that targets std::cout for the given level range.
        static ConsoleStreamRoute to_cout(LogLevel min_level, LogLevel max_level) {
            ConsoleStreamRoute route;
            route.min_level = min_level;
            route.max_level = max_level;
            route.kind = ConsoleStreamKind::Cout;
            return route;
        }

        /// \brief Builds a route that targets std::cerr for the given level range.
        static ConsoleStreamRoute to_cerr(LogLevel min_level, LogLevel max_level) {
            ConsoleStreamRoute route;
            route.min_level = min_level;
            route.max_level = max_level;
            route.kind = ConsoleStreamKind::Cerr;
            return route;
        }

        /// \brief Builds a route that targets a caller-provided std::ostream for the given level range.
        static ConsoleStreamRoute to_stream(
                LogLevel min_level,
                LogLevel max_level,
                std::ostream& stream) {
            ConsoleStreamRoute route;
            route.min_level = min_level;
            route.max_level = max_level;
            route.kind = ConsoleStreamKind::Custom;
            route.custom_stream = &stream;
            return route;
        }
    };

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_CONSOLELOGGER_CONSOLESTREAMROUTE_HPP_INCLUDED
