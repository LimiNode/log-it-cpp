#include "binary_log_record_codec.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

logit::LogRecordSnapshot make_record() {
    logit::LogRecordSnapshot record;
    record.session_id = 42;
    record.timestamp_ms = -123456789;
    record.sequence = 7;
    record.level = logit::LogLevel::LOG_LVL_WARN;
    record.message = "binary message";
    record.payload_id = 99;
    record.file = "source.cpp";
    record.function = "emit";
    record.line = 314;
    return record;
}

void assert_same(const logit::LogRecordSnapshot& lhs, const logit::LogRecordSnapshot& rhs) {
    assert(lhs.session_id == rhs.session_id);
    assert(lhs.timestamp_ms == rhs.timestamp_ms);
    assert(lhs.sequence == rhs.sequence);
    assert(lhs.level == rhs.level);
    assert(lhs.message == rhs.message);
    assert(lhs.payload_id == rhs.payload_id);
    assert(lhs.file == rhs.file);
    assert(lhs.function == rhs.function);
    assert(lhs.line == rhs.line);
}

} // namespace

int main() {
    const logit::LogRecordSnapshot first = make_record();
    const std::vector<uint8_t> first_bytes = logit_binary_research::encode(first);
    const logit_binary_research::DecodeResult decoded =
        logit_binary_research::decode_one(first_bytes.data(), first_bytes.size());
    assert(decoded.ok);
    assert(decoded.consumed == first_bytes.size());
    assert_same(first, decoded.record);

    logit::LogRecordSnapshot second = first;
    second.sequence = 8;
    second.message = "second binary message";
    const std::vector<uint8_t> second_bytes = logit_binary_research::encode(second);
    std::vector<uint8_t> stream = first_bytes;
    stream.insert(stream.end(), second_bytes.begin(), second_bytes.end());
    const logit_binary_research::StreamResult decoded_stream =
        logit_binary_research::decode_stream(stream);
    assert(decoded_stream.ok);
    assert(decoded_stream.records.size() == 2);
    assert_same(first, decoded_stream.records[0]);
    assert_same(second, decoded_stream.records[1]);

    std::vector<uint8_t> truncated(first_bytes.begin(), first_bytes.end() - 1);
    assert(logit_binary_research::decode_one(truncated.data(), truncated.size()).error ==
           logit_binary_research::DecodeError::Truncated);

    std::vector<uint8_t> bad_magic = first_bytes;
    bad_magic[0] = 'X';
    assert(logit_binary_research::decode_one(bad_magic.data(), bad_magic.size()).error ==
           logit_binary_research::DecodeError::BadMagic);

    std::vector<uint8_t> future_version = first_bytes;
    future_version[4] = 2;
    assert(logit_binary_research::decode_one(future_version.data(), future_version.size()).error ==
           logit_binary_research::DecodeError::UnsupportedVersion);

    std::vector<uint8_t> unsupported_flags = first_bytes;
    unsupported_flags[5] = 1;
    assert(logit_binary_research::decode_one(unsupported_flags.data(), unsupported_flags.size()).error ==
           logit_binary_research::DecodeError::UnsupportedFlags);

    std::vector<uint8_t> oversized_frame = first_bytes;
    oversized_frame[8] = 0xFF;
    oversized_frame[9] = 0xFF;
    oversized_frame[10] = 0xFF;
    oversized_frame[11] = 0xFF;
    assert(logit_binary_research::decode_one(oversized_frame.data(), oversized_frame.size()).error ==
           logit_binary_research::DecodeError::InvalidFrame);

    stream.push_back(0);
    const logit_binary_research::StreamResult trailing =
        logit_binary_research::decode_stream(stream);
    assert(!trailing.ok);
    assert(trailing.error == logit_binary_research::DecodeError::Truncated);
    assert(trailing.error_offset == first_bytes.size() + second_bytes.size());
    return 0;
}
