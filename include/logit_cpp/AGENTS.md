# Public Header Instructions

This tree is the installed, header-only API. Keep changes source-compatible
with the documented C++11 baseline; code behind MDBX, OTLP, and Prometheus
server feature macros may use C++17 only when CMake selects that standard.

For subsystem-specific work, also read the nearest guide:

- [`logit/detail/AGENTS.md`](logit/detail/AGENTS.md) - queue, executor, and
  signal-safety invariants.
- [`logit/loggers/AGENTS.md`](logit/loggers/AGENTS.md) - backend lifecycle and
  callback ownership.
- [`logit/formatter/AGENTS.md`](logit/formatter/AGENTS.md) - token and
  timestamp semantics.
- [`logit/utils/AGENTS.md`](logit/utils/AGENTS.md) - validation and stable
  serialization helpers.

- Include the nearest umbrella (`logit.hpp`, `utils.hpp`, `formatter.hpp`, or
  `loggers.hpp`) in examples and integration tests.
- Preserve the existing public names, overloads, macro expansion contracts,
  and feature guards. Add new API only with a focused test and documentation.
- Keep headers self-contained: include every standard type used directly and
  do not depend on include order or transitive headers.
- A `noexcept` declaration is a contract. Do not perform allocation, invoke a
  user callback, or execute code that may throw in a `noexcept` function.
  Signal/crash handlers are the explicit exception and must stay async-signal
  safe.
- For queue/executor code, preserve shutdown, ordering, and callback
  synchronization invariants described in `guides/concurrency.md`.
- Keep detail headers private and free of includes from public `logit/...`
  paths; cross-module includes go through the nearest umbrella.

When changing an inline implementation, inspect all call sites with
Codebase Memory and add a regression test before simplifying duplicated code.
