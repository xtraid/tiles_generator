#define _POSIX_C_SOURCE 200809L

#include "wang/solver.h"

#include "wang/tile.h"
#include "wang/verify.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FUZZ_CASES 192u
#define FUZZ_MAX_ACTIVE 3u

static uint32_t random_state = UINT32_C(0x8f31a2c7);

static uint32_t next_random(void)
{
    uint32_t value = random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random_state = value;
    return value;
}

static bool oracle_search(
    const Region *region,
    TileId *tiles,
    size_t cell_count,
    size_t index
)
{
    while (index < cell_count && !region->cells[index].active) {
        ++index;
    }
    if (index == cell_count) {
        return wang_verify_tiling(region, tiles, cell_count) ==
            WANG_VERIFY_VALID;
    }

    for (TileId tile = 0; tile < TILE_COUNT; ++tile) {
        tiles[index] = tile;
        if (oracle_search(region, tiles, cell_count, index + 1)) {
            return true;
        }
    }
    tiles[index] = TILE_NONE;
    return false;
}

static bool brute_force_oracle(const Region *region)
{
    const size_t cell_count =
        (size_t)region->width * (size_t)region->height;
    TileId *tiles = malloc(cell_count * sizeof(*tiles));
    assert(tiles != NULL);
    for (size_t i = 0; i < cell_count; ++i) {
        tiles[i] = TILE_NONE;
    }

    const bool sat = oracle_search(region, tiles, cell_count, 0);
    free(tiles);
    return sat;
}

static void assert_sat_result(
    const Region *region,
    const WangSolveResult *result
)
{
    const size_t cell_count = result->domain_count;
    TileId *tiles = malloc(cell_count * sizeof(*tiles));
    assert(tiles != NULL);

    for (size_t i = 0; i < cell_count; ++i) {
        if (!region->cells[i].active) {
            assert(result->domains[i] == 0);
            tiles[i] = TILE_NONE;
            continue;
        }

        uint32_t domain = result->domains[i];
        assert(domain != 0 && (domain & (domain - 1u)) == 0);
        TileId tile = 0;
        while ((domain & 1u) == 0) {
            domain >>= 1;
            ++tile;
        }
        tiles[i] = tile;
    }

    assert(wang_verify_tiling(region, tiles, cell_count) == WANG_VERIFY_VALID);
    free(tiles);
}

static void random_small_region(Region *region)
{
    const int32_t width = 1 + (int32_t)(next_random() % 3u);
    const int32_t height = 1 + (int32_t)(next_random() % 2u);
    assert(region_init(region, width, height));

    unsigned active_count = 0;
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            const bool active = active_count < FUZZ_MAX_ACTIVE &&
                (next_random() & 1u) != 0;
            if (active) {
                assert(region_set_active(region, x, y, true));
                ++active_count;
            }
        }
    }

    static const int32_t dx[DIR_COUNT] = { 0, 1, 0, -1 };
    static const int32_t dy[DIR_COUNT] = { -1, 0, 1, 0 };
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            const RegionCell *cell = region_cell_const(region, x, y);
            if (!cell->active) {
                continue;
            }
            for (Dir dir = N; dir < DIR_COUNT; ++dir) {
                const RegionCell *neighbor = region_cell_const(
                    region,
                    x + dx[dir],
                    y + dy[dir]
                );
                if (neighbor != NULL && neighbor->active) {
                    continue;
                }

                if (next_random() % 3u != 0) {
                    const ColorId color =
                        (ColorId)(next_random() % COLOR_COUNT);
                    assert(region_set_boundary(region, x, y, dir, color));
                }
            }
        }
    }
}

static void test_deterministic_fuzz_against_oracle(void)
{
    for (unsigned test_case = 0; test_case < FUZZ_CASES; ++test_case) {
        Region region = {0};
        WangSolveResult result = {0};
        random_small_region(&region);

        const size_t cell_count =
            (size_t)region.width * (size_t)region.height;
        RegionCell *original = malloc(cell_count * sizeof(*original));
        assert(original != NULL);
        memcpy(original, region.cells, cell_count * sizeof(*original));

        const bool expected_sat = brute_force_oracle(&region);
        const WangSolveStatus status = wang_solve_serial(
            &region,
            NULL,
            &result
        );
        assert(status == (expected_sat
            ? WANG_SOLVE_SAT
            : WANG_SOLVE_UNSAT));
        assert(memcmp(
            original,
            region.cells,
            cell_count * sizeof(*original)
        ) == 0);

        if (status == WANG_SOLVE_SAT) {
            assert_sat_result(&region, &result);
        } else {
            assert(result.domains == NULL);
            assert(result.domain_count == 0);
            assert(result.conflict_cell < region.cell_count);
            assert(region.cells[result.conflict_cell].active);
        }

        free(original);
        wang_solve_result_destroy(&result);
        region_destroy(&region);
    }
}

static void test_large_thin_forced_region(void)
{
    const int32_t width = 8192;
    Region region = {0};
    WangSolveResult result = {0};
    WangSolverOptions options = { .flags = WANG_SOLVE_COLLECT_METRICS };
    assert(region_init(&region, width, 1));

    for (int32_t x = 0; x < width; ++x) {
        assert(region_set_active(&region, x, 0, true));
    }
    for (int32_t x = 0; x < width; ++x) {
        assert(region_set_boundary(&region, x, 0, N, COLOR_B));
        assert(region_set_boundary(&region, x, 0, S, COLOR_B));
    }
    assert(region_set_boundary(&region, 0, 0, W, COLOR_0));
    assert(region_set_boundary(&region, width - 1, 0, E, COLOR_0));

    assert(wang_solve_serial(&region, &options, &result) == WANG_SOLVE_SAT);
    assert(result.resolved_count == (size_t)width);
    assert(result.metrics.decisions == 0);
    assert_sat_result(&region, &result);

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

static void test_deep_unconstrained_region(void)
{
    const int32_t width = 128;
    const int32_t height = 128;
    Region region = {0};
    WangSolveResult result = {0};
    WangSolverOptions options = { .flags = WANG_SOLVE_COLLECT_METRICS };
    assert(region_init(&region, width, height));

    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            assert(region_set_active(&region, x, y, true));
        }
    }

    assert(wang_solve_serial(&region, &options, &result) == WANG_SOLVE_SAT);
    assert(result.metrics.max_depth > 10000);
    assert(result.decision_depth == result.metrics.max_depth);
    assert_sat_result(&region, &result);

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

static void test_large_disconnected_checkerboard(void)
{
    const int32_t width = 257;
    const int32_t height = 129;
    Region region = {0};
    WangSolveResult result = {0};
    assert(region_init(&region, width, height));

    size_t active_count = 0;
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            if (((x + y) & 1) == 0) {
                assert(region_set_active(&region, x, y, true));
                ++active_count;
            }
        }
    }
    assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_SAT);
    assert(result.resolved_count == active_count);
    assert_sat_result(&region, &result);

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

static void test_large_all_inactive_region(void)
{
    Region region = {0};
    WangSolveResult result = {0};
    assert(region_init(&region, 512, 256));

    assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_SAT);
    assert(result.domain_count == (size_t)512 * 256);
    assert(result.resolved_count == 0);
    for (size_t i = 0; i < result.domain_count; ++i) {
        assert(result.domains[i] == 0);
    }

    wang_solve_result_destroy(&result);
    region_destroy(&region);
}

static void test_trace_capacity_overflow_is_rejected(void)
{
    char path[] = "/tmp/wang-overflow-trace-XXXXXX";
    const int temporary_fd = mkstemp(path);
    assert(temporary_fd >= 0);
    assert(close(temporary_fd) == 0);
    assert(unlink(path) == 0);

    Region region = {0};
    WangSolveResult result = {0};
    WangSolverOptions options = {
        .flags = WANG_SOLVE_TRACE_FAILED_LEAVES,
        .failed_leaf_path = path,
        .failed_leaf_capacity = SIZE_MAX,
    };
    assert(region_init(&region, 1, 1));
    assert(region_set_active(&region, 0, 0, true));
    assert(region_set_boundary(&region, 0, 0, N, COLOR_V));

    assert(wang_solve_serial(&region, &options, &result) == WANG_SOLVE_ERROR);
    assert(result.domains == NULL);
    assert(access(path, F_OK) != 0);

    region_destroy(&region);
}

int main(void)
{
    test_deterministic_fuzz_against_oracle();
    test_large_thin_forced_region();
    test_deep_unconstrained_region();
    test_large_disconnected_checkerboard();
    test_large_all_inactive_region();
    test_trace_capacity_overflow_is_rejected();

    puts("test_solver_stress: OK");
    return 0;
}
