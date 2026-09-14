#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_OTLP_OTLPCOMPRESSIONTYPE_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_OTLP_OTLPCOMPRESSIONTYPE_HPP_INCLUDED

/// \file OtlpCompressionType.hpp
/// \brief Compression algorithm selected for OTLP/HTTP payloads.

namespace logit {

/// \enum OtlpCompression
/// \brief Compression algorithm used for an OTLP/HTTP request payload.
enum class OtlpCompression {
    None,
    Gzip,
    Zstd
};

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_LOGGERS_OTLP_OTLPCOMPRESSIONTYPE_HPP_INCLUDED
