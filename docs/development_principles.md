# Development principles

This document guides implementation choices as the project grows. It is not a
second architecture specification and does not freeze APIs before they exist.

The architecture PDF describes the broad destination. Public headers and tests
remain authoritative for implemented behavior.

## Core rule

Prefer the smallest representation that preserves the required invariants.

- Every allocation and mutable datum has one clear owner.
- Consumers borrow data unless ownership transfer is explicit.
- Derived data is computed when needed until profiling justifies caching it.
- A new struct represents an object with its own state or lifetime, not a group
  of temporary outputs.
- Future metadata is added only when a concrete consumer exists.
- Correctness and module boundaries take priority over optimization.

## Module boundaries

| Module | Owns or defines | Must not own |
| --- | --- | --- |
| `tile` | Fixed tileset, colors, local matching | Search state |
| `permutation` | Signal tokens and adjacent-swap generation | Region geometry |
| `region` | Active cells and boundary constraints | Solver domains or scheduling |
| `yang_zhang` | Reduction-specific construction | Solver state or swap copies |
| `verify` | No persistent state; reads a candidate tiling | Search logic |
| `solver` | Domains, trail, assignments, search state | Reduction semantics |
| `task_plan` | Future OpenMP dependencies | Region or serial solver ownership |

The renderer and JSON layer remain downstream from construction, solving, and
verification. They do not decide correctness.

## Minimal Region direction

`Region` should initially store only source-of-truth geometry:

- bounding width and height;
- whether each dense row-major cell is active;
- boundary color constraints for active cells.

Do not initially store:

- per-cell `x` and `y`, because they are derived from the row-major index;
- cached neighbor indices, because they are derived from geometry;
- `active_count`, unless a measured consumer needs the cache;
- `zone_id` or other OpenMP metadata;
- tile domains, assignments, gadget types, or signal plans.

Neighbor lookup can be introduced as a pure helper when verifier or solver code
needs it. Scheduling metadata belongs to a separate future preprocessing result.

The generic representation may describe disconnected regions or regions with
holes. The Yang-Zhang builder is responsible for producing the required simply
connected instances, and its tests must verify that property.

## Development order

1. Minimal `Region` storage, lifetime, access, and boundary tests.
2. Independent verifier exercised on hand-built regions and tilings.
3. Minimal Cubic Monotone 1-in-3 SAT representation and validation.
4. Formula-to-signal construction and concrete Yang-Zhang rasterization.
5. Correct deterministic serial solver; every witness passes the verifier.
6. Z3 Boolean and tiling cross-checks on small regression instances.
7. OpenMP planning only after the serial solver is stable and profiled.
8. Square-to-hex verification, JSON, and rendering after the square core.

`TaskPlan`, zone ownership, cached compatibility structures, diagnostic IR, and
renderer schemas are deliberately deferred until the preceding module provides a
real use case.

## Definition of done for a module

A module is implemented when it has:

- a small documented public contract, if a public API is needed;
- explicit ownership and failure behavior;
- an implementation included in the build;
- focused unit tests for invariants and invalid input;
- at least one integration test with the preceding implemented module;
- no dependency on unimplemented future layers.

Placeholder files may remain to preserve repository continuity. Their presence
does not imply implementation, and they must not drive premature API design.

## When to add an abstraction

Add an abstraction when it removes duplicated behavior from real consumers,
enforces an otherwise fragile invariant, or owns a resource with a distinct
lifetime. Do not add one solely because it appears in the target architecture.

Performance-oriented state is introduced only with a benchmark or profile that
can show whether the added complexity helps.
