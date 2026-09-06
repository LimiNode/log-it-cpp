# Test Instructions

Tests define the supported behavior of the header-only library.

- Prefer deterministic assertions and condition variables over sleeps or
  scheduler timing. If timing is the behavior under test, use a generous
  bound and document the reason.
- Register every new test in `tests/CMakeLists.txt`; keep test names stable so
  CI and downstream users can filter them.
- Exercise both feature-enabled and feature-disabled paths when a change
  touches a compile-time option. Include-only tests must include only the
  public header named by the test.
- Do not require Unix tools (`rm`, `gzip`, `bash`) in tests that run on
  Windows. Resolve tools through CMake or skip the external-command case with
  a clear reason.
- Avoid global state leakage between tests. Shut down executors and loggers
  before their backing objects leave scope.

Run the focused executable first, then `ctest --output-on-failure`; report
environment-only failures separately from code failures.
