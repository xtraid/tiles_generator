#include "wang/formula_parser.h"
#include "wang/solver.h"
#include "wang/verify.h"
#include "wang/yang_zhang.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void assert_sat_witness(
    const YangZhangReduction *reduction,
    const WangSolveResult *result
)
{
    const size_t cell_count =
        (size_t)reduction->region.width * (size_t)reduction->region.height;
    TileId *tiles = malloc(cell_count * sizeof(*tiles));

    assert(tiles != NULL);
    assert(result->domain_count == cell_count);
    assert(result->conflict_cell == SIZE_MAX);

    for (size_t cell = 0; cell < cell_count; ++cell) {
        if (!reduction->region.cells[cell].active) {
            assert(result->domains[cell] == 0);
            tiles[cell] = TILE_NONE;
            continue;
        }

        uint32_t domain = result->domains[cell];
        TileId tile = 0;
        assert(domain != 0 && (domain & (domain - 1u)) == 0);
        while ((domain & UINT32_C(1)) == 0) {
            domain >>= 1;
            ++tile;
        }
        assert(tile < TILE_COUNT);
        tiles[cell] = tile;
    }

    assert(wang_verify_tiling(&reduction->region, tiles, cell_count) ==
           WANG_VERIFY_VALID);
    free(tiles);
}

static void assert_pipeline_status(
    const char *path,
    WangSolveStatus expected_status
)
{
    Cm13Formula formula = {0};
    Cm13ParseLocation location = {0};
    YangZhangReduction reduction = {0};
    WangSolveResult result = {0};

    assert(cm13_formula_load_path(path, &formula, &location) == CM13_PARSE_OK);
    assert(location.line == 0 && location.column == 0);
    assert(yang_zhang_build(&formula, &reduction));

    const WangSolveStatus status =
        wang_solve_serial(&reduction.region, NULL, &result);
    assert(status == expected_status);

    if (status == WANG_SOLVE_SAT) {
        assert_sat_witness(&reduction, &result);
    } else {
        assert(result.conflict_cell < result.domain_count);
        assert(result.domains[result.conflict_cell] == 0);
    }

    wang_solve_result_destroy(&result);
    yang_zhang_reduction_destroy(&reduction);
    cm13_formula_destroy(&formula);
}

int main(void)
{
    assert_pipeline_status(
        "tests/instances/pipeline_sat.cm13",
        WANG_SOLVE_SAT
    );
    assert_pipeline_status(
        "tests/instances/pipeline_unsat.cm13",
        WANG_SOLVE_UNSAT
    );

    puts("test_pipeline: OK");
    return 0;
}
