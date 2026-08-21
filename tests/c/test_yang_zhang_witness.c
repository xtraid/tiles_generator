#include "wang/verify.h"
#include "wang/yang_zhang.h"
#include "wang/yang_zhang_witness.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool assignment_is_valid(
    const Cm13Formula *formula,
    uint32_t assignment_bits
)
{
    for (size_t clause = 0; clause < formula->clause_count; ++clause) {
        unsigned true_count = 0;
        for (size_t position = 0; position < 3; ++position) {
            const uint32_t variable =
                formula->clauses[clause].variable_index[position];
            true_count +=
                (assignment_bits >> variable) & UINT32_C(1);
        }
        if (true_count != 1) {
            return false;
        }
    }

    return true;
}

static TileId *tiling_from_result(
    const Region *region,
    const WangSolveResult *result
)
{
    assert(result->domains != NULL);
    assert(result->domain_count == region->cell_count);

    TileId *tiling = malloc(result->domain_count * sizeof(*tiling));
    assert(tiling != NULL);

    for (size_t index = 0; index < result->domain_count; ++index) {
        const uint32_t domain = result->domains[index];
        if (!region->cells[index].active) {
            assert(domain == 0);
            tiling[index] = TILE_NONE;
            continue;
        }

        assert(domain != 0 && (domain & (domain - UINT32_C(1))) == 0);
        TileId tile = 0;
        uint32_t shifted = domain;
        while ((shifted & UINT32_C(1)) == 0) {
            shifted >>= 1;
            ++tile;
        }
        assert(tile < TILE_COUNT);
        tiling[index] = tile;
    }

    return tiling;
}

static void assert_variable_pattern(
    const Region *region,
    const WangSolveResult *result,
    uint32_t variable,
    bool value
)
{
    static const TileId false_tiles[3] = {
        TILE_V0_TOP,
        TILE_V0_MID,
        TILE_V0_BOTTOM,
    };

    for (int32_t row = 0; row < 3; ++row) {
        const TileId expected = value ? TILE_V1 : false_tiles[row];
        const size_t index = region_index(
            region,
            0,
            (int32_t)(4u * variable) + row
        );
        assert(result->domains[index] == (UINT32_C(1) << expected));
    }
}

static void assert_extension_round_trip(
    const Cm13Formula *formula,
    YangZhangReduction *reduction,
    const bool *assignment,
    YangZhangExtensionSolver solver
)
{
    WangSolveResult result = {0};
    const size_t assignment_count = formula->variable_count;

    assert(yang_zhang_solve_assignment_extension(
        formula,
        reduction,
        assignment,
        assignment_count,
        solver,
        &result
    ) == WANG_SOLVE_SAT);

    TileId *tiling = tiling_from_result(&reduction->region, &result);
    assert(wang_verify_tiling(
        &reduction->region,
        tiling,
        reduction->region.cell_count
    ) == WANG_VERIFY_VALID);

    bool *extracted = malloc(assignment_count * sizeof(*extracted));
    assert(extracted != NULL);
    memset(extracted, 0xa5, assignment_count * sizeof(*extracted));
    assert(yang_zhang_extract_assignment(
        formula,
        reduction,
        tiling,
        reduction->region.cell_count,
        extracted,
        assignment_count
    ) == YANG_ZHANG_WITNESS_YES);
    assert(memcmp(
        extracted,
        assignment,
        assignment_count * sizeof(*assignment)
    ) == 0);
    assert(yang_zhang_witnesses_correspond(
        formula,
        reduction,
        assignment,
        assignment_count,
        tiling,
        reduction->region.cell_count
    ) == YANG_ZHANG_WITNESS_YES);

    free(extracted);
    free(tiling);
    wang_solve_result_destroy(&result);
}

static void test_extension_extracts_the_requested_assignment(void)
{
    Cm13Clause clauses[] = {
        { .variable_index = { 0, 0, 1 } },
        { .variable_index = { 0, 1, 2 } },
        { .variable_index = { 1, 2, 2 } },
    };
    Cm13Formula formula = {
        .variable_count = 3,
        .clauses = clauses,
        .clause_count = 3,
    };
    const bool satisfying[] = { false, true, false };
    const bool invalid[] = { false, false, false };
    YangZhangReduction reduction = {0};

    assert(yang_zhang_build(&formula, &reduction));

    for (YangZhangExtensionSolver solver = YANG_ZHANG_EXTENSION_REFERENCE;
         solver <= YANG_ZHANG_EXTENSION_OPTIMIZED;
         ++solver) {
        WangSolveResult result = {0};
        assert(yang_zhang_solve_assignment_extension(
            &formula,
            &reduction,
            satisfying,
            3,
            solver,
            &result
        ) == WANG_SOLVE_SAT);
        assert_variable_pattern(&reduction.region, &result, 0, false);
        assert_variable_pattern(&reduction.region, &result, 1, true);
        assert_variable_pattern(&reduction.region, &result, 2, false);
        wang_solve_result_destroy(&result);

        assert(yang_zhang_solve_assignment_extension(
            &formula,
            &reduction,
            invalid,
            3,
            solver,
            &result
        ) == WANG_SOLVE_UNSAT);
        wang_solve_result_destroy(&result);

        assert_extension_round_trip(
            &formula,
            &reduction,
            satisfying,
            solver
        );
    }

    yang_zhang_reduction_destroy(&reduction);
}

static void test_bridge_ignores_swap_trace(void)
{
    Cm13Clause clauses[] = {
        { .variable_index = { 0, 0, 1 } },
        { .variable_index = { 0, 1, 2 } },
        { .variable_index = { 1, 2, 2 } },
    };
    Cm13Formula formula = {
        .variable_count = 3,
        .clauses = clauses,
        .clause_count = 3,
    };
    const bool assignment[] = { false, true, false };
    YangZhangReduction reduction = {0};

    assert(yang_zhang_build(&formula, &reduction));
    AdjacentSwap *const saved_swaps = reduction.swaps;
    const size_t saved_swap_count = reduction.swap_count;
    reduction.swaps = NULL;
    reduction.swap_count = SIZE_MAX;

    assert_extension_round_trip(
        &formula,
        &reduction,
        assignment,
        YANG_ZHANG_EXTENSION_REFERENCE
    );
    assert(reduction.swaps == NULL);
    assert(reduction.swap_count == SIZE_MAX);

    reduction.swaps = saved_swaps;
    reduction.swap_count = saved_swap_count;
    yang_zhang_reduction_destroy(&reduction);
}

static void test_invalid_bridge_arguments(void)
{
    Cm13Clause clauses[] = {
        { .variable_index = { 0, 0, 1 } },
        { .variable_index = { 0, 1, 1 } },
    };
    Cm13Formula formula = {
        .variable_count = 2,
        .clauses = clauses,
        .clause_count = 2,
    };
    const bool assignment[] = { true, false };
    YangZhangReduction reduction = {0};
    YangZhangReduction destroyed = {0};
    WangSolveResult result = {0};

    assert(yang_zhang_build(&formula, &reduction));
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        assignment,
        1,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &result
    ) == WANG_SOLVE_ERROR);
    assert(result.domains == NULL && result.domain_count == 0);
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        NULL,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &result
    ) == WANG_SOLVE_ERROR);
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &destroyed,
        assignment,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &result
    ) == WANG_SOLVE_ERROR);
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        assignment,
        2,
        (YangZhangExtensionSolver)2,
        &result
    ) == WANG_SOLVE_ERROR);
    assert(yang_zhang_solve_assignment_extension(
        NULL,
        &reduction,
        assignment,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &result
    ) == WANG_SOLVE_ERROR);
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        assignment,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        NULL
    ) == WANG_SOLVE_ERROR);

    WangSolveResult occupied = {
        .domains = malloc(sizeof(*occupied.domains)),
        .domain_count = 1,
        .conflict_cell = 17,
        .resolved_count = 19,
        .decision_depth = 23,
        .traced_leaf_count = 29,
        .trace_truncated = true,
        .metrics = {
            .dfs_nodes = 31,
        },
    };
    assert(occupied.domains != NULL);
    occupied.domains[0] = UINT32_C(0x5a5a5a5a);
    uint32_t *const saved_domains = occupied.domains;
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        assignment,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &occupied
    ) == WANG_SOLVE_ERROR);
    assert(occupied.domains == saved_domains);
    assert(occupied.domains[0] == UINT32_C(0x5a5a5a5a));
    assert(occupied.domain_count == 1);
    assert(occupied.conflict_cell == 17);
    assert(occupied.resolved_count == 19);
    assert(occupied.decision_depth == 23);
    assert(occupied.traced_leaf_count == 29);
    assert(occupied.trace_truncated);
    assert(occupied.metrics.dfs_nodes == 31);
    wang_solve_result_destroy(&occupied);
    assert(occupied.domains == NULL && occupied.domain_count == 0);

    Cm13Formula malformed_formula = formula;
    malformed_formula.clauses = NULL;
    assert(yang_zhang_solve_assignment_extension(
        &malformed_formula,
        &reduction,
        assignment,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &result
    ) == WANG_SOLVE_ERROR);

    const int32_t saved_height = reduction.region.height;
    reduction.region.height = saved_height - 1;
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        assignment,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &result
    ) == WANG_SOLVE_ERROR);
    reduction.region.height = saved_height;

    RegionCell *const variable_top = region_cell(&reduction.region, 0, 0);
    assert(variable_top != NULL);
    const ColorId saved_boundary = variable_top->boundary[W];
    variable_top->boundary[W] = COLOR_B;
    assert(region_validate(&reduction.region));
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        assignment,
        2,
        YANG_ZHANG_EXTENSION_REFERENCE,
        &result
    ) == WANG_SOLVE_ERROR);
    variable_top->boundary[W] = saved_boundary;

    yang_zhang_reduction_destroy(&reduction);
}

static void test_extraction_is_transactional_and_correspondence_is_tri_state(void)
{
    Cm13Clause clauses[] = {
        { .variable_index = { 0, 1, 2 } },
        { .variable_index = { 0, 1, 2 } },
        { .variable_index = { 0, 1, 2 } },
    };
    Cm13Formula formula = {
        .variable_count = 3,
        .clauses = clauses,
        .clause_count = 3,
    };
    const bool assignment[] = { true, false, false };
    const bool different_valid_assignment[] = { false, true, false };
    YangZhangReduction reduction = {0};
    WangSolveResult result = {0};

    assert(yang_zhang_build(&formula, &reduction));
    assert(yang_zhang_solve_assignment_extension(
        &formula,
        &reduction,
        assignment,
        3,
        YANG_ZHANG_EXTENSION_OPTIMIZED,
        &result
    ) == WANG_SOLVE_SAT);
    TileId *tiling = tiling_from_result(&reduction.region, &result);

    bool output[] = { true, true, true };
    const bool sentinel[] = { true, true, true };
    assert(yang_zhang_extract_assignment(
        &formula,
        &reduction,
        tiling,
        reduction.region.cell_count - 1,
        output,
        3
    ) == YANG_ZHANG_WITNESS_ERROR);
    assert(memcmp(output, sentinel, sizeof(output)) == 0);
    assert(yang_zhang_extract_assignment(
        &formula,
        &reduction,
        tiling,
        reduction.region.cell_count,
        output,
        2
    ) == YANG_ZHANG_WITNESS_ERROR);
    assert(memcmp(output, sentinel, sizeof(output)) == 0);

    const size_t variable_cell = region_index(&reduction.region, 0, 0);
    const TileId saved_tile = tiling[variable_cell];
    tiling[variable_cell] = TILE_F0;
    assert(yang_zhang_extract_assignment(
        &formula,
        &reduction,
        tiling,
        reduction.region.cell_count,
        output,
        3
    ) == YANG_ZHANG_WITNESS_NO);
    assert(memcmp(output, sentinel, sizeof(output)) == 0);
    assert(yang_zhang_witnesses_correspond(
        &formula,
        &reduction,
        assignment,
        3,
        tiling,
        reduction.region.cell_count
    ) == YANG_ZHANG_WITNESS_NO);
    tiling[variable_cell] = saved_tile;

    assert(yang_zhang_witnesses_correspond(
        &formula,
        &reduction,
        different_valid_assignment,
        3,
        tiling,
        reduction.region.cell_count
    ) == YANG_ZHANG_WITNESS_NO);
    assert(yang_zhang_witnesses_correspond(
        &formula,
        &reduction,
        assignment,
        2,
        tiling,
        reduction.region.cell_count
    ) == YANG_ZHANG_WITNESS_ERROR);
    assert(yang_zhang_witnesses_correspond(
        &formula,
        &reduction,
        NULL,
        3,
        tiling,
        reduction.region.cell_count
    ) == YANG_ZHANG_WITNESS_ERROR);

    free(tiling);
    wang_solve_result_destroy(&result);
    yang_zhang_reduction_destroy(&reduction);
}

static void report_exhaustive_failure(
    const Cm13Formula *formula,
    uint32_t assignment_bits,
    YangZhangExtensionSolver solver,
    WangSolveStatus status,
    const WangSolveResult *result
)
{
    fprintf(
        stderr,
        "witness failure: variables=%u assignment=0x%x solver=%d "
        "status=%d clauses=",
        formula->variable_count,
        assignment_bits,
        solver,
        status
    );
    for (size_t clause = 0; clause < formula->clause_count; ++clause) {
        fprintf(
            stderr,
            "%s(%u,%u,%u)",
            clause == 0 ? "" : ",",
            formula->clauses[clause].variable_index[0],
            formula->clauses[clause].variable_index[1],
            formula->clauses[clause].variable_index[2]
        );
    }
    fputs(" tiling-domains=", stderr);
    if (result->domains == NULL) {
        fputs("none", stderr);
    } else {
        for (size_t index = 0; index < result->domain_count; ++index) {
            fprintf(stderr, "%s%08x", index == 0 ? "" : ",",
                    result->domains[index]);
        }
    }
    fputc('\n', stderr);
}

static void assert_assignment_equivalence(
    const Cm13Formula *formula,
    YangZhangReduction *reduction,
    uint32_t assignment_bits,
    YangZhangExtensionSolver solver
)
{
    bool assignment[3] = { false, false, false };
    for (uint32_t variable = 0;
         variable < formula->variable_count;
         ++variable) {
        assignment[variable] =
            ((assignment_bits >> variable) & UINT32_C(1)) != 0;
    }

    WangSolveResult result = {0};
    const WangSolveStatus status = yang_zhang_solve_assignment_extension(
        formula,
        reduction,
        assignment,
        formula->variable_count,
        solver,
        &result
    );
    const bool expected_sat = assignment_is_valid(formula, assignment_bits);
    if (status != (expected_sat ? WANG_SOLVE_SAT : WANG_SOLVE_UNSAT)) {
        report_exhaustive_failure(
            formula,
            assignment_bits,
            solver,
            status,
            &result
        );
        abort();
    }

    if (status == WANG_SOLVE_SAT) {
        TileId *tiling = tiling_from_result(&reduction->region, &result);
        bool extracted[3] = { false, false, false };
        const WangVerifyStatus verify_status = wang_verify_tiling(
            &reduction->region,
            tiling,
            reduction->region.cell_count
        );
        const YangZhangWitnessStatus extract_status =
            yang_zhang_extract_assignment(
                formula,
                reduction,
                tiling,
                reduction->region.cell_count,
                extracted,
                formula->variable_count
            );
        const YangZhangWitnessStatus correspond_status =
            yang_zhang_witnesses_correspond(
                formula,
                reduction,
                assignment,
                formula->variable_count,
                tiling,
                reduction->region.cell_count
            );
        if (verify_status != WANG_VERIFY_VALID ||
            extract_status != YANG_ZHANG_WITNESS_YES ||
            memcmp(
                extracted,
                assignment,
                formula->variable_count * sizeof(*assignment)
            ) != 0 ||
            correspond_status != YANG_ZHANG_WITNESS_YES) {
            report_exhaustive_failure(
                formula,
                assignment_bits,
                solver,
                status,
                &result
            );
            free(tiling);
            wang_solve_result_destroy(&result);
            abort();
        }
        free(tiling);
    }

    wang_solve_result_destroy(&result);
}

static void enumerate_canonical_formulas(
    Cm13Formula *formula,
    uint8_t remaining[3],
    size_t position,
    size_t *case_count
)
{
    const size_t signal_count = 3u * formula->variable_count;
    if (position == signal_count) {
        YangZhangReduction reduction = {0};
        assert(yang_zhang_build(formula, &reduction));

        const uint32_t assignment_count =
            UINT32_C(1) << formula->variable_count;
        for (uint32_t bits = 0; bits < assignment_count; ++bits) {
            assert_assignment_equivalence(
                formula,
                &reduction,
                bits,
                YANG_ZHANG_EXTENSION_REFERENCE
            );
            assert_assignment_equivalence(
                formula,
                &reduction,
                bits,
                YANG_ZHANG_EXTENSION_OPTIMIZED
            );
        }

        yang_zhang_reduction_destroy(&reduction);
        ++*case_count;
        return;
    }

    for (uint32_t variable = 0;
         variable < formula->variable_count;
         ++variable) {
        if (remaining[variable] == 0) {
            continue;
        }

        --remaining[variable];
        formula->clauses[position / 3u].variable_index[position % 3u] =
            variable;
        enumerate_canonical_formulas(
            formula,
            remaining,
            position + 1u,
            case_count
        );
        ++remaining[variable];
    }
}

static void test_all_assignments_for_canonical_formulas(void)
{
    Cm13Clause clauses[3];
    size_t case_count = 0;

    for (uint32_t variable_count = 1;
         variable_count <= 3;
         ++variable_count) {
        uint8_t remaining[3] = {0};
        for (uint32_t variable = 0;
             variable < variable_count;
             ++variable) {
            remaining[variable] = 3;
        }

        Cm13Formula formula = {
            .variable_count = variable_count,
            .clauses = clauses,
            .clause_count = variable_count,
        };
        enumerate_canonical_formulas(
            &formula,
            remaining,
            0,
            &case_count
        );
    }

    assert(case_count == 1701);
}

int main(void)
{
    test_extension_extracts_the_requested_assignment();
    test_bridge_ignores_swap_trace();
    test_invalid_bridge_arguments();
    test_extraction_is_transactional_and_correspondence_is_tri_state();
    test_all_assignments_for_canonical_formulas();

    puts("test_yang_zhang_witness: OK");
    return 0;
}
