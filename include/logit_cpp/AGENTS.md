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
- Treat `<logit.hpp>` as the supported application entry point. Do not infer a
  standalone contract for `log_macros.hpp` or other aggregate-owned leaf
  headers unless a focused public-header test documents it. Public aliases are
  normally consumed through `<logit.hpp>` or the relevant module umbrella.
- Preserve the existing public names, overloads, macro expansion contracts,
  and feature guards. Add new API only with a focused test and documentation.
- Keep the supported public umbrellas (`logit.hpp`, `utils.hpp`,
  `formatter.hpp`, `loggers.hpp`) self-contained.
- Leaf headers follow the aggregate-first/NHR contract and may rely on
  prerequisites prepared by their nearest umbrella. Do not add standalone
  include contracts to leaf headers unless explicitly required by the public
  API and covered by a focused test.
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
