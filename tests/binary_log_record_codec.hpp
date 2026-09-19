#pragma once
#ifndef LOGIT_CPP_TEST_BINARY_LOG_RECORD_CODEC_HPP_INCLUDED
#define LOGIT_CPP_TEST_BINARY_LOG_RECORD_CODEC_HPP_INCLUDED

#include <logit.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace logit_binary_research {

enum class DecodeError {
    None,
    Truncated,
    BadMagic,
    UnsupportedVersion,
    UnsupportedFlags,
    InvalidFrame,
    InvalidField
};

struct DecodeResult {
    bool ok = false;
    DecodeError error = DecodeError::None;
    std::size_t consumed = 0;
    logit::LogRecordSnapshot record;
};

struct StreamResult {
    bool ok = false;
    DecodeError error = DecodeError::None;
    std::size_t error_offset = 0;
    std::vector<logit::LogRecordSnapshot> records;
};

namespace detail {

static const std::size_t kHeaderSize = 12;
static const std::size_t kMaxFrameSize = 64u * 1024u * 1024u;
static const std::size_t kMaxFieldSize = 16u * 1024u * 1024u;

inline void append_u8(std::vector<uint8_t>& out, uint8_t value) {
    out.push_back(value);
}

inline void append_u16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
}

inline void append_u32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
    }
}

inline void append_u64(std::vector<uint8_t>& out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
    }
}

inline void append_string(std::vector<uint8_t>& out, const std::string& value) {
    if (value.size() > kMaxFieldSize ||
        value.size() > static_cast<std::size_t>((std::numeric_limits<uint32_t>::max)())) {
        throw std::length_error("binary log record field is too large");
    }
    append_u32(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

class Cursor {
public:
    Cursor(const uint8_t* data, std::size_t size) : m_data(data), m_size(size) {}

    bool read_u8(uint8_t& value) {
        if (!take(1)) return false;
        value = m_data[m_offset++];
        return true;
    }

    bool read_u32(uint32_t& value) {
        if (!take(4)) return false;
        value = 0;
        for (int i = 0; i < 4; ++i) {
            value = (value << 8) | static_cast<uint32_t>(m_data[m_offset++]);
        }
        return true;
    }

    bool read_u64(uint64_t& value) {
        if (!take(8)) return false;
        value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | static_cast<uint64_t>(m_data[m_offset++]);
        }
        return true;
    }

    bool read_string(std::string& value) {
        uint32_t size = 0;
        if (!read_u32(size) || size > kMaxFieldSize || !take(size)) return false;
        value.assign(reinterpret_cast<const char*>(m_data + m_offset), size);
        m_offset += size;
        return true;
    }

    std::size_t remaining() const { return m_size - m_offset; }

private:
    const uint8_t* m_data;
    std::size_t m_size;
    std::size_t m_offset = 0;

    bool take(std::size_t size) const {
        return size <= m_size - m_offset;
    }
};

inline DecodeResult decode_payload(const uint8_t* data, std::size_t size) {
    DecodeResult result;
    Cursor cursor(data, size);
    uint64_t session_id = 0;
    uint64_t timestamp_bits = 0;
    uint32_t sequence = 0;
    uint8_t level = 0;
    uint8_t reserved[3] = {0, 0, 0};
    uint64_t payload_id = 0;
    uint32_t line_bits = 0;
    if (!cursor.read_u64(session_id) ||
        !cursor.read_u64(timestamp_bits) ||
        !cursor.read_u32(sequence) ||
        !cursor.read_u8(level) ||
        !cursor.read_u8(reserved[0]) ||
        !cursor.read_u8(reserved[1]) ||
        !cursor.read_u8(reserved[2]) ||
        !cursor.read_u64(payload_id) ||
        !cursor.read_u32(line_bits)) {
        result.error = DecodeError::Truncated;
        return result;
    }
    if (level >= 6 || reserved[0] != 0 || reserved[1] != 0 || reserved[2] != 0) {
        result.error = DecodeError::InvalidField;
        return result;
    }
    if (!cursor.read_string(result.record.message) ||
        !cursor.read_string(result.record.file) ||
        !cursor.read_string(result.record.function) ||
        cursor.remaining() != 0) {
        result.error = DecodeError::InvalidField;
        return result;
    }

    result.record.session_id = session_id;
    result.record.timestamp_ms = static_cast<int64_t>(timestamp_bits);
    result.record.sequence = sequence;
    result.record.level = static_cast<logit::LogLevel>(level);
    result.record.payload_id = payload_id;
    result.record.line = static_cast<int32_t>(line_bits);
    result.ok = true;
    result.error = DecodeError::None;
    return result;
}

} // namespace detail

inline std::vector<uint8_t> encode(const logit::LogRecordSnapshot& record) {
    std::vector<uint8_t> payload;
    payload.reserve(64 + record.message.size() + record.file.size() + record.function.size());
    detail::append_u64(payload, record.session_id);
    detail::append_u64(payload, static_cast<uint64_t>(record.timestamp_ms));
    detail::append_u32(payload, record.sequence);
    detail::append_u8(payload, static_cast<uint8_t>(record.level));
    detail::append_u8(payload, 0);
    detail::append_u8(payload, 0);
    detail::append_u8(payload, 0);
    detail::append_u64(payload, record.payload_id);
    detail::append_u32(payload, static_cast<uint32_t>(record.line));
    detail::append_string(payload, record.message);
    detail::append_string(payload, record.file);
    detail::append_string(payload, record.function);
    if (payload.size() > detail::kMaxFrameSize ||
        payload.size() > static_cast<std::size_t>((std::numeric_limits<uint32_t>::max)())) {
        throw std::length_error("binary log record frame is too large");
    }

    std::vector<uint8_t> frame;
    frame.reserve(detail::kHeaderSize + payload.size());
    frame.push_back('L');
    frame.push_back('G');
    frame.push_back('B');
    frame.push_back('R');
    detail::append_u8(frame, 1);
    detail::append_u8(frame, 0);
    detail::append_u16(frame, 0);
    detail::append_u32(frame, static_cast<uint32_t>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

inline DecodeResult decode_one(const uint8_t* data, std::size_t size) {
    DecodeResult result;
    if (data == nullptr && size != 0) {
        result.error = DecodeError::Truncated;
        return result;
    }
    if (size < detail::kHeaderSize) {
        result.error = DecodeError::Truncated;
        return result;
    }
    if (data[0] != 'L' || data[1] != 'G' || data[2] != 'B' || data[3] != 'R') {
        result.error = DecodeError::BadMagic;
        return result;
    }
    if (data[4] != 1) {
        result.error = DecodeError::UnsupportedVersion;
        return result;
    }
    if (data[5] != 0 || data[6] != 0 || data[7] != 0) {
        result.error = DecodeError::UnsupportedFlags;
        return result;
    }
    const uint32_t frame_size =
        (static_cast<uint32_t>(data[8]) << 24) |
        (static_cast<uint32_t>(data[9]) << 16) |
        (static_cast<uint32_t>(data[10]) << 8) |
        static_cast<uint32_t>(data[11]);
    if (frame_size == 0 || frame_size > detail::kMaxFrameSize) {
        result.error = DecodeError::InvalidFrame;
        return result;
    }
    if (static_cast<std::size_t>(frame_size) > size - detail::kHeaderSize) {
        result.error = DecodeError::Truncated;
        return result;
    }

    result = detail::decode_payload(data + detail::kHeaderSize, frame_size);
    if (result.ok) result.consumed = detail::kHeaderSize + frame_size;
    return result;
}

inline StreamResult decode_stream(const std::vector<uint8_t>& bytes) {
    StreamResult result;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DecodeResult one = decode_one(bytes.data() + offset, bytes.size() - offset);
        if (!one.ok) {
            result.error = one.error;
            result.error_offset = offset;
            return result;
        }
        result.records.push_back(one.record);
        offset += one.consumed;
    }
    result.ok = true;
    result.error = DecodeError::None;
    return result;
}

} // namespace logit_binary_research

#endif // LOGIT_CPP_TEST_BINARY_LOG_RECORD_CODEC_HPP_INCLUDED
