# Detail-layer invariants

This directory contains implementation building blocks shared by the public
headers. It is not a second public API.

- Keep detail headers independent from `logit/...` public headers. Public
  cross-module dependencies go through the nearest umbrella header.
- `MpscRingAny` is a bounded MPSC queue with one consumer. Producers may claim
  a cell only when they can publish it without throwing; a claimed cell must
  always become visible or the queue can stall permanently.
- Queue resize and replacement happen only under the executor lifecycle lock.
  Stop accepting producers before moving or destroying a queue, then join the
  worker before releasing backing storage.
- Preserve release/acquire publication ordering and the sequence-number
  protocol. Do not replace atomic operations with plain loads/stores to make a
  test pass.
- `noexcept` in this layer is a real contract. Check allocation, construction,
  move, destruction, and callback behavior before adding it. Prefer a
  compile-time trait or an explicit precondition over silently terminating.
- Keep signal/crash-safe helpers free of allocation, locks, formatting, and
  non-async-signal-safe calls.

Use `guides/concurrency.md` for the full shutdown and callback ordering
contract. Add a focused stress or lifecycle test when changing these rules.
