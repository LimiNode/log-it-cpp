# ADR 0008: Binary log record format research prototype

- Status: Accepted
- Date: 2026-09-19

## Context

Binary logging is a possible future backend, but committing to a wire format
before its compatibility and corruption behavior are explicit would make later
readers difficult to evolve. The existing MDBX value format is storage-specific
and is not a suitable public file contract.

## Decision

The research prototype uses a framed, big-endian record format with the magic
`LGBR`, a one-byte format version, one-byte flags, reserved bytes, and a
32-bit payload length. Version 1 stores the stable `LogRecordSnapshot` fields:
session id, timestamp, sequence, level, payload id, source line, and three
length-prefixed UTF-8 strings for message, file, and function.

The prototype reader is deliberately outside the installed library API. It
rejects unknown versions and flags, malformed or truncated frames, invalid log
levels, oversized fields, and trailing payload bytes. A stream is decoded one
length-delimited frame at a time, so a future reader can skip or recover around
individual records without relying on native struct layout.

This is a format and reader experiment only. No production backend, file
extension, persistence policy, compression, or compatibility promise is added
until the format is reviewed against real workloads and migration requirements.

## Consequences

The prototype provides deterministic bytes and explicit failure modes that can
be reviewed and fuzzed independently of a backend. Big-endian fields and
length-prefixed strings avoid ABI and host-endian coupling. The current
whole-record decoder has bounded allocations and keeps all decoded strings
owning.

The format carries no schema negotiation beyond its version and flags, and it
does not yet encode MDC/NDC context, arbitrary attributes, or formatter state.
Those omissions are intentional until the production record contract is chosen.

## Alternatives considered

- Reusing the MDBX value serializer would couple a file format to one storage
  backend and its migration history.
- Dumping native C++ structs would expose padding, endianness, enum width, and
  ABI details.
- Adopting an external serialization library before defining the record contract
  would add dependency and schema commitments prematurely.

## References

- `tests/binary_log_record_codec.hpp`
- `tests/binary_log_record_codec_test.cpp`
- `docs/future-plans.md`

