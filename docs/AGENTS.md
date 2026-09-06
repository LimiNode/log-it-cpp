# Documentation Instructions

Documentation is part of the public API contract.

- Update the relevant English and Russian document together when behavior,
  options, examples, or compatibility changes.
- Keep snippets buildable with the stated CMake options and include paths.
  Prefer linking to one canonical explanation over duplicating long blocks.
- Distinguish implemented behavior from future plans and platform limitations;
  do not promise optional backends that are not tested in the documented flow.
- Do not commit generated Doxygen output or local build artifacts.
