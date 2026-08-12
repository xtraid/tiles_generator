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
| `yang_zhang` | Reduction-specific construction and the transferred swap trace | Solver state or duplicated permutation data |
| `verify` | No persistent state; reads a candidate tiling | Search logic |
| `solver` | Domains, trail, assignments, search state | Reduction semantics |
| `task_plan` | Future OpenMP dependencies | Region or serial solver ownership |
| `python/model` | Pure immutable Python data contracts | I/O, ctypes, Z3, or native ownership |
| `python/native` | C ABI adaptation, native lifetimes, complete copy-out | Solver logic or leaked native pointers |
| `python/oracles` | Independent solver or verifier logic over pure models | Parsing, filesystem I/O, or reduction construction |

The renderer and JSON layer remain downstream from construction, solving, and
verification. They do not decide correctness.

## Python ownership and oracle boundaries

The current Python layer contains an immutable `Formula` model and an
independent Boolean witness checker. `native/formula.py`, the Boolean Z3 solver,
and the Wang Z3 solver are scaffolds. There is currently no Python `Region`
model, native region adapter, shared-library loader, or implemented Z3 encoding.

Dependencies flow in one direction:

```text
C formula/parser
      |
      v
   libwang
      |
      v
native/formula.py
      |
      v
model/formula.py
      |
      +--------------------+
      |                    |
      v                    v
boolean_solver.py    witness_check.py
```

Forbidden dependencies are equally explicit:

```text
boolean_solver.py  -X-> native/formula.py, ctypes, filesystem
witness_check.py   -X-> Z3, ctypes, C ABI
model/formula.py   -X-> ctypes, CDLL, filesystem, Z3
```

The native adapter is an ownership boundary. It must copy the complete C
formula, including all three ordered positions of every clause, into Python
storage and release the C allocation in a `finally` block. No ctypes pointer
may escape. The current C API only exposes `cm13_formula_parse(FILE *, ...)`,
so the scaffold does not bind a fragile `FILE *`. A small, non-Python-specific
future API such as `cm13_formula_load_path(...)` may provide a robust external
entry point, but it is not implemented.

Do not marshal `Formula` back into `Cm13Formula`. When one future consumer needs
both formula and region, parse once and branch while the native formula is
alive:

```text
                          .cm13
                            |
                            v
                       C parser
                            |
                       Cm13Formula C
                       /           \
                      /             \
                     v               v
             copy Formula Py    Yang-Zhang builder
                     |               |
                     v               v
             Boolean Z3           Region C
                     |               |
                     v               +------------------+
                assignment                              |
                     |                                  |
                     v                                  v
             witness checker                      native Wang solver
                                                        |
                                      Region copy ------+
                                          |
                                          v
                                    Wang Z3 oracle
```

The coordinating native code must use `finally` cleanup to destroy the C
region and then the C formula after the required copies and native operations
finish. Both returned Python models are fully Python-owned.

The future `native/region.py` will copy `Region C` into a pure Python region;
only once that second native consumer exists should `native/_lib.py`
centralize loading of a future shared `libwang.so`. A future
`native/reduction.py` may coordinate the single native formula lifetime shown
above, but it must not be created before a real consumer needs it.

There are two distinct planned oracles:

- Boolean Z3: `Formula -> Boolean constraints -> SAT/UNSAT/UNKNOWN` and an
  assignment only for SAT. It performs no parsing, I/O, ctypes work, region
  construction, or reduction.
- Wang Z3: `Region + canonical TILESET -> tiling constraints ->
  SAT/UNSAT/UNKNOWN` and a tiling only for SAT. It receives the same concrete
  region as the native solver; it must not rebuild Yang-Zhang independently.

The Boolean witness checker remains pure Python and counts clause positions,
not unique variables: `(x, x, y)` counts `x` twice. Verifiers never depend on
the solver they check. Reverse marshalling, a common `_lib.py`, and new model
layers remain forbidden until concrete consumers justify them.

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

1. Minimal `Region` storage, lifetime, access, and boundary tests (complete).
2. Canonical Cubic Monotone 1-in-3 SAT representation, strict text parser,
   validation, and formula-to-region Yang-Zhang builder (complete).
3. Independent verifier exercised on hand-built regions and tilings
   (complete).
4. Correct deterministic serial solver; every witness passes the verifier
   (complete).
5. Solver-level regression tests for the explicit forwarder bands, atomic
   anchor/crossover gadgets, whole crossover blocks, composed chains,
   deterministic fuzz cases, and large volumes (complete).
6. Z3 Boolean and tiling cross-checks on small regression instances.
7. OpenMP planning only after the serial solver is stable and profiled.
8. Square-to-hex verification, JSON, and rendering after the square core.

Private compatibility masks now have a concrete consumer in the serial solver
and remain derived from `TILESET`. `TaskPlan`, zone ownership, diagnostic IR,
and renderer schemas remain deferred until a preceding module provides a real
use case.

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
