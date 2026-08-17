#include "wang/solver.h"
#include "wang/verify.h"
#include "wang/yang_zhang.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static bool boolean_oracle(const Cm13Formula *formula)
{
    assert(formula->variable_count < 32);
    const uint32_t assignment_count = UINT32_C(1) << formula->variable_count;

    for (uint32_t assignment = 0;
         assignment < assignment_count;
         ++assignment) {
        bool valid = true;
        for (size_t c = 0; c < formula->clause_count; ++c) {
            unsigned true_count = 0;
            for (size_t position = 0; position < 3; ++position) {
                const uint32_t variable =
                    formula->clauses[c].variable_index[position];
                true_count += (assignment >> variable) & UINT32_C(1);
            }
            if (true_count != 1) {
                valid = false;
                break;
            }
        }
        if (valid) {
            return true;
        }
    }

    return false;
}

static void assert_reduction_matches_boolean_oracle(Cm13Formula *formula)
{
    YangZhangReduction reduction = {0};
    WangSolveResult result = {0};
    const bool expected_sat = boolean_oracle(formula);

    assert(yang_zhang_build(formula, &reduction));
    const WangSolveStatus status = wang_solve_serial(
        &reduction.region,
        NULL,
        &result
    );
    assert(status == (expected_sat ? WANG_SOLVE_SAT : WANG_SOLVE_UNSAT));

    if (status == WANG_SOLVE_SAT) {
        size_t active_count = 0;
        TileId *tiles = malloc(result.domain_count * sizeof(*tiles));
        assert(tiles != NULL);
        assert(result.conflict_cell == SIZE_MAX);
        for (size_t i = 0; i < result.domain_count; ++i) {
            const uint32_t domain = result.domains[i];
            if (!reduction.region.cells[i].active) {
                assert(domain == 0);
                tiles[i] = TILE_NONE;
                continue;
            }

            ++active_count;
            assert(domain != 0 && (domain & (domain - 1u)) == 0);

            TileId tile = 0;
            uint32_t remaining = domain;
            while ((remaining & UINT32_C(1)) == 0) {
                remaining >>= 1;
                ++tile;
            }
            tiles[i] = tile;
        }
        assert(result.resolved_count == active_count);
        assert(wang_verify_tiling(
            &reduction.region,
            tiles,
            result.domain_count
        ) == WANG_VERIFY_VALID);
        free(tiles);
    } else {
        assert(result.domains == NULL);
        assert(result.domain_count == 0);
        assert(result.conflict_cell < reduction.region.cell_count);
        assert(reduction.region.cells[result.conflict_cell].active);
    }

    wang_solve_result_destroy(&result);
    yang_zhang_reduction_destroy(&reduction);
}

static void test_minimal_unsat_formula(void)
{
    Cm13Clause clauses[] = {
        { .variable_index = { 0, 0, 0 } },
    };
    Cm13Formula formula = {
        .variable_count = 1,
        .clauses = clauses,
        .clause_count = 1,
    };
    assert_reduction_matches_boolean_oracle(&formula);
}

static void test_true_signal_in_first_clause_row(void)
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

    /* Assignment (x0, x1, x2) = (0, 1, 0) satisfies every clause. */
    assert_reduction_matches_boolean_oracle(&formula);
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
        assert_reduction_matches_boolean_oracle(formula);
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

static void test_all_canonical_formulas_through_three_variables(void)
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

static void test_two_variable_unsat_formula(void)
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
    assert_reduction_matches_boolean_oracle(&formula);
}

static void test_three_variable_sat_formulas(void)
{
    Cm13Clause paper_clauses[] = {
        { .variable_index = { 0, 0, 2 } },
        { .variable_index = { 1, 1, 2 } },
        { .variable_index = { 0, 1, 2 } },
    };
    Cm13Formula paper_formula = {
        .variable_count = 3,
        .clauses = paper_clauses,
        .clause_count = 3,
    };
    assert_reduction_matches_boolean_oracle(&paper_formula);

    Cm13Clause symmetric_clauses[] = {
        { .variable_index = { 0, 1, 2 } },
        { .variable_index = { 0, 1, 2 } },
        { .variable_index = { 0, 1, 2 } },
    };
    Cm13Formula symmetric_formula = {
        .variable_count = 3,
        .clauses = symmetric_clauses,
        .clause_count = 3,
    };
    assert_reduction_matches_boolean_oracle(&symmetric_formula);
}

int main(void)
{
    test_minimal_unsat_formula();
    test_two_variable_unsat_formula();
    test_three_variable_sat_formulas();
    test_true_signal_in_first_clause_row();
    test_all_canonical_formulas_through_three_variables();

    puts("test_solver_yang_zhang: OK");
    return 0;
}
