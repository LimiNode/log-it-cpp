# Formatter instructions

Formatters convert immutable `LogRecord` values to text or payload fragments.

- Keep formatting side-effect free: do not mutate the record, logger state, or
  global configuration while formatting.
- Preserve pattern-token semantics, escaping rules, timestamp units, timezone
  behavior, and raw-mode passthrough. Add a focused test for every new token
  or changed edge case.
- Use TimeShield conversion helpers for date/time calculations; do not create a
  second calendar or timezone implementation in this module.
- Avoid allocations in fast paths when the existing API permits it, but do not
  trade correctness for micro-optimizations or introduce hidden static state.
- Keep formatter interfaces compatible with C++11 unless the enclosing feature
  explicitly requires C++17.

Document unsupported tokens and fallback behavior in the formatter guide and
keep compiler/parser helpers private to this subtree.
