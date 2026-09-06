# Guide Maintenance Instructions

Guides are operational instructions for maintainers and coding agents.

- Keep rules concrete, scoped, and testable. Put module-specific rules in the
  nearest `AGENTS.md` instead of expanding the root file.
- Update command examples when CMake options, dependency versions, or CI
  behavior changes. Verify commands against the repository's minimum CMake
  and C++ standards.
- Avoid duplicating API documentation from `README.md`; link to the canonical
  source and focus on workflow, invariants, and failure handling.
- Record known platform limitations and expected environment-only failures so
  future reviews do not mistake them for regressions.
