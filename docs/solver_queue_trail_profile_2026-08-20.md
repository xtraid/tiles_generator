---
layout: page
title: Optimized solver queue and trail profile
permalink: /solver_queue_trail_profile_2026-08-20/
description: Direct post-T67 queue, trail, Callgrind, and Cachegrind evidence.
---

# Optimized solver queue and trail profile — 20 August 2026

Status: T69 measurement complete; queue deduplication selected as the next
isolated performance packet.

This report measures the current `wang_solve_optimized()` after the byte-wise
support table landed in T67. It adds diagnostic counters only. It does not
deduplicate the queue, index MRV, compact the trail, add `TaskPlan`, or enable
OpenMP. The reference and optimized paths retain their existing propagation,
search, rollback, and witness semantics.

## Reproduction identity

The immutable baseline is Git commit:

```text
535012b8ca6927948014047037896e713e9e38a9
Add byte-wise support lookup to optimized solver
```

A detached temporary worktree at that commit produced the portable `-O2`
schema-v5 benchmark binary. Its identities were:

```text
ecb6e558af0bb11009a9a38a4e2269ccd70b03948daad9014c9a095c49ecdb5e  build/benchmarks/c/bench_solver
89ac064039f4b1660b844b092bbc972935e53aefddfb5a5f391023fae27c046d  include/wang/solver.h
4ea9ba456a1342165b3169823bf8a2ab21f0b942d71583c1acb6af34c215921d  src/solver/solver_serial.c
15278b0b479a7d00d8eaafabb5cb3bed52037252f5e842b331f2fa750e9b48fa  benchmarks/c/bench_solver.c
```

The measurements were recorded from the reviewed worktree before this packet's
commit. Benchmark schema v6 adds only the five counters defined below. Final
source identities are recorded after the verification gates:

```text
c87b6b90cd09632f1ffb730f10399a65ac5c89f5a6942ef87d53b82374bfe8f7  include/wang/solver.h
b56521eea0a0230dbe108a5e0640e3fcc852986cfaa70650d97a6a9f41e1df6b  src/solver/solver_serial.c
39ff014ca1e16e86b40bc425a222e241d86020e434d8b921213e761a57377909  benchmarks/c/bench_solver.c
f12ac16d8e0e667aa219d94e53a4a5c3e5d98472c0a46b58e7b238ba1ff698a0  tests/c/test_solver.c
07b0dcd2ac2bd2dbb560809a154d47b89a929a96895ff672402d15f950a9b5e1  tests/c/test_solver_differential.c
```

The six pre-existing Python changes were outside T69. Their Git blob
identities were recorded before the task and reproduced after the final
cleanup:

```text
64b765063a51ab8af6891e9e292fc74601c01a46  D  python/native/formula.py (HEAD blob)
85278d4d947a2560e0281192254c9da3f36ab093  M  tests/python/test_native_formula.py
11436052bf024cd949525b40f700272f2302af53  M  tests/python/test_pipeline.py
eb24fea1e9dfd4eb44143a82b66cdea7c6e3e5b6  ?? python/model/region.py
c2974a66d0f4ecd167ce97ad2e70a5934c1a6490  ?? python/native/_lib.py
4bff754d6a43178625b1a96cb623a76990bed4fd  ?? python/native/formula_adapter.py
```

Environment:

```text
Debian GNU/Linux 13
Linux 6.12.101+deb13-amd64 x86_64
AMD Ryzen 5 3600, 6 cores / 12 threads, boost enabled
GCC 14.2.0
Clang 19.1.7
C17, -O2 portable build; no -march=native or LTO
Valgrind/Callgrind/Cachegrind 3.24.0
```

Native timings were pinned to CPU 2. Frequency was not pinned and the host was
not isolated, so medians are descriptive host evidence rather than confidence
intervals. Metric runs are separate single executions and are not timing
samples.

## Preventive red test

The public metric expectations were added to `tests/c/test_solver.c` before
the implementation. The preventive command was:

```sh
make build/tests/c/test_solver
```

It failed with exit status 2 because `WangSolverMetrics` had no members named
`enqueue_attempts`, `duplicate_enqueue_attempts`, `queue_unique_peak`,
`initial_trail_rewrites`, or `search_trail_rewrites`. After implementation,
the same fixture fixes exact expected values for all five fields and checks
the general bounds `duplicates <= attempts`, `unique_peak <= queue_peak`, and
`rewrites <= writes`.

## Counter definitions

Counters are collected only with `WANG_SOLVE_COLLECT_METRICS`; otherwise every
field remains zero.

- `enqueue_attempts` counts every request to append a cell index to the FIFO,
  including initial active cells, branch seeds, and propagation-induced
  requests.
- `duplicate_enqueue_attempts` counts an enqueue request when that cell
  already has at least one unconsumed occurrence in the FIFO. The operation is
  still performed: this packet observes duplicates and does not remove them.
- `queue_unique_peak` is the maximum number of distinct cell indices among
  unconsumed FIFO entries. Existing `queue_peak` remains the maximum total
  number of unconsumed occurrences.
- `initial_trail_rewrites` counts a reference-path trail entry after the first
  entry for the same cell during the single initial-propagation interval. The
  optimized path records no initial trail and therefore reports zero.
- `search_trail_rewrites` counts a trail entry after the first entry for the
  same cell since the most recent DFS candidate marker. The interval includes
  the candidate's singleton restriction and its propagation. A new candidate
  attempt starts a new interval, including at a deeper DFS frame.

Queue pending multiplicity and trail intervals are metrics-only derived state.
Pending state is decremented on every pop and explicitly drained when
propagation abandons a queue after conflict or error. It never suppresses an
enqueue or changes rollback state.

## Method

The detached baseline used five metrics-disabled passes in fresh benchmark
processes for each solver/case combination. The current schema-v6 corpus used
one metrics-enabled process per solver/case; queue and semantic-work counters
were identical. Path-specific counters retain their documented differences in
support, initial trail, stack capacity, SAT copy, and table storage. Default
and opt-in snapshot modes were repeated for the root UNSAT control.

Callgrind and Cachegrind used one metrics-disabled optimized process per case.
Their totals include fixture setup, solving, mandatory witness verification,
and teardown, whereas the native solver-only timer begins after fixture
construction. Percentages therefore use the stricter whole-process
denominator.

## Metrics-disabled commit baseline

Milliseconds per solve, five passes, portable `-O2`, CPU 2:

| Case | Reference median | Optimized median | Optimized range |
| --- | ---: | ---: | ---: |
| generic forced thin SAT | 4.767 | 3.150 | 3.097--3.225 |
| generic unconstrained SAT | 181.961 | 144.483 | 142.929--150.850 |
| generic backtracking SAT | 0.033 | 0.014 | 0.014--0.018 |
| generic root UNSAT | 19.213 | 19.083 | 17.985--19.468 |
| Yang-Zhang SAT, 6 variables | 12.464 | 2.503 | 2.472--3.074 |
| Yang-Zhang UNSAT, 6 variables | 3.106 | 0.548 | 0.543--0.722 |
| Yang-Zhang SAT, 12 variables | 98.569 | 20.825 | 20.565--22.785 |
| Yang-Zhang UNSAT, 12 variables | 25.112 | 4.264 | 4.183--5.093 |

The 20.825-ms large-SAT median is within 0.6 percent of T67's 20.701-ms
measurement. The passes were not interleaved with the older T67 run, so this
is a reproducibility check, not a new optimization comparison.

## Direct queue evidence

Queue counters are identical in reference and optimized because T69 does not
change queue behavior.

| Case | Enqueue attempts | Pending duplicates | Duplicate share | Queue peak | Unique peak | Peak amplification |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| generic forced thin SAT | 98,301 | 65,532 | 66.67% | 65,533 | 32,768 | 2.000x |
| generic unconstrained SAT | 126,314 | 36,289 | 28.73% | 27,265 | 9,216 | 2.958x |
| generic backtracking SAT | 138 | 43 | 31.16% | 34 | 16 | 2.125x |
| generic root UNSAT | 0 | 0 | n/a | 0 | 0 | n/a |
| Yang-Zhang SAT, 6 variables | 77,013 | 52,905 | 68.70% | 27,791 | 9,345 | 2.974x |
| Yang-Zhang UNSAT, 6 variables | 19,832 | 14,394 | 72.58% | 7,561 | 2,560 | 2.954x |
| Yang-Zhang SAT, 12 variables | 645,444 | 445,545 | 69.03% | 228,255 | 76,247 | 2.994x |
| Yang-Zhang UNSAT, 12 variables | 159,813 | 118,571 | 74.19% | 60,715 | 20,317 | 2.988x |

The Yang-Zhang propagation cases consistently attempt about three queue
occurrences at peak for each distinct pending cell, and 68.70--74.19 percent
of all enqueue requests are already pending. This is direct evidence of
redundant queue work, not merely a large allocation watermark.

## Direct trail evidence

The table shows the reference initial interval because that path still records
it, and the optimized search trail because it is the only rollback trail that
an optimized-path compaction could improve. Reference and optimized search
counts are identical.

| Case | Reference initial writes | Initial rewrites | Initial share | Optimized search writes | Search rewrites | Search share |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| generic forced thin SAT | 65,533 | 32,766 | 50.00% | 0 | 0 | n/a |
| generic unconstrained SAT | 36,290 | 27,074 | 74.61% | 80,808 | 8,930 | 11.05% |
| generic backtracking SAT | 46 | 31 | 67.39% | 78 | 13 | 16.67% |
| generic root UNSAT | 0 | 0 | n/a | 0 | 0 | n/a |
| Yang-Zhang SAT, 6 variables | 60,290 | 50,959 | 84.52% | 7,378 | 12 | 0.163% |
| Yang-Zhang UNSAT, 6 variables | 16,265 | 13,719 | 84.35% | 1,009 | 6 | 0.595% |
| Yang-Zhang SAT, 12 variables | 510,665 | 434,444 | 85.07% | 58,532 | 24 | 0.041% |
| Yang-Zhang UNSAT, 12 variables | 135,600 | 115,309 | 85.04% | 3,898 | 6 | 0.154% |

The apparently large initial rewrite opportunity is not present in the
optimized path: T52 already removed its entire non-rollbackable initial trail.
On the optimized Yang-Zhang search trail, repeated writes between markers are
only 0.041--0.595 percent. Generic backtracking and unconstrained search reach
16.67 and 11.05 percent respectively, but neither outweighs the cross-corpus
queue evidence or the unconstrained MRV hotspot below.

## Diagnostic modes

Default and `WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT` runs of the 2,097,152-cell root
UNSAT case returned the same status, conflict cell, and zero queue/trail work
for both solvers. Snapshot mode alone returned the requested dense domains.
The C suite also exercised capped failed-leaf tracing on the backtracking
fixture while fixing the exact queue/trail counters, which covers reset after
conflicts and subsequent candidate attempts.

## Callgrind attribution

One metrics-disabled optimized execution per row:

| Case | Whole-process instructions | Dominant self function | Self share | Solver inclusive |
| --- | ---: | --- | ---: | ---: |
| generic forced thin SAT | 61,673,219 | `propagate_queue()` | 25.61% | not isolated |
| generic unconstrained SAT | 1,965,873,358 | `select_mrv_cell()` | 96.90% | 99.96% |
| Yang-Zhang SAT, 6 variables | 40,393,824 | `propagate_queue()` | 55.74% | not isolated |
| Yang-Zhang SAT, 12 variables | 336,041,260 | `propagate_queue()` | 56.35% | 93.14% |

T67 removed much of the support-union work, reducing the propagation share
from the earlier reference profile's approximately 84 percent to about 56
percent on Yang-Zhang SAT. Propagation remains the largest named self hotspot.
The unconstrained regime is different: its unchanged row-major MRV scan alone
executes 1,905,013,160 instructions.

## Cachegrind attribution

One metrics-disabled optimized execution per row, with cache and branch
simulation enabled:

| Case | Data references | D1 miss rate | LLd miss rate | Branch mispredict rate |
| --- | ---: | ---: | ---: | ---: |
| generic unconstrained SAT | 190,207,663 | 3.0% | effectively 0.0% | 3.3% |
| Yang-Zhang SAT, 6 variables | 9,295,136 | 0.5% | 0.2% | 3.7% |
| Yang-Zhang SAT, 12 variables | 75,543,537 | 0.5% | 0.1% | 3.4% |

As before T67, the dominant cost is instruction/work volume rather than a high
last-level data miss rate. Cachegrind is a deterministic cache model, not a
native hardware-counter measurement.

## Decision

Queue deduplication is the next isolated performance packet. It has direct,
substantial evidence across forced propagation and every medium/large
Yang-Zhang case: 66.67--74.19 percent duplicate enqueue requests and peak FIFO
amplification close to 3x. The packet must retain propagation correctness and
deterministic solver contracts, measure its own index bytes, and compare the
full corpus before retention. T69 intentionally does not assume the observed
duplicate count converts one-for-one into time saved.

An MRV index remains the next distinct candidate for weakly constrained deep
search. The unconstrained case spends 96.90 percent of whole-process
instructions in linear MRV and scans 43,897,478 cells, while Yang-Zhang uses
only 1--241,688 scans. It should not displace the broader queue packet or be
combined with it.

Trail compaction is not justified now. The optimized path already omits the
only high-rewrite interval, and its Yang-Zhang search rewrite shares are below
0.6 percent. The larger generic shares can be reconsidered only if a later
profile shows trail operations dominating after queue and MRV work.

## Limitations

- Metrics-only pending and interval arrays add diagnostic memory and work;
  timing and profiler runs therefore disable metrics.
- A pending duplicate counter establishes redundant scheduling, not the cost
  of an `in_queue` test or the net effect on propagation locality.
- `queue_unique_peak` measures simultaneous distinct pending cells, not the
  minimum capacity of a future ring-buffer implementation.
- Trail rewrite counters identify entries removable within the stated marker
  intervals; they do not model every possible trail representation.
- Native frequency was not fixed, profiling used one simulated execution per
  case, and process totals include harness work.
- No conclusion about `TaskPlan` or OpenMP follows from these serial profiles.

## Reproduction commands

Baseline worktree and native timing:

```sh
git worktree add --detach /tmp/tiling-foundry-t69-baseline \
  535012b8ca6927948014047037896e713e9e38a9
make -C /tmp/tiling-foundry-t69-baseline build/benchmarks/c/bench_solver
cd /tmp/tiling-foundry-t69-baseline
taskset -c 2 build/benchmarks/c/bench_solver \
  --case yang_zhang_sat_large_solver --solver optimized
```

Direct metrics and diagnostic control:

```sh
build/benchmarks/c/bench_solver \
  --case yang_zhang_sat_large_solver --solver optimized \
  --iterations 1 --metrics
build/benchmarks/c/bench_solver \
  --case generic_root_unsat --solver optimized \
  --iterations 1 --metrics --capture-unsat
```

Profilers:

```sh
valgrind --tool=callgrind --error-exitcode=1 \
  --callgrind-out-file=/tmp/t69-callgrind.out \
  build/benchmarks/c/bench_solver \
  --case yang_zhang_sat_large_solver --solver optimized --iterations 1
callgrind_annotate --inclusive=yes /tmp/t69-callgrind.out

valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes \
  --error-exitcode=1 --cachegrind-out-file=/tmp/t69-cachegrind.out \
  build/benchmarks/c/bench_solver \
  --case yang_zhang_sat_large_solver --solver optimized --iterations 1
```

## Verification status

The final implementation passed `make check`, strict GCC and Clang builds,
ASan/UBSan/LeakSanitizer, GCC static analysis, the complete Memcheck target,
and the complete Cachegrind target. The first sanitizer attempt stopped at
LeakSanitizer's known ptrace limitation; the same target completed outside the
ptrace sandbox without a finding. Targeted metrics-disabled Callgrind and
Cachegrind executions also completed for the optimized profile cases.

Build, profiler, and temporary baseline artifacts were removed after recording
the evidence. The six pre-existing Python worktree changes were checked by Git
blob identity before and after T69 and were not modified.
