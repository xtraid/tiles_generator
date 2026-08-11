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
    WANG_SOLVE_TRACE_FAILED_LEAVES = UINT32_C(1) << 1
};

typedef struct {
    uint64_t dfs_nodes;
    uint64_t decisions;
    uint64_t backtracks;
    uint64_t failed_leaves;
    uint64_t domain_reductions;
    uint64_t propagated_arcs;
    uint64_t mrv_cells_scanned;
    size_t trail_peak;
    size_t queue_peak;
    size_t max_depth;
} WangSolverMetrics;

typedef struct {
    uint32_t flags;

    /*
     * Required only with WANG_SOLVE_TRACE_FAILED_LEAVES. Incomplete trace
     * output is removed if the writer fails after creating the path.
     */
    const char *failed_leaf_path;
    size_t failed_leaf_capacity;
} WangSolverOptions;

typedef struct {
    /*
     * Dense row-major domain snapshot, one uint32_t per RegionCell.
     * SAT contains singleton domains for every active cell. UNSAT contains
     * the best failed leaf found by the deterministic search.
     */
    uint32_t *domains;
    size_t domain_count;

    /* SIZE_MAX for SAT; the zero-domain active cell for UNSAT. */
    size_t conflict_cell;
    size_t resolved_count;
    size_t decision_depth;

    size_t traced_leaf_count;
    bool trace_truncated;

    /* Zeroed unless WANG_SOLVE_COLLECT_METRICS was requested. */
    WangSolverMetrics metrics;
} WangSolveResult;

/*
 * Solve a finite Wang region using the canonical TILESET.
 *
 * options may be NULL. out_result must be zero-initialized or destroyed.
 * On SAT or UNSAT, the caller owns out_result->domains. On ERROR, out_result
 * remains destroyed.
 */
WangSolveStatus wang_solve_serial(
    const Region *region,
    const WangSolverOptions *options,
    WangSolveResult *out_result
);

/* Release the owned snapshot and reset every field. Accepts NULL. */
void wang_solve_result_destroy(WangSolveResult *result);

#endif /* WANG_SOLVER_H */
