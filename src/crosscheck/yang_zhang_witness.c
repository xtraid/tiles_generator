#include "wang/yang_zhang_witness.h"

#include "wang/verify.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool witness_layout_is_valid(
    const Cm13Formula *formula,
    const YangZhangReduction *reduction
)
{
    if (formula == NULL || reduction == NULL ||
        formula->variable_count == 0 ||
        formula->variable_count > YANG_ZHANG_MAX_VARIABLES ||
        formula->clauses == NULL ||
        formula->clause_count != (size_t)formula->variable_count ||
        !region_validate(&reduction->region)) {
        return false;
    }

    const int32_t expected_height =
        (int32_t)(4u * formula->variable_count - 1u);
    const int32_t minimum_width = (int32_t)(
        YANG_ZHANG_VARIABLE_WIDTH +
        YANG_ZHANG_LEFT_FORWARD_WIDTH +
        YANG_ZHANG_RIGHT_FORWARD_WIDTH +
        YANG_ZHANG_CLAUSE_WIDTH
    );
    if (reduction->region.height != expected_height ||
        reduction->region.width < minimum_width) {
        return false;
    }

    for (uint32_t variable = 0;
         variable < formula->variable_count;
         ++variable) {
        const int32_t first_y = (int32_t)(4u * variable);
        for (int32_t row = 0; row < 3; ++row) {
            const RegionCell *cell = region_cell_const(
                &reduction->region,
                0,
                first_y + row
            );
            if (cell == NULL || !cell->active ||
                cell->boundary[W] != COLOR_V) {
                return false;
            }
        }
    }

    return true;
}

static bool assignment_storage_is_valid(
    const Cm13Formula *formula,
    const bool *assignment,
    size_t assignment_count
)
{
    return assignment != NULL &&
        assignment_count == (size_t)formula->variable_count;
}

WangSolveStatus yang_zhang_solve_assignment_extension(
    const Cm13Formula *formula,
    const YangZhangReduction *reduction,
    const bool *assignment,
    size_t assignment_count,
    YangZhangExtensionSolver solver,
    WangSolveResult *out_result
)
{
    if (!witness_layout_is_valid(formula, reduction) ||
        !assignment_storage_is_valid(
            formula,
            assignment,
            assignment_count
        ) ||
        (solver != YANG_ZHANG_EXTENSION_REFERENCE &&
         solver != YANG_ZHANG_EXTENSION_OPTIMIZED) ||
        out_result == NULL) {
        return WANG_SOLVE_ERROR;
    }

    const Region *region = &reduction->region;
    uint32_t *domains = malloc(region->cell_count * sizeof(*domains));
    if (domains == NULL) {
        return WANG_SOLVE_ERROR;
    }

    for (size_t index = 0; index < region->cell_count; ++index) {
        domains[index] = region->cells[index].active ? WANG_DOMAIN_ALL : 0;
    }

    static const TileId false_tiles[3] = {
        TILE_V0_TOP,
        TILE_V0_MID,
        TILE_V0_BOTTOM,
    };
    for (uint32_t variable = 0;
         variable < formula->variable_count;
         ++variable) {
        const int32_t first_y = (int32_t)(4u * variable);
        for (int32_t row = 0; row < 3; ++row) {
            const size_t index = region_index(region, 0, first_y + row);
            const TileId tile = assignment[variable]
                ? TILE_V1
                : false_tiles[row];
            domains[index] = UINT32_C(1) << tile;
        }
    }

    const WangSolverOptions options = {
        .initial_domains = domains,
        .initial_domain_count = region->cell_count,
    };
    const WangSolveStatus status =
        solver == YANG_ZHANG_EXTENSION_REFERENCE
            ? wang_solve_serial(region, &options, out_result)
            : wang_solve_optimized(region, &options, out_result);

    free(domains);
    return status;
}

YangZhangWitnessStatus yang_zhang_extract_assignment(
    const Cm13Formula *formula,
    const YangZhangReduction *reduction,
    const TileId *tiling,
    size_t tiling_count,
    bool *out_assignment,
    size_t assignment_count
)
{
    if (!witness_layout_is_valid(formula, reduction) ||
        tiling == NULL || out_assignment == NULL ||
        tiling_count != reduction->region.cell_count ||
        assignment_count != (size_t)formula->variable_count) {
        return YANG_ZHANG_WITNESS_ERROR;
    }

    if (wang_verify_tiling(
            &reduction->region,
            tiling,
            tiling_count
        ) != WANG_VERIFY_VALID) {
        return YANG_ZHANG_WITNESS_NO;
    }

    bool *decoded = malloc(assignment_count * sizeof(*decoded));
    if (decoded == NULL) {
        return YANG_ZHANG_WITNESS_ERROR;
    }

    for (uint32_t variable = 0;
         variable < formula->variable_count;
         ++variable) {
        const int32_t first_y = (int32_t)(4u * variable);
        const TileId top = tiling[region_index(
            &reduction->region,
            0,
            first_y
        )];
        const TileId middle = tiling[region_index(
            &reduction->region,
            0,
            first_y + 1
        )];
        const TileId bottom = tiling[region_index(
            &reduction->region,
            0,
            first_y + 2
        )];

        if (top == TILE_V0_TOP &&
            middle == TILE_V0_MID &&
            bottom == TILE_V0_BOTTOM) {
            decoded[variable] = false;
        } else if (top == TILE_V1 &&
                   middle == TILE_V1 &&
                   bottom == TILE_V1) {
            decoded[variable] = true;
        } else {
            free(decoded);
            return YANG_ZHANG_WITNESS_NO;
        }
    }

    memcpy(out_assignment, decoded, assignment_count * sizeof(*decoded));
    free(decoded);
    return YANG_ZHANG_WITNESS_YES;
}

YangZhangWitnessStatus yang_zhang_witnesses_correspond(
    const Cm13Formula *formula,
    const YangZhangReduction *reduction,
    const bool *assignment,
    size_t assignment_count,
    const TileId *tiling,
    size_t tiling_count
)
{
    if (!witness_layout_is_valid(formula, reduction) ||
        !assignment_storage_is_valid(
            formula,
            assignment,
            assignment_count
        )) {
        return YANG_ZHANG_WITNESS_ERROR;
    }

    bool *decoded = malloc(assignment_count * sizeof(*decoded));
    if (decoded == NULL) {
        return YANG_ZHANG_WITNESS_ERROR;
    }

    const YangZhangWitnessStatus status = yang_zhang_extract_assignment(
        formula,
        reduction,
        tiling,
        tiling_count,
        decoded,
        assignment_count
    );
    if (status != YANG_ZHANG_WITNESS_YES) {
        free(decoded);
        return status;
    }

    const bool equal = memcmp(
        decoded,
        assignment,
        assignment_count * sizeof(*decoded)
    ) == 0;
    free(decoded);
    return equal ? YANG_ZHANG_WITNESS_YES : YANG_ZHANG_WITNESS_NO;
}
