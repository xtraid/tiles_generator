# Hexagonal Wang Tiles — fixed-set tiling, dual solving and verified rendering

Research and implementation project on **finite Wang tiling**, centered on the fixed-tileset NP-completeness construction of Yang–Zhang and on a controlled translation from square Wang tiles to hexagonal Wang tiles.

> **Project reset — August 2026**
>
> This repository is being restarted from the theory and the specification outward.
> The previous codebase is preserved as **legacy / experimental work**, but it is not
> the implementation base of the new core.
>
> The new project is built around two mandatory and independent solving paths:
>
> 1. a **native Wang-tile solver in C**, later parallelized with OpenMP;
> 2. a **Z3 reference solver**, used both at the Boolean-formula level and, for small
>    instances, at the tiling-constraint level.
>
> The two paths are intentionally tested against each other.

---

## Project goal

The project aims to build a small, reproducible implementation around:

> Chao Yang, Zhujun Zhang —  
> *NP-completeness of Tiling Finite Simply Connected Regions with a Fixed Set of Wang Tiles*  
> arXiv:2405.01017 (2024)

The fixed set of **23 Wang tiles** used in the paper is the primary implementation target.

The system should be able to:

1. parse or construct an instance of **Cubic Monotone 1-in-3 SAT**;
2. construct the corresponding finite simply connected Wang region;
3. solve the resulting tiling instance with a native C solver;
4. solve the Boolean instance independently with Z3;
5. cross-check witnesses in **both directions**;
6. optionally solve the tiling constraints themselves in Z3 for small instances;
7. translate a validated square Wang tiling into a hexagonal Wang representation;
8. verify the hexagonal representation;
9. export a renderer-independent JSON description;
10. render the result using a hexagonal adaptation of
    [`xtraid/PAP_render`](https://github.com/xtraid/PAP_render).

## Current implementation status

As of **8 August 2026** on `main`:

- [x] repository reset around the C/OpenMP + Z3 architecture;
- [x] canonical 23-atomic-tile Yang-Zhang tileset;
- [x] generalized-tile family metadata kept separate from solver semantics;
- [x] oriented local Wang matching;
- [x] C regression tests for the tileset and matching API;
- [x] coarse Yang-Zhang adjacent-swap layout planner;
- [x] GitHub Actions build/test pipeline;
- [ ] Cubic Monotone 1-in-3 SAT parser and validator;
- [ ] unique occurrence tokens and source/target permutation builder;
- [ ] concrete region rasterization and boundary coloring;
- [ ] independent tiling verifier;
- [ ] native serial search solver;
- [ ] Z3 Boolean and tiling reference paths;
- [ ] OpenMP search implementation;
- [ ] square-to-hex verified translation;
- [ ] renderer integration.

The current `YangZhangLayout` is **not yet the region builder**. It only computes
coarse dimensions and owns the adjacent-swap sequence.

The coarse layout intentionally includes two forwarder columns before and two after the
crossover chain. This is a **project convention for explicit signal entry/exit bands**,
not a claim that Yang-Zhang require those exact standalone columns. The formula-to-permutation
layer should be completed before concrete region rasterization.

## Documentation

- [`docs/Wang23_C_OpenMP_Architecture_Spec_Merged.pdf`](docs/Wang23_C_OpenMP_Architecture_Spec_Merged.pdf)
  — architecture and implementation specification.
- [`docs/reduction_notes.md`](docs/reduction_notes.md)
  — implementation conventions checked directly against the Yang-Zhang reduction.

---

# 1. Scope

## 1.1 Core tiling problem

The core solver works on **finite simply connected square-grid regions** with a **fixed Wang tileset**.

The intended semantics are:

- fixed tile set;
- fixed edge colors;
- translation only;
- no rotation;
- no reflection;
- explicit boundary constraints;
- tileability as a decision problem.

The new implementation must not inherit theoretical claims from the old prototype.

---

## 1.2 Source SAT problem

The reduction implemented from Yang–Zhang is based on the appropriate
**Cubic Monotone 1-in-3 SAT** formulation used by the construction.

A Boolean assignment is valid when every clause contains exactly one true variable.

The SAT instance is not merely test data: it is one side of the cross-validation
architecture.

---

# 2. The two mandatory solvers

Z3 is **not optional** in this project.

The project deliberately contains two conceptually independent solvers.

## 2.1 Native Wang solver

Implemented in **C**.

Input:

```text
finite Wang region
+ fixed 23-tile set
+ boundary conditions
```

Output:

```text
SAT + concrete tiling witness
```

or:

```text
UNSAT
```

The first implementation must be serial.

Only after correctness is established should an OpenMP execution path be added.

---

## 2.2 Z3 reference solver

The Z3 path is part of the core architecture.

It has two roles.

### Role A — Boolean reference solver

Z3 solves the original Cubic Monotone 1-in-3 SAT instance.

Output:

```text
Boolean assignment
```

That assignment is then mapped through the Yang–Zhang construction to the
corresponding signal choices / expected tiling behavior.

### Role B — tiling reference solver

For small instances, Z3 may also encode the Wang tiling problem directly:

```text
one tile-id variable per cell
+ oriented edge-matching constraints
+ boundary constraints
```

This gives a second, implementation-independent way of checking tileability.

The Z3 tiling model is especially useful for regression tests and counterexample
search. It is not a substitute for the native C implementation.

---

# 3. Bidirectional cross-validation

A central requirement of the project is that the reduction is exercised in both directions.

There are therefore two primary **against tests**.

---

## 3.1 Z3 Boolean witness -> Wang tiling

Pipeline:

```text
Cubic Monotone 1-in-3 SAT formula
            |
            v
      Z3 Boolean solver
            |
            v
      Boolean assignment
            |
            v
Yang-Zhang witness / signal instantiation
            |
            v
       Wang tiling
            |
            v
 independent tiling verifier
```

The verifier checks that:

- every region cell is tiled exactly once;
- every selected tile belongs to the fixed tile set;
- all internal edge colors match;
- all boundary colors are respected;
- the produced tiling is therefore a valid witness.

This direction tests the constructive implication:

```text
satisfying Boolean assignment
        =>
valid tiling of the constructed region
```

---

## 3.2 Native Wang witness -> Boolean witness

Pipeline:

```text
Cubic Monotone 1-in-3 SAT formula
            |
            v
 Yang-Zhang region builder
            |
            v
     native C Wang solver
            |
            v
        tiling witness
            |
            v
extract variable/signal assignment
            |
            v
Boolean witness verifier / Z3
```

The native solver returns a tiling.

From the signal-carrying part of that tiling, the implementation extracts the
induced Boolean assignment.

The original formula is then checked against that assignment.

This direction tests the reverse implication:

```text
valid tiling of the constructed region
        =>
satisfying Boolean assignment
```

Strictly speaking, the solver output does not reconstruct a new Boolean formula:
the **formula is already the input**. What is reconstructed from the tiling is the
Boolean **assignment / witness** induced by the signal tiles.

---

## 3.3 Third cross-check: Z3 tiling solver vs native tiling solver

For small and medium regression instances:

```text
same Wang instance
      |
      +-----------------------+
      |                       |
      v                       v
native C solver          Z3 tiling solver
      |                       |
      v                       v
 SAT / UNSAT              SAT / UNSAT
      |                       |
      +-----------+-----------+
                  |
                  v
        independent verifier
```

The solvers do **not** need to return the same tiling.

They must agree on:

```text
SAT vs UNSAT
```

and every SAT witness must pass the same independent verifier.

This is one of the strongest implementation tests available to the project.

---

# 4. Verification is separate from solving

The verifier must not simply call the solver again.

Given:

```text
region
tileset
candidate tiling
```

it checks deterministically:

- cell coverage;
- valid tile IDs;
- internal edge equality;
- boundary conditions;
- optional gadget invariants;
- optional extraction of signal values.

This keeps the architecture honest:

```text
search != verification
```

and prevents a bug in a solver from validating its own output.

---

# 5. Core C representation

## 5.1 Fixed tiles

The 23 Yang–Zhang tiles are static data.

Conceptually:

```c
typedef uint8_t ColorId;
typedef uint8_t TileId;

typedef struct {
    TileId id;
    ColorId north;
    ColorId east;
    ColorId south;
    ColorId west;
} WangTile;
```

The tileset can therefore be compiled into the program or loaded from one
canonical static definition.

The **region cannot** be generated at compile time because it depends on the
input Boolean instance.

---

## 5.2 Region

The region builder runs at runtime.

It produces:

- cells;
- coordinates;
- adjacency;
- boundary edges;
- boundary colors;
- gadget metadata;
- signal-path metadata;
- initial admissible tile domains.

The square-grid solver remains independent from rendering.

---

## 5.3 Cell domains

With 23 possible tile IDs, one cell domain fits naturally in a 32-bit mask.

Example:

```c
typedef uint32_t TileDomain;
```

Semantics:

```text
bit i = 1  -> tile i is currently admissible
bit i = 0  -> tile i is excluded
```

Useful operations become cheap:

- intersection;
- emptiness;
- singleton detection;
- candidate removal;
- state copy.

---

# 6. Native solver

The serial solver is the correctness baseline.

Conceptual loop:

```text
choose unresolved cell
        |
        v
choose candidate tile
        |
        v
propagate local constraints
        |
   +----+----+
   |         |
 valid   contradiction
   |         |
   v         v
continue   backtrack
```

The implementation should initially favor clarity and invariants over aggressive optimization.

---

# 7. Structural preprocessing and OpenMP

The Yang–Zhang construction has known geometric structure.

That structure can be exploited after the concrete region has been built.

A preprocessing phase may annotate:

- signal paths;
- independent sections;
- switch / synchronization positions;
- task dependencies;
- regions that may be solved independently;
- points where state must be merged or synchronized.

Pipeline:

```text
formula
   |
   v
region builder
   |
   v
structural analysis
   |
   v
TaskPlan / dependency DAG
   |
   v
OpenMP runtime
```

This preprocessing is:

```text
not compile time
not part of rendering
not part of the mathematical definition
```

It is instance-specific runtime preparation performed once before the parallel solve.

---

## 7.1 OpenMP strategy

Prefer:

```text
coarse tasks
+ explicit dependencies
+ independent state where possible
```

over:

```text
many threads
+ arbitrary concurrent writes
+ fine-grained locks
```

A useful design rule is:

> If two worker threads frequently compete to mutate the same cell/domain state,
> the task decomposition is probably too fine-grained.

OpenMP `task` / dependency mechanisms are the intended starting point.

The serial and OpenMP implementations must be regression-tested against each other.

---

# 8. Square Wang -> hexagonal Wang translation

The square solver and the hex renderer are intentionally separated.

The native solver does **not need to become a six-sided hex solver for the MVP**.

After a square tiling has been found and verified:

```text
validated square Wang tiling
            |
            v
square -> hex translator
            |
            v
hexagonal tiling representation
            |
            v
hex verifier
            |
            v
JSON / mask
            |
            v
PAP_render hex fork
```

---

## 8.1 Tile translation

The intended construction maps the four meaningful square Wang colors onto four
chosen sides of the hexagon.

The two additional hexagonal sides receive the same dedicated helper / neutral color.

Conceptually:

```text
square tile:

        N
      +---+
    W |   | E
      +---+
        S

             |
             v

hex tile:

       helper
      /      \
     /   N    \
    | W      E |
    |          |
     \   S    /
      \______/
       helper
```

The exact orientation is part of the formal square -> hex specification.

---

## 8.2 This is more than drawing

The translation can live in Python and does **not** require changing the native
four-sided solver, provided the transfer theorem / lemma is correct.

However, it is not purely a cosmetic rendering operation.

The project must verify that the translation preserves tileability.

In particular:

- the two helper sides must have a precise color semantics;
- adjacent translated hexagons must match on all six sides;
- the helper color must not create unintended degrees of freedom;
- boundary behavior must be preserved;
- the translated region must preserve the required topology;
- no extra tilings may appear merely because two extra edges were introduced.

Therefore the Python hex layer should contain a small **hex verifier**.

A general six-sided search solver is not required for the MVP.

---

# 9. Renderer

The new visualization path uses a fork/adaptation of:

[`xtraid/PAP_render`](https://github.com/xtraid/PAP_render)

The renderer is downstream from all correctness-critical logic.

It receives a validated intermediate representation and produces diagnostic images.

The renderer must **not** decide:

- SAT / UNSAT;
- Wang compatibility;
- Boolean correctness;
- proof validity.

It only visualizes already defined data.

---

## 9.1 PAP_render adaptation

Useful concepts to retain:

- CLI workflow;
- palette handling;
- framebuffer composition;
- asset loading;
- PNG export;
- testable separation of responsibilities.

The rectangular assumptions must be replaced or extended with:

- axial hex coordinates `(q, r)`;
- axial -> pixel conversion;
- hexagonal cells;
- six edge-color visualization;
- tile IDs;
- solver-generated JSON input.

No interactive editor is required for the MVP.

---

# 10. Intermediate JSON

The solver and the renderer communicate through a common representation.

A diagnostic file should eventually contain information such as:

```json
{
  "grid": "hex",
  "cells": [
    {
      "q": 0,
      "r": 0,
      "tile_id": 4,
      "edges": [1, 3, 7, 2, 9, 9]
    }
  ]
}
```

The precise schema should be versioned once stabilized.

The renderer must not import Z3 or native solver internals.

---

# 11. New repository architecture

Target structure:

```text
tiles_generator/
|
+-- README.md
+-- LICENSE
+-- Makefile
|
+-- include/
|   `-- wang/
|       +-- tile.h
|       +-- region.h
|       +-- solver.h
|       +-- verify.h
|       +-- task_plan.h
|       `-- yang_zhang.h
|
+-- src/
|   +-- core/
|   |   +-- tile.c
|   |   `-- region.c
|   |
|   +-- builder/
|   |   `-- yang_zhang.c
|   |
|   +-- solver/
|   |   `-- solver_serial.c
|   |
|   +-- parallel/
|   |   `-- solver_openmp.c
|   |
|   +-- verify/
|   |   `-- verify_tiling.c
|   |
|   `-- io/
|       `-- json.c
|
+-- python/
|   +-- z3/
|   |   +-- boolean_solver.py
|   |   +-- tiling_solver.py
|   |   `-- witness_check.py
|   |
|   `-- hex/
|       +-- translate_square_to_hex.py
|       `-- verify_hex.py
|
+-- schemas/
|   `-- tiling.schema.json
|
+-- tests/
|   +-- c/
|   +-- python/
|   `-- instances/
|
+-- examples/
+-- docs/
+-- tools/
|
`-- legacy/
    +-- pygame/
    +-- solver/
    +-- notes/
    +-- proofs/
    `-- assets/
```

Names may change during implementation; the separation of responsibilities should not.

---

# 12. Legacy code policy

The previous implementation is **frozen, not deleted**.

The old Pygame engine, biome system, patterns, props and previous procedural
generation experiments may contain useful ideas and code.

They remain available for possible future use, particularly if the project later
returns to:

- interactive editing;
- world generation;
- biome visualization;
- richer map semantics;
- animation;
- game-oriented rendering.

They are not currently:

- the solver architecture;
- a formal specification;
- proof evidence;
- required by the MVP.

The new implementation starts from the current theory and specification.

---

# 13. Testing strategy

Testing is a central part of the architecture.

## 13.1 Geometry tests

- square neighbors;
- boundary detection;
- region construction;
- connectedness;
- simple connectivity where implemented.

## 13.2 Tile tests

- exact definition of all 23 tiles;
- valid color IDs;
- oriented compatibility;
- no accidental rotation/reflection semantics.

## 13.3 Native solver tests

- small SAT instances;
- small UNSAT instances;
- witness validation;
- deterministic regression cases.

## 13.4 Z3 Boolean tests

- valid Cubic Monotone 1-in-3 instances;
- known satisfying assignments;
- known UNSAT formulas;
- exact-one clause semantics.

## 13.5 Bidirectional reduction tests

### Direction A

```text
Z3 assignment
-> constructed tiling
-> tiling verifier
```

### Direction B

```text
native tiling
-> extracted assignment
-> Boolean verifier / Z3
```

## 13.6 Solver-against-solver tests

For small instances:

```text
native C tiling solver
vs
Z3 tiling solver
```

Required agreement:

```text
SAT / UNSAT
```

Every generated witness must independently validate.

## 13.7 Parallel tests

```text
serial solver
vs
OpenMP solver
```

Again, the exact witness may differ.

The decision result must agree and both witnesses must validate.

## 13.8 Square -> hex tests

For small cases:

```text
square tiling
-> translation
-> hex verifier
```

Test specifically:

- helper-edge matching;
- boundary mapping;
- no malformed translated tiles;
- known SAT examples;
- counterexamples discovered during development.

Every discovered bug or counterexample should become a regression test.

---

# 14. Development roadmap

## Phase 0 — freeze scope and terminology

- formalize the finite simply connected Wang problem;
- fix orientation conventions;
- fix boundary semantics;
- define the exact SAT input representation;
- keep procedural generation separate.

**Gate:** the problem can be stated without vague references to implementation code.

---

## Phase 1 — literature and Yang–Zhang reduction

Core reading:

1. Yang–Zhang;
2. Pak–Yang;
3. Moore–Robson.

**Gate:** the reduction can be explained independently from the PDF.

---

## Phase 2 — fixed tile and region model

Implement:

- tile structures;
- colors;
- region;
- adjacency;
- boundary representation;
- verifier primitives.

**Gate:** geometry and tile invariants pass unit tests.

---

## Phase 3 — Z3 Boolean solver

Implement the source-formula model first.

**Gate:** known SAT / UNSAT formulas behave correctly and model extraction works.

---

## Phase 4 — Yang–Zhang builder

Implement:

```text
formula -> finite Wang region
```

with explicit gadget and signal metadata where useful.

**Gate:** representative constructed regions can be inspected and validated structurally.

---

## Phase 5 — native serial Wang solver

Implement the C reference solver.

**Gate:** SAT witnesses validate and known UNSAT cases terminate correctly.

---

## Phase 6 — bidirectional cross-validation

Implement:

```text
Z3 assignment -> tiling
```

and:

```text
tiling -> assignment -> formula check
```

**Gate:** both directions pass on the regression corpus.

---

## Phase 7 — Z3 tiling solver

Encode small Wang instances directly in Z3.

**Gate:** native C and Z3 tiling solvers agree on SAT / UNSAT.

---

## Phase 8 — structural analysis + OpenMP

Build the instance-specific TaskPlan and parallel solver.

**Gate:** serial and parallel decisions agree and performance can be measured reproducibly.

---

## Phase 9 — square -> hex translation

Formalize and implement the two-helper-edge translation.

**Gate:** translated instances pass the six-edge hex verifier and the theoretical proof obligations are explicitly documented.

---

## Phase 10 — PAP_render hex fork

Adapt rendering to axial coordinates and the common JSON format.

**Gate:** a small validated tiling renders correctly to PNG.

---

## Phase 11 — optional procedural branch

Only after the core is stable, reconsider:

- biome adjacency;
- height;
- props;
- interactive editing;
- richer procedural generation.

These remain future / stretch work.

---

# 15. MVP

Non-negotiable:

- [x] precise finite Wang problem definition;
- [x] fixed 23-tile representation;
- [ ] Cubic Monotone 1-in-3 SAT input representation;
- [ ] Yang–Zhang region builder;
- [ ] independent tiling verifier;
- [ ] Z3 Boolean solver;
- [ ] native serial C Wang solver;
- [ ] Z3 witness -> tiling cross-check;
- [ ] native tiling -> Boolean witness cross-check;
- [ ] Z3 tiling solver for small-instance validation;
- [ ] native vs Z3 SAT/UNSAT regression tests;
- [ ] OpenMP solver path;
- [ ] serial vs OpenMP regression tests;
- [ ] square -> hex translation;
- [ ] six-edge hex verifier;
- [ ] common JSON representation;
- [ ] PAP_render hex visualization;
- [ ] reproducible offline demo.

Stretch:

- procedural world generator;
- interactive editor;
- height;
- props;
- animation;
- global world-connectivity constraints;
- optimization of the number of hex tile types.

---

# 16. Research discipline

The repository distinguishes three categories.

## Published results

Claims imported from the literature.

These must be cited.

## Project lemmas / adaptations

For example the square -> hex transfer.

These need explicit assumptions, proof obligations and counterexample testing.

## Experimental implementation

Observed behavior of C, Z3, Python or rendering code.

Code behavior is not itself a mathematical proof.

No claim involving `P`, `NP`, NP-completeness, decidability or asymptotic complexity
should be inherited from the previous prototype without a new justification.

---

# 17. References

## Core

- Chao Yang, Zhujun Zhang. **NP-completeness of Tiling Finite Simply Connected Regions with a Fixed Set of Wang Tiles**. arXiv:2405.01017, 2024.
- Igor Pak, Jed Yang. **Tiling simply connected regions with rectangles**. *Journal of Combinatorial Theory, Series A* 120(7), 1804–1816, 2013.
- Cristopher Moore, John Michael Robson. **Hard Tiling Problems with Simple Tiles**. *Discrete & Computational Geometry* 26, 573–590, 2001.

## Square -> hex

- Karel Culik II. **Small Aperiodic Sets of Triangular and Hexagonal Tiles**. In *Jewels are Forever*, Springer, 1999.
- Sky Basire. **Wang Tiles**. University of Canterbury report, 2022.

## Background

- Hao Wang. **Proving Theorems by Pattern Recognition — II**. *Bell System Technical Journal* 40(1), 1961.
- Robert Berger. **The Undecidability of the Domino Problem**. *Memoirs of the AMS* 66, 1966.
- Raphael M. Robinson. **Undecidability and Nonperiodicity for Tilings of the Plane**. *Inventiones Mathematicae* 12, 1971.

## SAT / SMT

- Nikolaj Bjørner, Leonardo de Moura, Lev Nachmanson, Christoph Wintersteiger. **Programming Z3**.
- Microsoft Research / Z3 project. **Z3 Guide**.
- Carsten Sinz. **Towards an Optimal CNF Encoding of Boolean Cardinality Constraints**. CP 2005.

---

# License

See [`LICENSE`](LICENSE).
