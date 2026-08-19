---
layout: page
title: "Technical guide: verifier, serial solver, and leaf trace"
permalink: /serial_solver_implementation_guide/
description: An implementation guide to verification, serial Wang search, diagnostics, and leaf tracing.
---

# Technical guide: verifier, serial solver, and leaf trace

Implementation status as of 17 August 2026: this guide has been implemented.
The public headers and tests are authoritative where they differ from an
earlier prospective detail below. The document remains the technical contract
and maintenance handoff for the module.

## 1. Scope

This guide is an operational handoff for implementing the project's next block
without having to reconstruct the architectural decisions from its history.

The work includes:

1. an independent verifier for complete tilings;
2. a deterministic serial solver based on bitmask domains;
3. local propagation, MRV, and rollback through an undo trail;
4. optional metrics;
5. a renderable snapshot that is always returned for `SAT` and available for
   `UNSAT` only through an explicit diagnostic flag;
6. an optional binary trace of all failed leaves, written through `mmap` to a
   file with a known maximum capacity and truncated to its actual size on
   completion.

Do not implement in this block:

- a formula parser;
- OpenMP or other forms of parallelism;
- memoization, skip lists, clause learning, or backjumping;
- Z3, JSON, or rendering;
- a compact formal certificate of unsatisfiability;
- changes to the Yang-Zhang builder or the tileset, except for corrections
  demonstrated by independent tests.

The solver receives only a `Region`. The 23 tiles are always read from the
canonical `TILESET` in `src/core/tile.c`; a tileset is not passed as a
parameter, and compatibility information is not duplicated manually.

## 2. Repository status and constraints

The following implemented APIs are authoritative:

- `include/wang/tile.h`: `TileId`, `ColorId`, directions, colors, and 23 tiles;
- `include/wang/region.h`: dense row-major geometry, active mask, and
  boundaries;
- `include/wang/yang_zhang.h`: formula-to-region construction;
- `wang_tiles_match()`: reference implementation for oriented local matching.

Files implemented by this block:

- `include/wang/verify.h`;
- `src/verify/verify_tiling.c`;
- `include/wang/solver.h`;
- `src/solver/solver_serial.c`;
- `src/solver/failed_leaf_trace.c` and its private header.

Tests added:

- `tests/c/test_verify.c`;
- `tests/c/test_solver.c`;
- `tests/c/test_solver_stress.c`;
- `tests/c/test_solver_yang_zhang.c`.

The `Makefile` uses wildcards for C tests and already includes the verifier and
solver in the serial library. Do not add a parallel build system.

Design rules to preserve:

- `TILESET` is the single source of truth for edges and colors;
- the verifier does not use solver state or caches;
- the solver does not read `AdjacentSwap` or gadget metadata;
- tiles may be reused without limit;
- rotations and reflections are not allowed;
- there is no mutable global state;
- every public output is constructed transactionally.

## 3. Domain representation

`TILE_COUNT == 23`, so a `uint32_t` contains the complete domain of a cell:

```c
#define WANG_DOMAIN_ALL \
    ((UINT32_C(1) << TILE_COUNT) - UINT32_C(1))
```

Conventions:

- bit `t` set: `TILESET[t]` is still allowed;
- zero domain on an active cell: conflict;
- exactly one bit: placed/forced tile;
- multiple bits: unresolved cell;
- zero domain on an inactive cell: normal value, not a conflict.

Do not introduce an `assignment` array: for active cells, the assignment can
be derived from singleton domains.

## 4. Verifier API

Define the following in `include/wang/verify.h`:

```c
#ifndef WANG_VERIFY_H
#define WANG_VERIFY_H

#include <stddef.h>

#include "wang/region.h"
#include "wang/tile.h"

#define TILE_NONE ((TileId)UINT8_MAX)

typedef enum {
    WANG_VERIFY_VALID = 0,
    WANG_VERIFY_INVALID_ARGUMENT,
    WANG_VERIFY_INVALID_REGION,
    WANG_VERIFY_INVALID_LENGTH,
    WANG_VERIFY_INCOMPLETE,
    WANG_VERIFY_INVALID_TILE_ID,
    WANG_VERIFY_INACTIVE_ASSIGNED,
    WANG_VERIFY_BOUNDARY_MISMATCH,
    WANG_VERIFY_ADJACENCY_MISMATCH
} WangVerifyStatus;

WangVerifyStatus wang_verify_tiling(
    const Region *region,
    const TileId *tiles,
    size_t tile_count
);

#endif /* WANG_VERIFY_H */
```

If another module needs `TILE_NONE`, moving the macro to `tile.h` is
acceptable; there must still be exactly one definition.

The tiling is a dense array parallel to `Region.cells`:

- exact length `width * height`;
- active cell: `TileId < TILE_COUNT`;
- inactive cell: must be `TILE_NONE`.

The verifier must:

1. validate pointers, dimensions, and the `width * height` product;
2. reject out-of-range colors;
3. reject boundary constraints on inactive cells or on edges touching an
   active cell;
4. check every exposed constraint other than `COLOR_NONE`;
5. compare the edges of adjacent tiles directly;
6. visit only `E` and `S` to avoid checking each adjacency twice.

Do not call solver functions or use `compat`. The verifier may read
`TILESET[a].edge[d]` and `TILESET[b].edge[opposite(d)]` directly.

## 5. Public solver API

Define an API similar to the following in `include/wang/solver.h`. The names
are normative unless a concrete implementation reason requires a change.

```c
#ifndef WANG_SOLVER_H
#define WANG_SOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wang/region.h"

typedef enum {
    WANG_SOLVE_ERROR = -1,
    WANG_SOLVE_UNSAT = 0,
    WANG_SOLVE_SAT = 1
} WangSolveStatus;

enum {
    WANG_SOLVE_COLLECT_METRICS = UINT32_C(1) << 0,
    WANG_SOLVE_TRACE_FAILED_LEAVES = UINT32_C(1) << 1,
    WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT = UINT32_C(1) << 2
};

typedef struct {
    uint64_t dfs_nodes;
    uint64_t decisions;
    uint64_t backtracks;
    uint64_t failed_leaves;
    uint64_t domain_reductions;
    uint64_t propagated_arcs;
    uint64_t mrv_cells_scanned;
    uint64_t initial_trail_writes;
    uint64_t search_trail_writes;
    size_t trail_peak;
    size_t trail_capacity_peak;
    size_t trail_bytes_peak;
    size_t queue_peak;
    size_t dfs_stack_capacity_peak;
    size_t dfs_stack_bytes_peak;
    size_t max_depth;
} WangSolverMetrics;

typedef struct {
    uint32_t flags;

    /* Required only with WANG_SOLVE_TRACE_FAILED_LEAVES. */
    const char *failed_leaf_path;
    size_t failed_leaf_capacity;
} WangSolverOptions;

typedef struct {
    /*
     * Dense row-major snapshot, one uint32_t per RegionCell.
     * SAT: all active domains are singletons.
     * UNSAT with WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT: best failed leaf
     * according to the rule in Section 10. Without the flag: NULL and a
     * zero count.
     */
    uint32_t *domains;
    size_t domain_count;

    /* SIZE_MAX for SAT; index of the zero-domain cell for UNSAT. */
    size_t conflict_cell;

    size_t resolved_count;
    size_t decision_depth;

    size_t traced_leaf_count;
    bool trace_truncated;

    /* All zero unless WANG_SOLVE_COLLECT_METRICS is requested. */
    WangSolverMetrics metrics;
} WangSolveResult;

WangSolveStatus wang_solve_serial(
    const Region *region,
    const WangSolverOptions *options,
    WangSolveResult *out_result
);

void wang_solve_result_destroy(WangSolveResult *result);

#endif /* WANG_SOLVER_H */
```

Contract:

- `options == NULL` is equivalent to zero flags;
- unknown flags are an error;
- `out_result` must be zero-initialized or previously destroyed;
- on `SAT` and `UNSAT`, the caller owns `out_result->domains`;
- on `ERROR`, the output remains in a fully destroyed state;
- `wang_solve_result_destroy()` accepts `NULL`, frees the snapshot, and zeros
  every field;
- the solver does not create a file unless the trace flag is present;
- with the trace flag, a nonempty path and capacity greater than zero are
  required;
- the trace path is an explicit output: the solver creates or truncates it.

## 6. Private solver structures

Everything that follows must remain in `solver_serial.c`.

```c
typedef struct {
    uint32_t edge_mask[DIR_COUNT][COLOR_COUNT];
    uint32_t compat[DIR_COUNT][TILE_COUNT];
} SolverTables;

typedef struct {
    size_t cell_index;
    uint32_t old_domain;
} TrailEntry;

typedef struct {
    size_t cell_index;
    uint32_t candidates;
    size_t entry_mark;
} SearchFrame;

typedef struct {
    const Region *region;
    SolverTables tables;

    uint32_t *domains;
    uint8_t *neighbor_mask;
    size_t cell_count;
    size_t active_count;
    size_t resolved_count;

    TrailEntry *trail;
    size_t trail_count;
    size_t trail_capacity;

    size_t *queue;
    size_t queue_capacity;

    uint32_t *best_snapshot;
    size_t best_resolved_count;
    size_t best_depth;
    size_t best_conflict_cell;
    bool has_best_leaf;

    bool collect_metrics;
    bool capture_unsat_snapshot;
    WangSolverMetrics metrics;

    /* private mmap writer, if enabled */
} SolverState;
```

The conceptual separation to preserve is:

- `SolverTables`: immutable derived data that may be shared in the future;
- `SolverState`: mutable data private to an individual branch/worker.

Do not use skip lists. The trail is a contiguous stack, and the initial MRV is
a cache-friendly linear scan.

## 7. Private compatibility tables

Build `edge_mask` first:

```c
edge_mask[d][color] |= UINT32_C(1) << tile_id;
```

Then derive:

```c
compat[d][tile_id] =
    edge_mask[opposite(d)][TILESET[tile_id].edge[d]];
```

Meaning:

- `edge_mask[d][c]`: tiles with color `c` on edge `d`;
- `compat[d][t]`: tiles allowed in the cell located in direction `d` from a
  cell containing `t`.

The tables are caches, not sources of truth. Add an indirect exhaustive test
or a private assertion/test function that establishes the following
equivalence for every `d`, `a`, and `b`:

```text
bit b in compat[d][a]  <=>  wang_tiles_match(&TILESET[a], d, &TILESET[b])
```

Do not export these tables in the public header.

## 8. State initialization

Validate the `Region` before allocating the state:

- nonnull region pointer and `cells`;
- positive dimensions;
- overflow-free product and allocation sizes;
- colors equal to `COLOR_NONE` or satisfying `< COLOR_COUNT`;
- no boundary color on inactive cells;
- no boundary color on an edge with an active neighbor.

For each cell:

1. if inactive, set `domain = 0` and `neighbor_mask = 0`;
2. if active, start from `WANG_DOMAIN_ALL`;
3. build the four bits of `neighbor_mask`;
4. for each exposed edge with color `c != COLOR_NONE`:

```c
domain &= tables.edge_mask[dir][c];
```

5. count the cell in `resolved_count` if the domain is a singleton;
6. if the domain becomes zero, there is a failed leaf at depth zero.

A valid region with no active cells is `SAT`: its snapshot contains only
zeros, but no conflict exists because no cell is active.

After applying the boundary masks, perform initial propagation to a fixed
point. Recording changes in the trail and then setting `trail_count = 0`
without a rollback is allowed: the resulting state becomes the DFS root.

## 9. Undo trail

Every actual domain change must:

1. ensure capacity in the `trail` vector;
2. append `{ cell_index, old_domain }`;
3. update `resolved_count` by comparing the old and new cardinalities;
4. write the new domain;
5. update metrics and peaks, if enabled.

Before trying a candidate:

```c
const size_t mark = state->trail_count;
```

Rollback walks the trail backward to `mark`. It must also update
`resolved_count` by comparing the current domain with the restored domain.

Recording the same cell multiple times is correct: reverse rollback recreates
every intermediate state exactly. Do not deduplicate the trail.

Do not copy the entire domain array at every decision.

## 10. Propagation and leaf snapshots

Use a contiguous queue of indices. The baseline allows duplicates: add an
`in_queue` mechanism only if metrics reveal a real problem.

When the domain of cell `i` changes, for each active neighbor `j` in direction
`d`:

```c
uint32_t supported = 0;
uint32_t candidates = domains[i];

while (candidates != 0) {
    const TileId tile = first_set_bit(candidates);
    supported |= tables.compat[d][tile];
    candidates &= candidates - 1;
}

const uint32_t new_domain = domains[j] & supported;
```

If the domain changes, save it to the trail and enqueue `j`. If it becomes
zero, keep the zero in the state, record the leaf before rollback, and report
a conflict.

The solver must always retain the metadata for the best leaf, even when the
file trace and dense snapshot are disabled. The deterministic preference order
is:

1. higher `resolved_count`;
2. at equal counts, greater decision depth;
3. if still tied, retain the first one encountered.

Always save `conflict_cell`, depth, and the number of resolved cells. Only with
`WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT`, allocate `best_snapshot` for the first
best leaf and copy the entire domain array into it. The mmap trace and snapshot
capture remain independent options.

This UNSAT snapshot is diagnostic and renderable; it is not a formal proof of
unsatisfiability.

## 11. Iterative DFS and MRV

The baseline must be deterministic:

- choose an active cell with a nonsingleton domain of minimum cardinality;
- on ties, choose the smallest row-major index;
- try `TileId` values in ascending order;
- visit neighbors in `N`, `E`, `S`, `W` order.

A linear MRV scan is intentional: the domains are contiguous, and reading them
is cache-friendly. Do not maintain a mutable ordered structure. If profiling
shows that MRV dominates execution time, the first alternative to try is a
24-bucket structure, not a skip list.

Traversal uses an explicit heap-allocated stack, with at most one frame per
active cell. This prevents a large region from exhausting the process stack. A
frame contains the node's MRV cell, the candidates not yet tried, and the trail
marker preceding the branch that opened the node.

DFS outline:

```text
count the root node
if everything is resolved: SAT
push the root MRV frame

while the stack is not empty:
    frame = top of stack

    if no candidates remain:
        pop the frame
        if it was the root: UNSAT
        rollback(frame.entry_mark)
        count the parent branch backtrack
        continue

    extract the smallest TileId from the frame candidates
    mark = trail_count
    restrict the cell to the singleton and propagate

    if there is a conflict:
        record the leaf
        rollback(mark) and count the backtrack
    else if everything is resolved:
        SAT without rollback
    else:
        count the new node
        push { new MRV cell, its domain, mark }
```

The depth of a branch equals the number of frames while trying the candidate.
Traversal order, metrics, and rollback semantics remain the same as in the
recursive formulation.

Allocation or writer failures are `ERROR`, not `UNSAT`.

Do not implement memoization in the baseline. With deterministic ordering and
monotonic domains, reaching the same global state twice is unlikely, while a
hash table would introduce extra memory use and random accesses.

## 12. Mandatory SAT-result verification

Before publishing `SAT`:

1. temporarily build a `TileId[cell_count]` array;
2. write `TILE_NONE` to inactive cells;
3. extract the only bit from each active singleton domain;
4. call `wang_verify_tiling()`;
5. accept `SAT` only if the result is `WANG_VERIFY_VALID`.

If the verifier rejects solver output, return `ERROR`: this is an internal bug,
not `UNSAT`.

Only after this check should the final domains be copied into the public
snapshot.

## 13. Optional metrics

Update the public counters only when `WANG_SOLVE_COLLECT_METRICS` is present.
Otherwise, every field must be zero.

Definitions that must remain stable in tests and documentation:

- `dfs_nodes`: search states visited, including the root;
- `decisions`: singleton candidates actually tried;
- `backtracks`: failed candidates that were restored;
- `failed_leaves`: terminal conflicts observed;
- `domain_reductions`: writes that actually narrow a domain;
- `propagated_arcs`: processed cell-neighbor arcs;
- `mrv_cells_scanned`: active cells inspected by MRV scans;
- `initial_trail_writes`: entries actually added to the trail during initial
  propagation;
- `search_trail_writes`: entries actually added to the trail during DFS;
- `trail_peak`: maximum number of entries simultaneously present in the trail;
- `trail_capacity_peak`: maximum allocated trail capacity in entries;
- `trail_bytes_peak`: bytes corresponding to the maximum allocated trail
  capacity;
- `queue_peak`: maximum number of not-yet-popped indices simultaneously
  present in a propagation queue;
- `dfs_stack_capacity_peak`: maximum allocated DFS stack capacity in frames;
- `dfs_stack_bytes_peak`: bytes corresponding to the maximum allocated DFS
  stack capacity;
- `max_depth`: maximum DFS depth reached.

Time is not part of `SolverMetrics`: benchmarks and callers measure it
externally.

## 14. Binary mmap leaf trace

### 14.1 Semantics

The trace is optional. If enabled, it contains every failed leaf encountered
before rollback, up to `failed_leaf_capacity`.

The file is diagnostic, not a formal UNSAT certificate. It may contain leaves
even when the final result is `SAT`, because the solver may have failed on
earlier branches.

The exact number of leaves is not known before the search. The following is
known instead:

```text
record_size = aligned_record_prefix + cell_count * sizeof(uint32_t)
allocated_file_size = file_header_size
                    + failed_leaf_capacity * record_size
```

The file is therefore preallocated to the requested capacity and truncated to
the number of records actually written on completion.

### 14.2 Version 1 format

Do not write C structs directly without checking their layout and size. The v1
format is little-endian and has a 64-byte header.

```c
typedef struct {
    char magic[8];             /* "W23LEAF\0" */
    uint32_t version;          /* 1 */
    uint32_t header_size;      /* 64 */
    uint32_t width;
    uint32_t height;
    uint32_t tile_count;       /* 23 */
    uint32_t flags;            /* bit 0: trace_truncated */
    uint64_t cell_count;
    uint64_t record_size;
    uint64_t record_capacity;
    uint64_t record_count;
} FailedLeafFileHeader;
```

The layout above is schematic. The current implementation writes each field at
an explicit offset with little-endian helpers and does not depend on padding or
`sizeof(FailedLeafFileHeader)`. If a future change writes a C struct directly,
add at least:

```c
_Static_assert(sizeof(FailedLeafFileHeader) == 64,
               "failed-leaf header must be 64 bytes");
```

The `_Static_assert` checks layout, not endianness. Write fields with small
little-endian helpers, or explicitly reject a non-little-endian platform at
compile time or runtime; do not silently produce a file with a byte order that
differs from the declared one.

Each record begins with 32 bytes:

```c
typedef struct {
    uint64_t leaf_index;
    uint64_t conflict_cell;
    uint64_t decision_depth;
    uint64_t resolved_count;
} FailedLeafRecordHeader;
```

Exactly `cell_count` `uint32_t` values follow in row-major order. The record is
completed with zero padding up to the next multiple of 8:

```text
raw_record_size = 32 + 4 * cell_count
record_size = align_up(raw_record_size, 8)
```

Check every overflow before `open`, `ftruncate`, and `mmap`.

### 14.3 Writer lifecycle

The writer is isolated in `src/solver/failed_leaf_trace.c`, with a header
private to the solver. Do not make it part of the general serialization API.

Sequence:

1. open `failed_leaf_path` with `O_RDWR | O_CREAT | O_TRUNC`, mode `0666`;
2. calculate the maximum size with checked arithmetic;
3. `ftruncate(fd, allocated_size)`;
4. `mmap(..., PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)`;
5. initialize the header with `record_count = 0`;
6. for every failed leaf to record, use `memcpy` for the record header and
   domains;
7. if a leaf arrives after capacity is exhausted, do not write it and set
   `trace_truncated = true`;
8. at the end, update the header, `record_count`, and flags;
9. call `msync` on the used portion;
10. call `munmap` on the entire mapping;
11. truncate to:

```text
64 + record_count * record_size
```

12. close the descriptor.

Do not shrink the file with `ftruncate` while the mapping is still in use.

If setup, writing, synchronization, or finalization fails, clean up all
resources and return `WANG_SOLVE_ERROR`. The public result must remain
destroyed. The partial file may remain for diagnostics, but it must not be
presented as a complete trace.

### 14.4 Future multithreading

Do not use locks or atomics today. In the future, do not let multiple workers
write to the same resizable mapping: assign a private segment or file to each
worker and merge the indices after the search. This note does not authorize
any OpenMP implementation in this block.

## 15. Error handling and ownership

Every allocation must be preceded by an overflow check. On any error:

- roll back and clean up the private state;
- safely finalize or close the writer if it was opened;
- `free` domains, neighbor mask, trail, queue, snapshot, and temporary arrays;
- zero the public output;
- return `WANG_SOLVE_ERROR`.

Construct the result in a local variable and transfer it to `out_result` only
on completion. Do not publish fields progressively.

The solver must reject an undestroyed `out_result` to prevent leaks or silent
overwrites.

## 16. Required tests

### 16.1 Verifier

Test at least:

- null arguments and incorrect length;
- `TILE_NONE` on an active cell;
- out-of-range `TileId`;
- a tile assigned to an inactive cell;
- a single cell with all correct boundaries;
- a boundary mismatch in each direction;
- two horizontally compatible and incompatible cells;
- two vertically compatible and incompatible cells;
- a region with an inactive cell/hole;
- a manually corrupted `Region`: invalid color or boundary on an internal
  edge.

### 16.2 Solver

Test at least:

- invalid options and output objects;
- a region with no active cells -> `SAT`;
- a single forced cell -> `SAT`, with the correct singleton domain;
- a single cell with an impossible boundary -> `UNSAT`, no default snapshot,
  and a valid `conflict_cell`;
- the same region with `WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT` -> snapshot present,
  zero conflict domain, and consistent metadata;
- small SAT and UNSAT regions compared with an independent brute force in the
  tests;
- every SAT result accepted by `wang_verify_tiling()`;
- determinism: two executions produce the same status and snapshot;
- metrics all zero without the flag and consistent with the flag;
- rollback after multiple levels, including multiple changes to the same cell;
- an unconstrained region exceeding ten thousand decision levels, to exercise
  the explicit DFS stack without depending on the process stack;
- unmodified input and `Region`;
- output left in a destroyed state after an error, and idempotent destruction.

The brute-force test oracle must enumerate `TileId` values directly on very
small regions and use the verifier; it must not call the solver or its caches.

### 16.3 mmap trace

Use a temporary path owned by the test. Verify:

- no file created without the flag;
- error with a null/empty path or zero capacity;
- magic, version, dimensions, and `record_size`;
- at least one leaf written for an UNSAT case;
- `record_count == out_result.traced_leaf_count`;
- exact final size:

```text
64 + record_count * record_size
```

- the domain of the record's `conflict_cell` is zero;
- capacity reached: additional leaves are not written and
  `trace_truncated == true`;
- file readable after `munmap`/close;
- correct cleanup on a writer error.

Do not leave temporary files after the tests.

### 16.4 Yang-Zhang integration

After the small tests are stable:

- build the minimal `{0,0,0}` formula with `yang_zhang_build()` and compare the
  solver result with the expected CM1-in-3 semantics (`UNSAT`);
- try the three-variable instance documented in the builder, which admits the
  assignment `(0,0,1)`, and require a verified `SAT` tiling;
- retain a regression in which the only true signal enters the first row of a
  clause;
- enumerate all 1,701 canonical formulas with up to three variables and
  compare every result with the Boolean oracle;
- if these tests are too expensive for `make check`, mark them as a separate
  integration target instead of weakening the unit tests.

## 17. Checks and profiling

Run during development:

```sh
make clean
make check
make valgrind-check
make cachegrind-check
```

If Valgrind is not installed, `make check` remains the mandatory gate.

Use metrics and Cachegrind to answer these questions before optimizing:

- how much of the work is the MRV scan;
- how many duplicates enter the queue;
- how large the trail grows;
- how many reductions each decision produces;
- how much copying each leaf into the trace costs.

Optimizations allowed only after measurement:

- 24 MRV buckets;
- a bitset of cells for each bucket;
- propagation-queue deduplication;
- traces of deltas or decision paths instead of complete snapshots.

Do not introduce skip lists or global memoization as the first optimization.

## 18. Recommended implementation order

1. Implement `verify.h`, `verify_tiling.c`, and `test_verify.c`.
2. Define `solver.h` and test result lifetime/validation.
3. Implement and verify the private `edge_mask` and `compat` tables.
4. Initialize domains and the neighbor mask from boundaries.
5. Implement the trail, rollback, and propagation queue.
6. Implement DFS/MRV with an explicit stack and no trace.
7. Independently verify every SAT result.
8. Implement selection and return of the best UNSAT leaf.
9. Add metrics behind a flag.
10. Implement the mmap writer and a minimal format parser in the tests.
11. Add deterministic regressions and brute-force comparisons.
12. Run checks, Valgrind, and Cachegrind.
13. Only after completing the work, update the README from "not implemented"
    to the status actually reached.

## 19. Definition of done

The work package is complete when:

- the verifier and solver have documented public contracts;
- the solver returns `SAT`, `UNSAT`, or `ERROR` unambiguously;
- `SAT` contains singleton domains and passes the independent verifier;
- `UNSAT` always contains a renderable failed leaf;
- the optional mmap trace contains valid records, respects the cap, and is
  truncated to its actual size;
- metrics remain zero or are populated according to the flag;
- no private cache becomes a second source of truth;
- the solver remains independent of builder geometry and metadata;
- `make check` passes without warnings;
- tests do not leak memory or leave temporary files.
