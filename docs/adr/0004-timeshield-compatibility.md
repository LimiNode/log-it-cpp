# ADR 0004: TimeShield compatibility and dependency reuse

- Status: Accepted
- Date: 2026-09-14

## Context

The current TimeShield release is `v2.0.0`, whose CMake package uses
`SameMinorVersion` compatibility. Consumers may provide dependency targets
from a parent project or sibling build instead of installing another package
configuration.

## Decision

Pin the bundled submodule to the `v2.0.0` release and request
`TimeShield 2.0.0`, which expresses the supported `2.0.x` line. Before package
discovery, reuse an existing `time_shield::time_shield` target; installed
dependency discovery follows the same target-reuse rule for optional packages.

## Consequences

Build-tree and installed-package consumers remain composable, while the
minimum supported TimeShield API is explicit. Moving to another minor line or
changing the compatibility policy requires a new compatibility review and
release note.
