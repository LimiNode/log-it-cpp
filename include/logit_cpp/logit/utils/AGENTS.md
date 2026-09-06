# Utility instructions

Utilities are shared value types and conversions used by multiple backends.

- Keep utilities deterministic, thread-safe by default, and independent of
  logger singletons. Thread-local context is the explicit exception and must
  preserve push/pop nesting and thread isolation.
- Validate lengths, encodings, paths, and numeric ranges at the boundary.
  Malformed input should return the documented error or throw the documented
  exception; never silently truncate semantic data.
- Name constants for protocol bytes, limits, units, and sentinel values. Avoid
  unexplained literals in serialization and parsing code.
- Do not add convenience overloads that depend on transitive includes. Include
  every standard type directly used by a utility header.
- Keep serialization formats stable. A format change requires compatibility
  tests for old data and documentation of the versioning decision.

Prefer one canonical helper over copy-pasted conversions in individual
backends. Update `utils.hpp` when adding a public utility.
