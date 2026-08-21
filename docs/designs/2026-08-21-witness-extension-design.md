---
layout: page
title: Boolean–Wang witness correspondence
permalink: /witness_correspondence/
description: How exact Boolean assignments are extended to Wang tilings and extracted again without coupling the generic solver to reduction semantics.
section: Architecture and correctness
document_kind: Technical design
status: Current implementation
updated: 2026-08-21
nav_order: 30
---

# Boolean–Wang witness correspondence

## 1. Purpose

The pipeline establishes two important but separate facts:

- Boolean Z3 and the Wang solvers agree on SAT/UNSAT for shared inputs;
- every Boolean assignment and Wang tiling returned by its own solver passes an
  independent verifier.

Decision-level agreement alone does not compute the witness correspondence
required by the original architecture:

```text
specific Boolean assignment -> concrete Wang tiling -> Wang verifier
specific Wang tiling        -> Boolean assignment -> formula verifier
```

The implemented witness path adds that correspondence while preserving the
module boundaries. The generic Wang solver accepts an optional initial-domain input. A
small native Yang–Zhang bridge translates between Boolean assignments and the
variable cells of a concrete reduction. A minimal Python coordinator keeps the
native formula and reduction alive while connecting Boolean Z3 to the native
solver. No persistent signal plan, copied swap trace, or reverse-marshalled
formula or region is introduced.

## 2. Decision summary

The implementation has three layers:

1. **Generic C solver capability.** When supplied, `WangSolverOptions` borrows
   a dense array of initial tile-domain masks. Both the reference and optimized
   entry points apply the same contract. This layer knows only `Region`, tile
   masks, and Wang constraints.
2. **Native Yang–Zhang witness bridge.** A small cross-check C module receives the concrete
   `YangZhangReduction`, constructs dense initial masks from a Boolean
   assignment, extracts a Boolean assignment from a verified dense tiling, and
   tests whether an assignment and tiling correspond. It does not inspect the
   reduction's swap trace. This is the only new layer that knows the
   variable-gadget coordinates and tile IDs. It sits above the generic solver
   and independent verifier rather than inside either module.
3. **Minimal Python orchestration.** A native adapter/coordinator parses once,
   keeps the `Cm13Formula` and `YangZhangReduction` alive, copies the existing
   immutable Python formula and region views, obtains a Boolean Z3 assignment,
   and asks the native bridge and selected native solver to extend that exact
   assignment. Python orchestrates lifetimes and independent checks; it does
   not reproduce gadget semantics or solver logic.

The C operation names below mirror the implemented API. Exact declarations live
in the public headers; the contracts and dependency boundaries described here
are the stable part of the design.

## 3. Goals

- Extend one exact satisfying Boolean assignment to a concrete, independently
  verified Wang tiling of its corresponding Yang–Zhang region.
- Extract one exact Boolean assignment from any verified tiling of the
  corresponding region, leaving formula verification to the external Boolean
  checker so a spurious Wang witness remains observable as a counterexample.
- Establish the round trip
  `extract(solve_assignment_extension(a)) == a` for every tested satisfying
  assignment, rather than comparing whichever unrelated models two solvers
  happen to choose.
- Make initial-domain restriction a reusable Wang-solver feature with precise
  ownership, validation, and status semantics.
- Exercise identical public semantics through `wang_solve_serial()` and
  `wang_solve_optimized()`.
- Retain independent Wang and Boolean verification at the end of each path.

## 4. Non-goals

- No claim that every satisfying assignment has exactly one tiling.
- No requirement that
  `solve_assignment_extension(extract(tiling))` reproduce the original tiling
  byte for byte.
- No requirement that independent Boolean, native Wang, and Wang Z3 solvers
  choose the same satisfying model without an explicit assignment restriction.
- No persistent `SignalPlan`, gadget map, witness-pair object, or new formula,
  region, assignment, or tiling model.
- No Python copy of `YangZhangReduction.swaps` and no Python reimplementation of
  occurrence routing or gadget placement.
- No reverse marshalling from Python `Formula` or `Region` into native structs.
- No Yang–Zhang concepts in the generic solver and no solver concepts in the
  immutable model classes.
- No OpenMP work, square-to-hex work, renderer integration, JSON schema work,
  or unrelated solver optimization.

## 5. Correctness statement

Let `F` be a validated Cubic Monotone 1-in-3 SAT formula and `R` the exact
`Region` produced from `F` by `yang_zhang_build()` under the current project
layout and canonical `TILESET`.

For every well-formed Boolean assignment `a` of length `F.variable_count`:

```text
is_valid_assignment(F, a)
    iff
solve_assignment_extension(F, R, a) returns WANG_SOLVE_SAT.
```

When the result is SAT, its dense singleton domains must convert to a tiling
`t` such that:

```text
wang_verify_tiling(R, t) == WANG_VERIFY_VALID
extract(F, R, t) == a
correspond(F, R, a, t) == true.
```

For every verified tiling `t` of `R`, `extract(F, R, t)` must return the value
encoded by its variable gadgets. The caller then applies the independent
Boolean witness checker. Extraction must not reject or hide a decoded value
merely because it fails that external formula check.

These properties define a computable correspondence in which extraction is a
left inverse of assignment extension. They deliberately do not assert a
bijection between all satisfying assignments and all possible tilings. A
uniqueness claim requires separate proof and model-counting evidence and is
outside this design.

## 6. Provenance precondition

The bridge accepts a `Formula`/`Cm13Formula` and a `YangZhangReduction` under
the precondition that the reduction is the successful output of
`yang_zhang_build()` for that exact formula and the current canonical tileset
and layout constants. Accepting the builder result rather than a generic
`Region` represents this provenance at the API boundary without adding
persistent metadata. The bridge borrows `reduction.region` and deliberately
does not inspect `reduction.swaps`. The production Python coordinator satisfies
the precondition structurally by parsing once and keeping the native formula
and reduction in one lifetime.

The bridge must still reject detectable inconsistencies, including invalid
formula storage, invalid region storage, impossible dimensions, wrong dense
array lengths, and malformed variable-gadget coordinates. It is not required
to reconstruct the entire reduction or prove that an arbitrary independently
supplied region came from the supplied formula. Same-sized but unrelated
formula/reduction pairs remain a documented precondition violation, not a case
that motivates persistent provenance metadata.

## 7. Generic initial-domain solver contract

### 7.1 Public options

`WangSolverOptions` gains these optional borrowed dense-domain fields:

```c
const uint32_t *initial_domains;
size_t initial_domain_count;
```

The pointer is borrowed and immutable for the duration of the solve call. The
solver copies/intersects values into its private state and never stores, frees,
or modifies caller storage. Callers must zero-initialize `WangSolverOptions` or
initialize every field explicitly; omitted designated fields then preserve the
current unconstrained behavior.

The feature does not require a flag. Absence and presence are strict:

- absent means `initial_domains == NULL` and `initial_domain_count == 0`;
- present means `initial_domains != NULL` and
  `initial_domain_count == region->cell_count`;
- every other pointer/count combination is an API error.

The array is dense row-major and parallel to `Region.cells` and to result
domains.

### 7.2 Mask semantics

The public contract defines the legal tile-bit universe as the low
`TILE_COUNT` bits:

```text
WANG_DOMAIN_ALL = (UINT32_C(1) << TILE_COUNT) - UINT32_C(1)
```

The public `WANG_DOMAIN_ALL` constant is the canonical expression of this mask,
so generic callers and the native bridge do not duplicate the bit expression.

For every supplied entry:

- bits outside `WANG_DOMAIN_ALL` are forbidden and cause
  `WANG_SOLVE_ERROR`;
- an inactive cell must have mask zero; any nonzero inactive mask causes
  `WANG_SOLVE_ERROR`;
- an active cell may have any subset of `WANG_DOMAIN_ALL`, including zero;
- `WANG_DOMAIN_ALL` means no caller restriction for that active cell;
- a singleton fixes that cell to one tile ID;
- a multi-bit mask restricts the cell to those candidate tile IDs;
- zero on an active cell is a well-formed contradictory constraint and causes
  `WANG_SOLVE_UNSAT`, not `WANG_SOLVE_ERROR`.

After `region_validate()` succeeds, the solver validates the complete
pointer/count pair and every supplied mask before interpreting any empty active
domain as UNSAT. A malformed later entry therefore cannot be hidden by an
earlier zero mask; any malformed entry makes the whole call ERROR.

For an active cell, initialization is the intersection of three independent
sources of truth:

```text
canonical tile universe
    & optional caller initial domain
    & all exposed boundary-color masks.
```

Initial domains never bypass boundary validation, oriented adjacency,
propagation, search, or final independent tiling verification. A legal nonzero
mask that becomes empty after boundary intersection or propagation is UNSAT.

### 7.3 Initialization and diagnostics

Caller masks are root constraints. They are applied before initial propagation
and do not create rollback trail entries, just as boundary masks do not. The
reference solver retains its full initial-propagation trail behavior and the
optimized solver retains its no-initial-trail optimization; both begin search
from the same restricted fixed point.

`domain_reductions`, when collected, counts an effective restriction from
`WANG_DOMAIN_ALL` to a supplied active-cell mask and each later effective
boundary or propagation narrowing according to the existing counter rule.
Calls without `initial_domains` retain current metrics and deterministic
results.

All existing trace, UNSAT snapshot, ownership, SAT-domain verification, and
late-error cleanup rules remain in force. In particular, a root contradiction
from legal initial domains is an ordinary failed leaf and participates in the
existing trace and opt-in snapshot behavior.

### 7.4 Status boundary

The solver must preserve the distinction between malformed input and a
well-formed inconsistent constraint:

| Condition | Status |
| --- | --- |
| Invalid region, output, flags, pointer/count pair, high mask bits, or inactive nonzero mask | `WANG_SOLVE_ERROR` |
| Allocation, trace, propagation-infrastructure, or final-verification failure | `WANG_SOLVE_ERROR` |
| Active zero mask or contradiction after boundary/adjacency propagation | `WANG_SOLVE_UNSAT` |
| Complete verified extension | `WANG_SOLVE_SAT` |

When the caller supplies the required destroyed `out_result`, every
`WANG_SOLVE_ERROR` path leaves it destroyed. Passing an already-owned result is
itself an API violation: the call returns ERROR and leaves that caller-owned
object unchanged so it is not leaked or overwritten. SAT and UNSAT retain the
current result ownership contract. Both public solver entry points implement
this exact table through their shared core.

## 8. Native Yang–Zhang witness bridge

### 8.1 Boundary

The bridge is a small C module above `formula`, `yang_zhang`, `tile`, `verify`,
and `solver`. Its public operations borrow `YangZhangReduction`, use only its
`region`, and never inspect its `swaps`. Its extension operation calls the
generic solver, but the solver must not call back into the bridge. Conceptual
operations are:

- `yang_zhang_solve_assignment_extension()`;
- `yang_zhang_extract_assignment()`;
- `yang_zhang_witnesses_correspond()`.

The module owns no persistent state. It uses caller-owned inputs, temporary
dense masks or tile arrays, and transactional caller-provided outputs.

### 8.2 Assignment-to-domain translation

For a formula with `n` variables, a well-formed assignment has exactly `n`
Boolean values. The bridge validates storage, length, the metadata needed for
safe positional access, and the detectable portion of the formula/reduction
provenance contract. It does not evaluate the assignment against the clauses.
The successful-builder provenance precondition remains the source of the
complete cubic reduction-domain guarantee; the bridge does not duplicate the
builder's full validator.

It then allocates one dense domain mask per `RegionCell`:

- inactive cell: `0`;
- active cell not belonging to a variable gadget: `WANG_DOMAIN_ALL`;
- for false variable `v`, cells `(0, 4v)`, `(0, 4v + 1)`, and
  `(0, 4v + 2)` are fixed respectively to `TILE_V0_TOP`, `TILE_V0_MID`, and
  `TILE_V0_BOTTOM`;
- for true variable `v`, all three cells are fixed to `TILE_V1`.

These are the only Yang–Zhang-specific initial restrictions. Redundant rows,
forwarders, anchors, crossovers, and clause gadgets remain consequences of the
region boundary, canonical tileset, propagation, and search.

A well-formed but formula-invalid Boolean assignment is not an API error.
`solve_assignment_extension()` must not determine or assume its Boolean
validity: it fixes the variable domains, runs the native Wang solver, and
returns the Wang status. The external proof harness alone requires equality
between direct Boolean validity and Wang SAT. The bridge must never
short-circuit from a Boolean checker or return SAT without a Wang solve and
verified tiling.

The operation supports both reference and optimized native entry points through
an explicit solver selector. It does not add a Yang–Zhang mode flag to
`WangSolverOptions`.

On SAT, the extension operation returns the generic solver result without
calling extraction or correspondence. The proof harness normalizes the
singleton domains, independently verifies the complete tiling, extracts its
assignment, and compares it with the requested assignment. Keeping these
postconditions outside the extension operation avoids making the forward path
self-confirming and preserves the original SAT result as a counterexample if
verification, extraction, or equality fails.

### 8.3 Tiling-to-assignment extraction

Extraction accepts a dense tiling aligned with the region. It validates the
length and requires `wang_verify_tiling()` to accept the entire witness before
reading logical values. It then examines each variable block in the leftmost
column:

- the exact `V0_TOP`, `V0_MID`, `V0_BOTTOM` triple encodes false;
- three exact `V1` tiles encode true;
- every other mixture is rejected.

The extracted array has exactly `formula.variable_count` Boolean entries.
Extraction does not evaluate clauses or call a Boolean witness checker. Output
is transactional: invalid arguments, invalid tilings, or inconsistent variable
gadgets leave caller output unchanged. Once extraction succeeds, the external
proof harness checks all three ordered positions of every formula clause,
including repeated variable occurrences. If that check fails, the decoded
assignment and tiling are retained and reported as a reduction counterexample.

Extraction does not inspect solver decision history, domains wider than the
final singleton tiling, swap traces, or signal tokens. It therefore applies
equally to reference-native, optimized-native, and Wang-Z3 tilings after they
have been normalized to the same dense tile representation.

### 8.4 Correspondence predicate

`correspond()` verifies the Wang tiling, extracts its encoded assignment, and
reports true exactly when it equals the supplied well-formed assignment. It
does not evaluate either assignment against the formula. It is a representation
relation check, not a reduction-validity check or a comparison with a solver's
preferred model.

The concrete C contract must preserve three outcomes, whether by an enum or by
a status plus an output Boolean:

- malformed API storage, invalid formula/reduction metadata, provenance failures
  that can be detected, or allocation failures are operation errors;
- a well-formed tiling rejected by the Wang verifier, an undecodable variable
  gadget, or two well-formed representations of different assignments is a
  successful negative relation;
- a verified tiling whose extracted assignment equals the supplied assignment
  is a successful positive relation, regardless of the later external Boolean
  validity result.

No failure outcome may be silently collapsed into a positive or negative
relation.

## 9. Python lifetime coordinator

The Python layer provides the actual Boolean-Z3-to-native-Wang path without
reverse marshalling:

```text
.cm13
  |
  v
native parse -----------------------------------------------+
  |                                                         |
  +-> copy immutable Python Formula -> Boolean Z3 -> a      |
  |                                               |         |
  +-> build and keep native YangZhangReduction    |         |
                                                  v         |
                                    native witness bridge    |
                                                  |         |
                              generic native solver with     |
                                  initial_domains            |
                                                  |         |
                                         verified tiling     |
                                                  |         |
                                   extract and compare a <---+
```

The coordinator must:

1. load the native formula once;
2. copy it into the existing immutable Python `Formula`;
3. build and retain the native reduction from that same live formula;
4. copy the existing immutable Python `Region` view when Python verification
   needs it;
5. call Boolean Z3 on the Python formula;
6. on SAT, pass the exact Boolean tuple back as primitive values to the native
   bridge while the native formula and reduction remain alive;
7. select reference or optimized solving explicitly;
8. copy the resulting dense tiling into the existing Python convention of tile
   IDs for active cells and `None` for inactive cells;
9. run the existing independent Python Boolean and Wang witness checkers;
10. destroy the native reduction and then the native formula in `finally`
    cleanup on every path.

No ctypes pointer may escape. The adapter adds no persistent aggregate result
model. Its narrow helpers return existing status enums and immutable assignment
or tiling tuples; the coordinator composes them within one call and copies out
only existing Python-owned values. It does not copy `swaps`, reconstruct
source/target tokens, calculate gadget coordinates, or implement clause
semantics.

If Boolean Z3 returns UNKNOWN, the coordinator propagates
`BooleanSolveStatus.UNKNOWN`, returns no tiling, and does not invoke the native
extension path. Native `WANG_SOLVE_ERROR` and `WANG_SOLVE_UNSAT` remain distinct
in Python; neither may be collapsed into the other or into Boolean UNKNOWN.

The reverse path takes any normalized native or Wang-Z3 tiling, invokes the
native extraction bridge under the same provenance lifetime, copies the
Boolean values into a Python tuple, and runs the existing independent Boolean
witness checker. It does not ask Boolean Z3 to rediscover an assignment.

## 10. Why no SignalPlan or copied swaps are needed

The exact assignment is represented at the three variable cells for each
variable. The native builder's region boundary already forces signal
forwarding, crossover locations, clause behavior, and redundant zero rows.
The solver therefore needs only the initial variable-cell restrictions.

Extraction reads those same variable cells after complete tiling verification;
it does not need to trace signals through the region. Formula clause storage is
used only by the external Boolean checker, not by extraction. Occurrence tokens
and adjacent swaps are not needed.

The production coordinator already has the native `YangZhangReduction` alive,
so adding a Python `SignalPlan` or copying `swaps` would duplicate derived
construction data without a consumer. The existing swap trace remains native
diagnostic metadata and is destroyed with the reduction.

## 11. Error handling and ownership

- Initial domain arrays are borrowed by the generic solver and by the bridge's
  solve call; temporary arrays allocated by the bridge are always freed before
  return.
- The extension operation transfers the generic solver's SAT/UNSAT result under
  the existing `WangSolveResult` contract. It does not call extraction or
  rewrite a SAT mismatch as ERROR; the external proof harness retains the
  returned result when checking all postconditions.
- Assignment and tiling inputs are borrowed and immutable.
- Caller-provided extraction output is written only after Wang-witness and
  variable-gadget validation succeeds; Boolean validity is checked later and
  never suppresses a decoded counterexample.
- Normalized tiling buffers follow the current solver/verifier ownership rules
  and are destroyed on all error paths.
- A well-formed non-satisfying assignment, an active zero initial mask, or a
  legal restriction with no extension is UNSAT.
- Invalid lengths, illegal bits, nonzero inactive masks, invalid regions,
  malformed tiling storage, allocation failure, and violated internal SAT
  postconditions are errors.
- For `correspond()`, a structurally well-formed tiling rejected by the Wang
  verifier is a successful negative relation. For `extract()`, it is an invalid
  witness and produces no assignment. Neither case is an infrastructure error.
- A valid tiling representing a different assignment is a normal
  negative `correspond()` result, not an error.
- The Python coordinator releases the reduction before the formula and uses
  `finally` blocks for both lifetimes.

## 12. Verification evidence

### 12.1 Generic solver tests

The C suite runs every public-contract case through both
`wang_solve_serial()` and `wang_solve_optimized()`:

- absent initial domains preserve the existing deterministic status and SAT
  witness;
- `NULL/0` is accepted, while `NULL/nonzero`, `nonnull/0`, and wrong counts are
  errors;
- high tile bits are errors;
- a malformed later mask still makes the call ERROR when an earlier active
  entry is zero, proving complete validation precedes UNSAT interpretation;
- an inactive zero entry is accepted and an inactive nonzero entry is an error;
- a region with no active cells accepts an all-zero dense initial array and
  remains SAT;
- an active zero entry is UNSAT, including existing root-failed-leaf trace and
  opt-in snapshot behavior;
- singleton and multi-bit masks restrict solutions without being modified;
- a legal mask incompatible with a boundary or neighbor is UNSAT, not ERROR;
- an isolated cell selects within its restricted mask rather than ignoring it;
- metrics and diagnostics preserve their contracts;
- every ERROR reached from a valid destroyed output leaves
  `WangSolveResult` destroyed, while an already-owned output is rejected
  unchanged;
- reference and optimized statuses agree and every SAT witness passes the
  independent verifier.

The differential suite includes randomized small generic regions with random
legal initial masks and compares each solver against brute force under the same
masks.

### 12.2 Exhaustive small witness equivalence

The exhaustive suite reuses all 1,701 canonical formulas through three
variables. For every formula it enumerates all `2^n` Boolean assignments rather
than requesting one preferred solver model.

For each assignment and for each native solver entry point:

1. compute direct Boolean witness validity;
2. run `solve_assignment_extension()`;
3. require SAT exactly when the direct Boolean witness is valid and UNSAT
   otherwise;
4. for SAT, normalize and independently verify the tiling;
5. extract its assignment and require exact equality with the enumerated input;
6. require `correspond()` to be true;
7. on focused representative fixtures, corrupt each variable-block pattern
   and require extraction or correspondence to reject it without mutating
   outputs;
8. on explicit multi-witness fixtures, pair the tiling with a different valid
   assignment and require a successful negative correspondence result.

This is the primary computational evidence for the reduction at witness level.
It is stronger than one SAT/UNSAT comparison per formula and avoids ambiguity
from formulas with multiple satisfying assignments. Any failed SAT
postcondition must report the formula, requested assignment, solver selector,
and returned tiling before cleanup so the mismatch remains reproducible.

### 12.3 End-to-end adapter tests

- Boolean Z3 SAT assignment -> reference native extension -> C verifier ->
  Python tiling checker -> exact extracted Boolean assignment -> Python Boolean
  checker.
- The same path through the optimized native solver.
- Native reference and optimized tilings obtained without a requested Boolean
  model -> extraction -> direct Boolean checker.
- Wang Z3 tiling -> extraction -> direct Boolean checker.
- Shared UNSAT fixtures preserve UNSAT and produce no witness.
- A controlled Boolean UNKNOWN result does not call the native extension path.
- Parser, bridge, solver, copy-out, and verifier failures release both native
  lifetimes and retain their distinct statuses.
- Repeated variable positions such as `(x, x, y)` are counted positionally in
  every Boolean verification.

### 12.4 Regression gates

The implementation passed the C and Python suites, strict GCC and Clang builds,
sanitizers, static analysis, Memcheck, and the relevant Cachegrind and
differential targets. The standard comparison benchmark remains a
decision/performance suite; witness-extension timing is outside its published
baselines.

## 13. Documentation boundary

The public description of the feature is split by concern:

- the [architecture reference]({{ '/development_principles/' | relative_url }})
  defines module ownership and dependency direction;
- the [serial solver reference]({{ '/serial_solver_implementation_guide/' | relative_url }})
  defines initial-domain, status, and ownership contracts;
- the [reduction note]({{ '/reduction_notes/' | relative_url }}) states the
  witness-level correspondence and its non-bijection caveat;
- this page explains why the native bridge and Python coordinator preserve
  independent verification.

The [initial architecture specification]({{ '/historical_architecture/' | relative_url }})
and the Yang–Zhang paper remain historical and theoretical sources rather than
descriptions of the current API.

## 14. Implemented properties and limits

The implementation provides these properties:

- optional initial domains obey the strict dense mask contract in both native
  solver entry points;
- the generic solver remains free of formula, assignment, signal, gadget, and
  Yang–Zhang dependencies;
- the native bridge implements extension, extraction, and representation-only
  correspondence over `YangZhangReduction` without persistent metadata or
  access to `swaps`;
- the Python coordinator performs the real Boolean-Z3-to-native-Wang witness
  path without reverse marshalling or copied swaps;
- exhaustive small formulas and assignments establish SAT equivalence and exact
  assignment round trips for reference and optimized solvers;
- native and Wang-Z3 tilings extract independently verified Boolean witnesses;
- ERROR, UNSAT, SAT, and Boolean UNKNOWN remain distinct;
- ownership, invalid-input, independent-verification, regression, and artifact
  gates cover the feature.

It does not introduce a `SignalPlan`, copy swaps into Python, enable OpenMP,
integrate rendering, or claim a bijection between assignments and tilings.
