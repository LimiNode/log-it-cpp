# ADR 0007: Transparent compressed-file reads

- Status: Accepted
- Date: 2026-09-19

## Context

`FileLogger` and `UniqueFileLogger` already expose rotated `.gz` and `.zst`
artifacts through `list_log_files()`, but their read APIs previously returned
`ok == false` for every compressed file. Callers therefore had to inspect the
suffix and duplicate LogIt++'s optional-dependency handling.

## Decision

`read_log_file()` and `read_log_files()` transparently decompress `.gz` and
`.zst` file contents through the shared compression helpers. Suffixes remain
case-sensitive and match the names produced by the rotation code.

The operation succeeds only when the corresponding build feature is enabled
and the payload is valid. A disabled feature, unsupported suffix, malformed
payload, or file-read failure returns the existing `LogFileReadResult` contract
with `ok == false` and empty content. The readers do not invoke external
commands and do not drain pending asynchronous writes.

## Consequences

Callers use the same file-read API for plain and compressed artifacts, while
builds without zlib or zstd retain fail-closed behavior. Decompression loads the
compressed and decompressed contents into memory, matching the existing
whole-file result type; callers should avoid using this API for unbounded files.

## Alternatives considered

- Returning compressed bytes would make the result meaning depend on the
  suffix and leave dependency handling to every caller.
- Shelling out to gzip or zstd would add platform-specific process and quoting
  behavior to a synchronous read API.
- Adding separate compressed-read methods would duplicate the existing file
  discovery, ownership, and batch-result contracts.

## References

- `include/logit_cpp/logit/loggers/FileLogger.hpp`
- `include/logit_cpp/logit/loggers/UniqueFileLogger.hpp`
- `tests/file_logger_gzip_compression_test.cpp`
- `tests/file_logger_zstd_compression_test.cpp`

