#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_DETAIL_COMPRESSIONUTILS_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_DETAIL_COMPRESSIONUTILS_HPP_INCLUDED

/// \file CompressionUtils.hpp
/// \brief Shared gzip/zstd compression helpers used by OTLP and MDBX backends.

#include <cstddef>
#include <string>

#if defined(LOGIT_HAS_ZLIB)
#   include <zlib.h>
#endif

#if defined(LOGIT_HAS_ZSTD)
#   include <zstd.h>
#endif

namespace logit {
namespace detail {

/// \brief Compress a string with gzip.
/// \param input Uncompressed data.
/// \param[out] output Compressed result (valid only on success).
/// \param level Compression level 1-9.
/// \return true on success, false if zlib is unavailable or compression fails.
/// \note Callers must check the return value and handle fallback explicitly.
inline bool compress_string_gzip(const std::string& input, std::string& output, int level) {
#if defined(LOGIT_HAS_ZLIB)
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());
    zs.next_out = Z_NULL;
    zs.avail_out = 0;

    if (level < 1) level = 1;
    if (level > 9) level = 9;

    int window_bits = 15 + 16;
    if (deflateInit2(&zs, level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    output.clear();

    int ret = Z_OK;
    std::size_t offset = 0;
    const std::size_t buf_size = 32768;

    do {
        output.resize(offset + buf_size);
        zs.next_out = reinterpret_cast<Bytef*>(&output[offset]);
        zs.avail_out = static_cast<uInt>(buf_size);

        ret = deflate(&zs, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            return false;
        }

        offset = zs.total_out;
    } while (ret != Z_STREAM_END);

    output.resize(zs.total_out);
    deflateEnd(&zs);
    return true;
#else
    (void)input; (void)output; (void)level;
    return false;
#endif
}

/// \brief Decompress gzip-compressed bytes.
/// \param input Compressed data.
/// \param[out] output Decompressed result (valid only on success).
/// \return true on success, false if zlib is unavailable or decompression fails.
inline bool decompress_string_gzip(const std::string& input, std::string& output) {
    output.clear();
#if defined(LOGIT_HAS_ZLIB)
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());
    zs.next_out = Z_NULL;
    zs.avail_out = 0;

    int window_bits = 15 + 32; // automatic header detection (gzip/zlib)
    if (inflateInit2(&zs, window_bits) != Z_OK) {
        return false;
    }

    int ret = Z_OK;
    std::size_t offset = 0;
    const std::size_t buf_size = 32768;

    do {
        output.resize(offset + buf_size);
        zs.next_out = reinterpret_cast<Bytef*>(&output[offset]);
        zs.avail_out = static_cast<uInt>(buf_size);

        const uInt previous_avail_in = zs.avail_in;
        const uLong previous_total_out = zs.total_out;
        ret = inflate(&zs, Z_NO_FLUSH);

        // Only these two results are valid for the loop below.  In particular,
        // Z_BUF_ERROR means that inflate made no progress (typically because a
        // compressed stream was truncated after consuming all input).
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            output.clear();
            return false;
        }

        offset = zs.total_out;

        if (ret == Z_OK &&
            zs.avail_in == previous_avail_in &&
            zs.total_out == previous_total_out) {
            inflateEnd(&zs);
            output.clear();
            return false;
        }
    } while (ret != Z_STREAM_END);

    output.resize(zs.total_out);
    inflateEnd(&zs);
    return true;
#else
    (void)input; (void)output;
    return false;
#endif
}

/// \brief Compress a string with zstd.
/// \param input Uncompressed data.
/// \param[out] output Compressed result (valid only on success).
/// \param level Compression level 1-19.
/// \return true on success, false if zstd is unavailable or compression fails.
/// \note Callers must check the return value and handle fallback explicitly.
inline bool compress_string_zstd(const std::string& input, std::string& output, int level) {
#if defined(LOGIT_HAS_ZSTD)
    if (level < 1) level = 1;
    if (level > 19) level = 19;

    std::size_t bound = ZSTD_compressBound(input.size());
    output.resize(bound);

    std::size_t result = ZSTD_compress(
        &output[0], output.size(),
        input.data(), input.size(),
        level);

    if (ZSTD_isError(result)) {
        output.clear();
        return false;
    }

    output.resize(result);
    return true;
#else
    (void)input; (void)output; (void)level;
    return false;
#endif
}

/// \brief Decompress zstd-compressed bytes.
/// \param input Compressed data.
/// \param[out] output Decompressed result (valid only on success).
/// \return true on success, false if zstd is unavailable or decompression fails.
inline bool decompress_string_zstd(const std::string& input, std::string& output) {
    output.clear();
#if defined(LOGIT_HAS_ZSTD)
    std::size_t const d_size = ZSTD_getFrameContentSize(input.data(), input.size());
    if (d_size == ZSTD_CONTENTSIZE_ERROR || d_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return false;
    }

    output.resize(d_size);
    std::size_t const result = ZSTD_decompress(
        &output[0], output.size(),
        input.data(), input.size());

    if (ZSTD_isError(result)) {
        output.clear();
        return false;
    }

    output.resize(result);
    return true;
#else
    (void)input; (void)output;
    return false;
#endif
}

/// \brief Decompress a file payload based on its compressed suffix.
/// \param filename File name ending in `.gz` or `.zst`.
/// \param input Compressed file bytes.
/// \param[out] output Decompressed result (valid only on success).
/// \return true when the suffix is supported and decompression succeeds.
inline bool decompress_string_by_suffix(
        const std::string& filename,
        const std::string& input,
        std::string& output) {
    if (filename.size() >= 3 &&
        filename.compare(filename.size() - 3, 3, ".gz") == 0) {
        return decompress_string_gzip(input, output);
    }
    if (filename.size() >= 4 &&
        filename.compare(filename.size() - 4, 4, ".zst") == 0) {
        return decompress_string_zstd(input, output);
    }
    output.clear();
    return false;
}

} // namespace detail
} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_DETAIL_COMPRESSIONUTILS_HPP_INCLUDED
