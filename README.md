# Hexagonal Wang Tiles

Research and implementation project on finite Wang tiling, centered on the
fixed 23-tile NP-completeness construction of Yang–Zhang and on a verified
translation from square Wang tilings to a hexagonal representation.

The project is being rebuilt from the theory outward. The previous experimental
codebase remains under `legacy/`, but it is not the implementation base of the
new core.

## Goal

The intended end-to-end pipeline is:

```text
Cubic Monotone 1-in-3 SAT formula
        |
        v
Yang-Zhang finite Wang region
        |
        +-------------------+
        |                   |
        v                   v
native C solver       Z3 reference paths
        |                   |
        +---------+---------+
                  |
                  v
       independent verifier
                  |
                  v
      square-to-hex translation
                  |
                  v
       hex verifier and renderer
```

The native solver, Z3 models, verifier, and renderer are deliberately separate.
The renderer is downstream from correctness-critical logic and never decides
tileability.

## Correctness boundaries

- The solver uses the 23 atomic Wang tiles, with translation only: no rotation
  or reflection.
- The 14 generalized tiles are builder and diagnostic metadata, not solver
  primitives.
- The region depends on the input formula and is built at runtime.
- Search and verification remain independent implementations.
- Z3 is an oracle and cross-check, not a replacement for the native C solver.
- OpenMP is introduced only after the serial path is correct and measurable.
- Project conventions must be distinguished from claims inherited from the
  Yang–Zhang paper.

## Current status

Implemented and tested as of 12 August 2026:

- canonical static definition of the 23 atomic Wang tiles;
- generalized-tile family metadata kept outside solver semantics;
- oriented local edge matching;
- Yang–Zhang signal tokens and deterministic adjacent-swap routing;
- application and validation of adjacent-swap sequences;
- minimal canonical in-memory Cubic Monotone 1-in-3 SAT representation using
  `variable_count` and clauses, with builder-side domain validation;
- transactional Yang–Zhang formula-to-region construction, including exact
  swap-trace ownership, dimensions, the paper-shaped simply connected active
  mask, and all exposed boundary colors;
- minimal dense row-major `Region` storage, access, and boundary constraints;
- independent verification of complete dense tilings, including region,
  boundary, inactive-cell, tile-ID, and adjacency validation;
- deterministic native serial solver with private compatibility masks,
  bitmask domains, propagation, MRV search, an undo trail, and mandatory
  independent validation of every SAT witness;
- optional solver metrics, a renderable best failed leaf for UNSAT, and a
  capped binary failed-leaf trace backed by `mmap`;
- C regression tests, deterministic fuzzing against brute-force and Boolean
  oracles, large-region stress cases, and end-to-end Yang-Zhang SAT/UNSAT
  checks;
- golden coverage of all 23 `(N,E,S,W)` tile tuples and focused solver-level
  tests for forwarder, anchor, and atomic crossover behavior;
- C17/OpenMP build scaffold and GitHub Actions CI with strict GCC/Clang,
  ASan, UBSan, GCC static analysis, Memcheck, and Cachegrind paths.

Not implemented yet:

- Cubic Monotone 1-in-3 SAT text parsing;
- whole-block crossover regression including its triangular forwarder/anchor
  areas;
- native OpenMP solver;
- Z3 Boolean and tiling models;
- square-to-hex translation and verification;
- JSON export and renderer integration.

Some empty files intentionally reserve the target module layout. A placeholder
does not indicate that the corresponding feature is implemented.

## Next milestones

Development proceeds through small, testable modules:

1. add the Cubic Monotone 1-in-3 SAT text parser and formula ownership API;
2. add a whole-block crossover regression for the triangular gadget areas;
3. add the Z3 cross-checks;
4. profile the serial baseline on larger reduction instances;
5. introduce OpenMP from measured serial split points;
6. implement and verify the square-to-hex translation;
7. stabilize JSON and renderer integration last.

The implementation follows a deliberately small design rule: each datum has one
owner, derived state is computed when needed, and future metadata is not added to
core structures before it has a concrete consumer.

## Build and test

Requirements:

- a C17 compiler;
- OpenMP support for the parallel build target;
- [`uv`](https://docs.astral.sh/uv/) for future Python/Z3 tests.

Run the complete current check:

```sh
make clean
make check
```

Useful individual targets:

```sh
make serial
make openmp
make c-check
make python-check
make strict-check
make sanitizer-check
make analyzer-check
make valgrind-check
make cachegrind-check
```

## Repository layout

```text
include/wang/    public C APIs
src/core/        tiles and region primitives
src/builder/     Yang-Zhang reduction components
src/solver/      serial solver
src/parallel/    OpenMP path
src/verify/      independent tiling verification
src/io/          serialization
python/z3/       reference Z3 models
python/hex/      square-to-hex translation and verifier
tests/           C, Python, and instance regressions
docs/            theory and architecture references
legacy/          frozen experimental code
```

## Documentation

- [`docs/development_principles.md`](docs/development_principles.md) records the
  practical rules used to keep module ownership clear and avoid premature
  abstractions as the implementation grows.
- [`docs/reduction_notes.md`](docs/reduction_notes.md) is the living record of
  reduction conventions already adopted by the implementation and of their
  explicit correctness obligations.
- [`docs/yang_zhang_builder_design.md`](docs/yang_zhang_builder_design.md) is
  the implementation contract for the formula-to-region builder, including
  ownership, signal routing, crossover orientation, offsets, and the complete
  boundary-color template.
- [`docs/serial_solver_implementation_guide.md`](docs/serial_solver_implementation_guide.md)
  records the implemented contract for the independent verifier,
  deterministic serial solver, optional metrics, UNSAT diagnostic snapshot,
  and mmap leaf trace.
- [`docs/references.md`](docs/references.md) records authoritative paper links,
  their role in the project, and when a PDF may be copied into the repository.
- [`docs/Wang23_C_OpenMP_Architecture_Spec_Merged.pdf`](docs/Wang23_C_OpenMP_Architecture_Spec_Merged.pdf)
  is the initial, future-facing architecture specification. Its proposed data
  structures are design sketches, not normative public APIs. Current headers
  and tests are authoritative for implemented behavior.

This separation is intentional: the README describes the project and its real
status, the reduction notes record mathematical/geometry conventions, and the
architecture PDF preserves the broader destination without forcing premature
abstractions into the code.

## Legacy policy

The old Pygame, procedural-generation, solver, notes, proof, and asset material
is frozen under `legacy/`. It may be consulted for ideas but is not a formal
specification, proof artifact, or dependency of the new implementation.

## Primary reference

Chao Yang and Zhujun Zhang, *NP-completeness of Tiling Finite Simply Connected
Regions with a Fixed Set of Wang Tiles*, arXiv:2405.01017 (2024).

See the architecture specification for the extended bibliography and future
research directions.

## License

See [`LICENSE`](LICENSE).
