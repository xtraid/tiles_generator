#include "wang/solver.h"

#include "failed_leaf_trace.h"
#include "wang/tile.h"
#include "wang/verify.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WANG_DOMAIN_ALL \
    ((UINT32_C(1) << TILE_COUNT) - UINT32_C(1))

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
    /* Trail position before the parent branch entered this node. */
    size_t entry_mark;
} SearchFrame;

typedef enum {
    SEARCH_STACK_FIXED,
    SEARCH_STACK_DYNAMIC
} SearchStackMode;

typedef struct {
    SearchStackMode stack_mode;
    bool record_initial_trail;
} SolverMechanisms;

typedef enum {
    TRAIL_PHASE_INITIAL,
    TRAIL_PHASE_SEARCH
} TrailPhase;

typedef struct {
    SearchFrame *frames;
    size_t count;
    size_t capacity;
    size_t limit;
    size_t allocated_bytes;
} SearchStack;

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
    TrailPhase trail_phase;
    bool record_trail;

    size_t *queue;
    size_t queue_count;
    size_t queue_capacity;

    uint32_t *best_snapshot;
    size_t best_resolved_count;
    size_t best_depth;
    size_t best_conflict_cell;
    bool has_best_leaf;

    bool collect_metrics;
    bool capture_unsat_snapshot;
    WangSolverMetrics metrics;
    FailedLeafWriter writer;
} SolverState;

typedef enum {
    PROPAGATE_ERROR = -1,
    PROPAGATE_CONFLICT = 0,
    PROPAGATE_OK = 1
} PropagateStatus;

static bool checked_mul_size(size_t a, size_t b, size_t *out)
{
    if (out == NULL || (a != 0 && b > SIZE_MAX / a)) {
        return false;
    }
    *out = a * b;
    return true;
}

static unsigned domain_popcount(uint32_t domain)
{
    unsigned count = 0;
    while (domain != 0) {
        domain &= domain - UINT32_C(1);
        ++count;
    }
    return count;
}

static bool domain_is_singleton(uint32_t domain)
{
    return domain != 0 &&
        (domain & (domain - UINT32_C(1))) == 0;
}

static TileId first_set_tile(uint32_t domain)
{
    TileId tile = 0;
    while ((domain & UINT32_C(1)) == 0) {
        domain >>= 1;
        ++tile;
    }
    return tile;
}

static bool metrics_are_zero(const WangSolverMetrics *metrics)
{
    return metrics->dfs_nodes == 0 &&
        metrics->decisions == 0 &&
        metrics->backtracks == 0 &&
        metrics->failed_leaves == 0 &&
        metrics->domain_reductions == 0 &&
        metrics->propagated_arcs == 0 &&
        metrics->mrv_cells_scanned == 0 &&
        metrics->initial_trail_writes == 0 &&
        metrics->search_trail_writes == 0 &&
        metrics->trail_peak == 0 &&
        metrics->trail_capacity_peak == 0 &&
        metrics->trail_bytes_peak == 0 &&
        metrics->queue_peak == 0 &&
        metrics->dfs_stack_capacity_peak == 0 &&
        metrics->dfs_stack_bytes_peak == 0 &&
        metrics->max_depth == 0;
}

static bool result_is_destroyed(const WangSolveResult *result)
{
    return result != NULL &&
        result->domains == NULL &&
        result->domain_count == 0 &&
        result->conflict_cell == 0 &&
        result->resolved_count == 0 &&
        result->decision_depth == 0 &&
        result->traced_leaf_count == 0 &&
        !result->trace_truncated &&
        metrics_are_zero(&result->metrics);
}

static void build_solver_tables(SolverTables *tables)
{
    memset(tables, 0, sizeof(*tables));

    for (TileId tile = 0; tile < TILE_COUNT; ++tile) {
        for (Dir dir = N; dir < DIR_COUNT; ++dir) {
            const ColorId color = TILESET[tile].edge[dir];
            tables->edge_mask[dir][color] |= UINT32_C(1) << tile;
        }
    }

    for (TileId tile = 0; tile < TILE_COUNT; ++tile) {
        for (Dir dir = N; dir < DIR_COUNT; ++dir) {
            const ColorId color = TILESET[tile].edge[dir];
            tables->compat[dir][tile] =
                tables->edge_mask[opposite(dir)][color];
        }
    }
}

static bool solver_tables_are_valid(const SolverTables *tables)
{
    for (TileId a = 0; a < TILE_COUNT; ++a) {
        for (TileId b = 0; b < TILE_COUNT; ++b) {
            for (Dir dir = N; dir < DIR_COUNT; ++dir) {
                const bool cached =
                    (tables->compat[dir][a] & (UINT32_C(1) << b)) != 0;
                if (cached != wang_tiles_match(&TILESET[a], dir, &TILESET[b])) {
                    return false;
                }
            }
        }
    }
    return true;
}

static void solver_state_destroy(SolverState *state)
{
    if (state == NULL) {
        return;
    }

    if (state->writer.active) {
        (void)failed_leaf_writer_finish(&state->writer);
    }
    free(state->domains);
    free(state->neighbor_mask);
    free(state->trail);
    free(state->queue);
    free(state->best_snapshot);
    memset(state, 0, sizeof(*state));
    state->writer.fd = -1;
}

static bool ensure_trail_capacity(SolverState *state, size_t needed)
{
    if (needed <= state->trail_capacity) {
        return true;
    }

    size_t capacity = state->trail_capacity == 0 ? 64 : state->trail_capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    size_t bytes;
    if (!checked_mul_size(capacity, sizeof(*state->trail), &bytes)) {
        return false;
    }

    TrailEntry *resized = realloc(state->trail, bytes);
    if (resized == NULL) {
        return false;
    }

    state->trail = resized;
    state->trail_capacity = capacity;
    if (state->collect_metrics &&
        capacity > state->metrics.trail_capacity_peak) {
        state->metrics.trail_capacity_peak = capacity;
        state->metrics.trail_bytes_peak = bytes;
    }
    return true;
}

static bool ensure_queue_capacity(SolverState *state, size_t needed)
{
    if (needed <= state->queue_capacity) {
        return true;
    }

    size_t capacity = state->queue_capacity == 0 ? 64 : state->queue_capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    size_t bytes;
    if (!checked_mul_size(capacity, sizeof(*state->queue), &bytes)) {
        return false;
    }

    size_t *resized = realloc(state->queue, bytes);
    if (resized == NULL) {
        return false;
    }

    state->queue = resized;
    state->queue_capacity = capacity;
    return true;
}

static bool ensure_best_snapshot(SolverState *state)
{
    if (state->best_snapshot != NULL) {
        return true;
    }

    size_t bytes;
    if (!checked_mul_size(
            state->cell_count,
            sizeof(*state->best_snapshot),
            &bytes
        )) {
        return false;
    }

    state->best_snapshot = malloc(bytes);
    return state->best_snapshot != NULL;
}

static bool queue_push(SolverState *state, size_t cell_index)
{
    if (state->queue_count == SIZE_MAX) {
        return false;
    }
    if (!ensure_queue_capacity(state, state->queue_count + 1)) {
        return false;
    }

    state->queue[state->queue_count++] = cell_index;
    return true;
}

static void note_queue_occupancy(SolverState *state, size_t occupancy)
{
    if (state->collect_metrics && occupancy > state->metrics.queue_peak) {
        state->metrics.queue_peak = occupancy;
    }
}

static bool restrict_domain(
    SolverState *state,
    size_t cell_index,
    uint32_t new_domain
)
{
    const uint32_t old_domain = state->domains[cell_index];
    if (old_domain == new_domain) {
        return true;
    }

    if (state->record_trail) {
        if (state->trail_count == SIZE_MAX) {
            return false;
        }
        if (!ensure_trail_capacity(state, state->trail_count + 1)) {
            return false;
        }

        state->trail[state->trail_count++] = (TrailEntry) {
            .cell_index = cell_index,
            .old_domain = old_domain,
        };
    }

    if (domain_is_singleton(old_domain) &&
        !domain_is_singleton(new_domain)) {
        --state->resolved_count;
    } else if (!domain_is_singleton(old_domain) &&
               domain_is_singleton(new_domain)) {
        ++state->resolved_count;
    }

    state->domains[cell_index] = new_domain;
    if (state->collect_metrics) {
        ++state->metrics.domain_reductions;
        if (state->record_trail) {
            if (state->trail_phase == TRAIL_PHASE_INITIAL) {
                ++state->metrics.initial_trail_writes;
            } else {
                ++state->metrics.search_trail_writes;
            }
        }
        if (state->trail_count > state->metrics.trail_peak) {
            state->metrics.trail_peak = state->trail_count;
        }
    }
    return true;
}

static void rollback_to(SolverState *state, size_t mark)
{
    while (state->trail_count > mark) {
        const TrailEntry entry = state->trail[--state->trail_count];
        const uint32_t current = state->domains[entry.cell_index];

        if (domain_is_singleton(current) &&
            !domain_is_singleton(entry.old_domain)) {
            --state->resolved_count;
        } else if (!domain_is_singleton(current) &&
                   domain_is_singleton(entry.old_domain)) {
            ++state->resolved_count;
        }

        state->domains[entry.cell_index] = entry.old_domain;
    }
}

static size_t neighbor_index(
    const SolverState *state,
    size_t cell_index,
    Dir dir
)
{
    const size_t width = (size_t)state->region->width;
    switch (dir) {
    case N:
        return cell_index - width;
    case E:
        return cell_index + 1;
    case S:
        return cell_index + width;
    case W:
        return cell_index - 1;
    case DIR_COUNT:
        break;
    }
    return SIZE_MAX;
}

static PropagateStatus propagate_queue(
    SolverState *state,
    size_t *out_conflict_cell
)
{
    size_t head = 0;
    note_queue_occupancy(state, state->queue_count);

    while (head < state->queue_count) {
        const size_t cell_index = state->queue[head++];
        const uint32_t domain = state->domains[cell_index];

        if (domain == 0) {
            *out_conflict_cell = cell_index;
            state->queue_count = 0;
            return PROPAGATE_CONFLICT;
        }

        for (Dir dir = N; dir < DIR_COUNT; ++dir) {
            if ((state->neighbor_mask[cell_index] &
                 (uint8_t)(UINT8_C(1) << dir)) == 0) {
                continue;
            }

            if (state->collect_metrics) {
                ++state->metrics.propagated_arcs;
            }

            uint32_t supported = 0;
            uint32_t candidates = domain;
            while (candidates != 0) {
                const TileId tile = first_set_tile(candidates);
                supported |= state->tables.compat[dir][tile];
                candidates &= candidates - UINT32_C(1);
            }

            const size_t adjacent = neighbor_index(state, cell_index, dir);
            const uint32_t old_domain = state->domains[adjacent];
            const uint32_t new_domain = old_domain & supported;
            if (new_domain == old_domain) {
                continue;
            }

            if (!restrict_domain(state, adjacent, new_domain)) {
                state->queue_count = 0;
                return PROPAGATE_ERROR;
            }
            if (new_domain == 0) {
                *out_conflict_cell = adjacent;
                state->queue_count = 0;
                return PROPAGATE_CONFLICT;
            }
            if (!queue_push(state, adjacent)) {
                state->queue_count = 0;
                return PROPAGATE_ERROR;
            }
            note_queue_occupancy(state, state->queue_count - head);
        }
    }

    state->queue_count = 0;
    return PROPAGATE_OK;
}

static PropagateStatus propagate_from_cell(
    SolverState *state,
    size_t cell_index,
    size_t *out_conflict_cell
)
{
    state->queue_count = 0;
    if (!queue_push(state, cell_index)) {
        return PROPAGATE_ERROR;
    }
    return propagate_queue(state, out_conflict_cell);
}

static PropagateStatus propagate_initial(
    SolverState *state,
    size_t *out_conflict_cell
)
{
    state->queue_count = 0;
    for (size_t i = 0; i < state->cell_count; ++i) {
        if (state->region->cells[i].active && !queue_push(state, i)) {
            state->queue_count = 0;
            return PROPAGATE_ERROR;
        }
    }
    return propagate_queue(state, out_conflict_cell);
}

static bool record_failed_leaf(
    SolverState *state,
    size_t conflict_cell,
    size_t depth
)
{
    if (state->collect_metrics) {
        ++state->metrics.failed_leaves;
    }

    const bool better = !state->has_best_leaf ||
        state->resolved_count > state->best_resolved_count ||
        (state->resolved_count == state->best_resolved_count &&
         depth > state->best_depth);

    if (better) {
        if (state->capture_unsat_snapshot) {
            if (!ensure_best_snapshot(state)) {
                return false;
            }
            memcpy(
                state->best_snapshot,
                state->domains,
                state->cell_count * sizeof(*state->domains)
            );
        }
        state->best_resolved_count = state->resolved_count;
        state->best_depth = depth;
        state->best_conflict_cell = conflict_cell;
        state->has_best_leaf = true;
    }

    return failed_leaf_writer_write(
        &state->writer,
        state->domains,
        state->cell_count,
        conflict_cell,
        depth,
        state->resolved_count
    );
}

static size_t select_mrv_cell(SolverState *state)
{
    size_t selected = SIZE_MAX;
    unsigned best_size = TILE_COUNT + 1u;

    for (size_t i = 0; i < state->cell_count; ++i) {
        if (!state->region->cells[i].active) {
            continue;
        }
        if (state->collect_metrics) {
            ++state->metrics.mrv_cells_scanned;
        }

        const unsigned size = domain_popcount(state->domains[i]);
        if (size > 1 && size < best_size) {
            selected = i;
            best_size = size;
            if (size == 2) {
                break;
            }
        }
    }

    return selected;
}

static void note_dfs_node(SolverState *state, size_t depth)
{
    if (state->collect_metrics) {
        ++state->metrics.dfs_nodes;
        if (depth > state->metrics.max_depth) {
            state->metrics.max_depth = depth;
        }
    }
}

static bool search_stack_resize(SearchStack *stack, size_t capacity)
{
    size_t bytes;
    if (capacity == 0 || capacity > stack->limit ||
        !checked_mul_size(capacity, sizeof(*stack->frames), &bytes)) {
        return false;
    }

    SearchFrame *resized = realloc(stack->frames, bytes);
    if (resized == NULL) {
        return false;
    }
    stack->frames = resized;
    stack->capacity = capacity;
    stack->allocated_bytes = bytes;
    return true;
}

static bool search_stack_init(
    SearchStack *stack,
    size_t limit,
    SearchStackMode mode
)
{
    enum { INITIAL_DYNAMIC_CAPACITY = 16 };

    *stack = (SearchStack){ .limit = limit };
    const size_t initial_capacity =
        mode == SEARCH_STACK_DYNAMIC && limit > INITIAL_DYNAMIC_CAPACITY
            ? INITIAL_DYNAMIC_CAPACITY
            : limit;
    size_t bytes;
    if (initial_capacity == 0 ||
        !checked_mul_size(
            initial_capacity,
            sizeof(*stack->frames),
            &bytes
        )) {
        return false;
    }
    stack->frames = malloc(bytes);
    if (stack->frames == NULL) {
        return false;
    }
    stack->capacity = initial_capacity;
    stack->allocated_bytes = bytes;
    return true;
}

static bool search_stack_push(SearchStack *stack, SearchFrame frame)
{
    if (stack->count == stack->limit) {
        return false;
    }
    if (stack->count == stack->capacity) {
        size_t capacity = stack->capacity;
        if (capacity > stack->limit / 2) {
            capacity = stack->limit;
        } else {
            capacity *= 2;
        }
        if (capacity <= stack->capacity ||
            !search_stack_resize(stack, capacity)) {
            return false;
        }
    }

    stack->frames[stack->count++] = frame;
    return true;
}

static void search_stack_destroy(SearchStack *stack)
{
    free(stack->frames);
    *stack = (SearchStack){0};
}

static void note_search_stack_capacity(
    SolverState *state,
    const SearchStack *stack
)
{
    if (state->collect_metrics &&
        stack->capacity > state->metrics.dfs_stack_capacity_peak) {
        state->metrics.dfs_stack_capacity_peak = stack->capacity;
        state->metrics.dfs_stack_bytes_peak = stack->allocated_bytes;
    }
}

static WangSolveStatus search(
    SolverState *state,
    SearchStackMode stack_mode
)
{
    note_dfs_node(state, 0);

    if (state->resolved_count == state->active_count) {
        state->best_depth = 0;
        return WANG_SOLVE_SAT;
    }

    SearchStack stack;
    if (!search_stack_init(&stack, state->active_count, stack_mode)) {
        return WANG_SOLVE_ERROR;
    }
    note_search_stack_capacity(state, &stack);

    const size_t root_cell = select_mrv_cell(state);
    if (root_cell == SIZE_MAX) {
        search_stack_destroy(&stack);
        return WANG_SOLVE_ERROR;
    }

    if (!search_stack_push(&stack, (SearchFrame) {
            .cell_index = root_cell,
            .candidates = state->domains[root_cell],
            .entry_mark = 0,
        })) {
        search_stack_destroy(&stack);
        return WANG_SOLVE_ERROR;
    }
    WangSolveStatus status = WANG_SOLVE_ERROR;

    while (stack.count != 0) {
        SearchFrame *frame = &stack.frames[stack.count - 1];
        if (frame->candidates == 0) {
            const size_t entry_mark = frame->entry_mark;
            --stack.count;
            if (stack.count == 0) {
                status = WANG_SOLVE_UNSAT;
                break;
            }

            rollback_to(state, entry_mark);
            if (state->collect_metrics) {
                ++state->metrics.backtracks;
            }
            continue;
        }

        const TileId tile = first_set_tile(frame->candidates);
        const uint32_t singleton = UINT32_C(1) << tile;
        frame->candidates &= frame->candidates - UINT32_C(1);

        if (state->collect_metrics) {
            ++state->metrics.decisions;
        }

        const size_t mark = state->trail_count;
        if (!restrict_domain(state, frame->cell_index, singleton)) {
            rollback_to(state, mark);
            break;
        }

        size_t conflict_cell = SIZE_MAX;
        const PropagateStatus propagated = propagate_from_cell(
            state,
            frame->cell_index,
            &conflict_cell
        );

        if (propagated == PROPAGATE_ERROR) {
            rollback_to(state, mark);
            break;
        }

        const size_t branch_depth = stack.count;
        if (propagated == PROPAGATE_CONFLICT) {
            if (!record_failed_leaf(
                    state,
                    conflict_cell,
                    branch_depth
                )) {
                rollback_to(state, mark);
                break;
            }
        } else {
            note_dfs_node(state, branch_depth);

            if (state->resolved_count == state->active_count) {
                state->best_depth = branch_depth;
                status = WANG_SOLVE_SAT;
                break;
            }

            const size_t child_cell = select_mrv_cell(state);
            if (child_cell == SIZE_MAX ||
                !search_stack_push(&stack, (SearchFrame) {
                    .cell_index = child_cell,
                    .candidates = state->domains[child_cell],
                    .entry_mark = mark,
                })) {
                rollback_to(state, mark);
                break;
            }
            note_search_stack_capacity(state, &stack);
            continue;
        }

        rollback_to(state, mark);
        if (state->collect_metrics) {
            ++state->metrics.backtracks;
        }
    }

    search_stack_destroy(&stack);
    return status;
}

static bool allocate_solver_arrays(SolverState *state)
{
    size_t domain_bytes;
    size_t neighbor_bytes;
    if (!checked_mul_size(
            state->cell_count,
            sizeof(*state->domains),
            &domain_bytes
        ) ||
        !checked_mul_size(
            state->cell_count,
            sizeof(*state->neighbor_mask),
            &neighbor_bytes
        )) {
        return false;
    }

    state->domains = malloc(domain_bytes);
    state->neighbor_mask = malloc(neighbor_bytes);
    if (state->domains == NULL || state->neighbor_mask == NULL) {
        return false;
    }
    return true;
}

static bool initialize_domains(
    SolverState *state,
    size_t *out_initial_conflict
)
{
    *out_initial_conflict = SIZE_MAX;
    static const int32_t dx[DIR_COUNT] = { 0, 1, 0, -1 };
    static const int32_t dy[DIR_COUNT] = { -1, 0, 1, 0 };

    for (int32_t y = 0; y < state->region->height; ++y) {
        for (int32_t x = 0; x < state->region->width; ++x) {
            const size_t index = region_index(state->region, x, y);
            const RegionCell *cell = &state->region->cells[index];
            state->neighbor_mask[index] = 0;

            if (!cell->active) {
                state->domains[index] = 0;
                continue;
            }

            ++state->active_count;
            uint32_t domain = WANG_DOMAIN_ALL;

            for (Dir dir = N; dir < DIR_COUNT; ++dir) {
                const RegionCell *neighbor = region_cell_const(
                    state->region,
                    x + dx[dir],
                    y + dy[dir]
                );

                if (neighbor != NULL && neighbor->active) {
                    state->neighbor_mask[index] |=
                        (uint8_t)(UINT8_C(1) << dir);
                    continue;
                }

                const ColorId boundary = cell->boundary[dir];
                if (boundary != COLOR_NONE) {
                    const uint32_t restricted =
                        domain & state->tables.edge_mask[dir][boundary];
                    if (state->collect_metrics && restricted != domain) {
                        ++state->metrics.domain_reductions;
                    }
                    domain = restricted;
                }
            }

            /* An isolated cell cannot constrain any other choice. */
            if (state->neighbor_mask[index] == 0 &&
                !domain_is_singleton(domain) && domain != 0) {
                domain &= UINT32_C(0) - domain;
                if (state->collect_metrics) {
                    ++state->metrics.domain_reductions;
                }
            }

            state->domains[index] = domain;
            if (domain_is_singleton(domain)) {
                ++state->resolved_count;
            } else if (domain == 0 && *out_initial_conflict == SIZE_MAX) {
                *out_initial_conflict = index;
            }
        }
    }

    return true;
}

static bool verify_sat_domains(const SolverState *state)
{
    size_t bytes;
    if (!checked_mul_size(state->cell_count, sizeof(TileId), &bytes)) {
        return false;
    }

    TileId *tiles = malloc(bytes);
    if (tiles == NULL) {
        return false;
    }

    bool complete = true;
    for (size_t i = 0; i < state->cell_count; ++i) {
        if (!state->region->cells[i].active) {
            tiles[i] = TILE_NONE;
        } else if (!domain_is_singleton(state->domains[i])) {
            complete = false;
            break;
        } else {
            tiles[i] = first_set_tile(state->domains[i]);
        }
    }

    const bool valid = complete &&
        wang_verify_tiling(state->region, tiles, state->cell_count) ==
            WANG_VERIFY_VALID;
    free(tiles);
    return valid;
}

static bool solver_options_are_valid(const WangSolverOptions *options)
{
    if (options == NULL) {
        return true;
    }

    const uint32_t known_flags =
        WANG_SOLVE_COLLECT_METRICS |
        WANG_SOLVE_TRACE_FAILED_LEAVES |
        WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT;
    if ((options->flags & ~known_flags) != 0) {
        return false;
    }

    if ((options->flags & WANG_SOLVE_TRACE_FAILED_LEAVES) != 0 &&
        (options->failed_leaf_path == NULL ||
         options->failed_leaf_path[0] == '\0' ||
         options->failed_leaf_capacity == 0)) {
        return false;
    }

    return true;
}

void wang_solve_result_destroy(WangSolveResult *result)
{
    if (result == NULL) {
        return;
    }

    free(result->domains);
    *result = (WangSolveResult){0};
}

static WangSolveStatus solve_wang_core(
    const Region *region,
    const WangSolverOptions *options,
    WangSolveResult *out_result,
    SolverMechanisms mechanisms
)
{
    if (!result_is_destroyed(out_result) ||
        !solver_options_are_valid(options)) {
        return WANG_SOLVE_ERROR;
    }

    if (!region_validate(region)) {
        return WANG_SOLVE_ERROR;
    }
    const size_t cell_count = region->cell_count;

    SolverState state = {0};
    state.writer.fd = -1;
    state.region = region;
    state.cell_count = cell_count;
    state.collect_metrics = options != NULL &&
        (options->flags & WANG_SOLVE_COLLECT_METRICS) != 0;
    state.capture_unsat_snapshot = options != NULL &&
        (options->flags & WANG_SOLVE_CAPTURE_UNSAT_SNAPSHOT) != 0;
    state.record_trail = mechanisms.record_initial_trail;
    build_solver_tables(&state.tables);
    if (!solver_tables_are_valid(&state.tables)) {
        return WANG_SOLVE_ERROR;
    }

    if (!allocate_solver_arrays(&state)) {
        solver_state_destroy(&state);
        return WANG_SOLVE_ERROR;
    }

    size_t initial_conflict;
    if (!initialize_domains(&state, &initial_conflict)) {
        solver_state_destroy(&state);
        return WANG_SOLVE_ERROR;
    }

    const bool trace_requested = options != NULL &&
        (options->flags & WANG_SOLVE_TRACE_FAILED_LEAVES) != 0;
    if (trace_requested && !failed_leaf_writer_init(
            &state.writer,
            options->failed_leaf_path,
            options->failed_leaf_capacity,
            region,
            cell_count)) {
        solver_state_destroy(&state);
        return WANG_SOLVE_ERROR;
    }

    WangSolveStatus status;
    if (initial_conflict != SIZE_MAX) {
        if (!record_failed_leaf(&state, initial_conflict, 0)) {
            solver_state_destroy(&state);
            return WANG_SOLVE_ERROR;
        }
        status = WANG_SOLVE_UNSAT;
    } else {
        size_t conflict_cell = SIZE_MAX;
        const PropagateStatus initial_status = propagate_initial(
            &state,
            &conflict_cell
        );

        if (initial_status == PROPAGATE_ERROR) {
            solver_state_destroy(&state);
            return WANG_SOLVE_ERROR;
        }
        if (initial_status == PROPAGATE_CONFLICT) {
            if (!record_failed_leaf(&state, conflict_cell, 0)) {
                solver_state_destroy(&state);
                return WANG_SOLVE_ERROR;
            }
            status = WANG_SOLVE_UNSAT;
        } else {
            state.trail_count = 0;
            state.trail_phase = TRAIL_PHASE_SEARCH;
            state.record_trail = true;
            status = search(&state, mechanisms.stack_mode);
        }
    }

    if (status == WANG_SOLVE_SAT && !verify_sat_domains(&state)) {
        status = WANG_SOLVE_ERROR;
    }
    if (status == WANG_SOLVE_UNSAT && !state.has_best_leaf) {
        status = WANG_SOLVE_ERROR;
    }

    if (status == WANG_SOLVE_SAT) {
        if (!ensure_best_snapshot(&state)) {
            solver_state_destroy(&state);
            return WANG_SOLVE_ERROR;
        }
        memcpy(
            state.best_snapshot,
            state.domains,
            state.cell_count * sizeof(*state.domains)
        );
        state.best_resolved_count = state.resolved_count;
        state.best_conflict_cell = SIZE_MAX;
        state.has_best_leaf = true;
    }

    const size_t traced_leaf_count = state.writer.count;
    const bool trace_truncated = state.writer.truncated;
    if (!failed_leaf_writer_finish(&state.writer)) {
        status = WANG_SOLVE_ERROR;
    }

    if (status == WANG_SOLVE_ERROR) {
        solver_state_destroy(&state);
        return WANG_SOLVE_ERROR;
    }

    const bool return_domains = status == WANG_SOLVE_SAT ||
        state.capture_unsat_snapshot;
    WangSolveResult result = {
        .domains = return_domains ? state.best_snapshot : NULL,
        .domain_count = return_domains ? state.cell_count : 0,
        .conflict_cell = status == WANG_SOLVE_SAT
            ? SIZE_MAX
            : state.best_conflict_cell,
        .resolved_count = state.best_resolved_count,
        .decision_depth = state.best_depth,
        .traced_leaf_count = traced_leaf_count,
        .trace_truncated = trace_truncated,
        .metrics = state.collect_metrics
            ? state.metrics
            : (WangSolverMetrics){0},
    };

    if (return_domains) {
        state.best_snapshot = NULL;
    }
    solver_state_destroy(&state);
    *out_result = result;
    return status;
}

WangSolveStatus wang_solve_serial(
    const Region *region,
    const WangSolverOptions *options,
    WangSolveResult *out_result
)
{
    return solve_wang_core(
        region,
        options,
        out_result,
        (SolverMechanisms) {
            .stack_mode = SEARCH_STACK_FIXED,
            .record_initial_trail = true,
        }
    );
}

WangSolveStatus wang_solve_optimized(
    const Region *region,
    const WangSolverOptions *options,
    WangSolveResult *out_result
)
{
    return solve_wang_core(
        region,
        options,
        out_result,
        (SolverMechanisms) {
            .stack_mode = SEARCH_STACK_DYNAMIC,
            .record_initial_trail = false,
        }
    );
}
