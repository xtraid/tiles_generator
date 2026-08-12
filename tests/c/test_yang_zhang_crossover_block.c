#include "wang/permutation.h"
#include "wang/solver.h"
#include "wang/tile.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { EXHAUSTIVE_SIGNAL_COUNT = 7, MAX_TEST_SIGNAL_COUNT = 31 };

static ColorId signal_color(bool signal)
{
    return signal ? COLOR_1 : COLOR_0;
}

static TileId solved_tile_at(
    const Region *region,
    const WangSolveResult *result,
    int32_t x,
    int32_t y
)
{
    uint32_t domain = result->domains[region_index(region, x, y)];
    assert(domain != 0 && (domain & (domain - UINT32_C(1))) == 0);

    TileId tile = 0;
    while ((domain & UINT32_C(1)) == 0) {
        domain >>= 1;
        ++tile;
    }
    assert(tile < TILE_COUNT);
    return tile;
}

static int32_t crossover_chain_width(
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t signal_count
)
{
    int32_t width = 0;

    assert(swaps != NULL);
    assert(swap_count > 0);
    for (size_t i = 0; i < swap_count; ++i) {
        assert(swaps[i].row < (uint32_t)signal_count - 1u);
        width += (int32_t)swaps[i].row + 1;
    }

    return width;
}

/*
 * Reproduce only the boundary conditions contributed by the real builder to
 * one or more adjacent crossover blocks.  West and east are cut interfaces:
 * inputs are fixed on west, and output_mask selects the binary compatibility
 * conditions imposed on east.  No interior edge or tile is preselected.
 */
static void build_crossover_chain_region(
    Region *region,
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t signal_count,
    uint32_t input_bits,
    uint32_t output_mask,
    uint32_t output_bits
)
{
    const int32_t width = crossover_chain_width(
        swaps,
        swap_count,
        signal_count
    );

    assert(signal_count >= 3 && signal_count <= MAX_TEST_SIGNAL_COUNT);
    assert(region_init(region, width, signal_count));
    for (int32_t y = 0; y < region->height; ++y) {
        for (int32_t x = 0; x < region->width; ++x) {
            assert(region_set_active(region, x, y, true));
        }
    }

    for (int32_t x = 0; x < region->width; ++x) {
        assert(region_set_boundary(region, x, 0, N, COLOR_B));
        assert(region_set_boundary(
            region,
            x,
            region->height - 1,
            S,
            COLOR_B
        ));
    }

    int32_t block_x = 0;
    for (size_t i = 0; i < swap_count; ++i) {
        const int32_t block_width = (int32_t)swaps[i].row + 1;
        assert(region_set_boundary(
            region,
            block_x + block_width - 1,
            0,
            N,
            COLOR_R
        ));
        assert(region_set_boundary(
            region,
            block_x,
            region->height - 1,
            S,
            COLOR_L
        ));
        block_x += block_width;
    }
    assert(block_x == region->width);

    for (int32_t y = 0; y < region->height; ++y) {
        const bool input = (input_bits & (UINT32_C(1) << y)) != 0;
        assert(region_set_boundary(
            region,
            0,
            y,
            W,
            signal_color(input)
        ));
    }

    for (int32_t y = 0; y < region->height; ++y) {
        const uint32_t row_mask = UINT32_C(1) << y;
        if ((output_mask & row_mask) != 0) {
            assert(region_set_boundary(
                region,
                region->width - 1,
                y,
                E,
                signal_color((output_bits & row_mask) != 0)
            ));
        }
    }

    assert(region_validate(region));
}

static uint32_t apply_swaps_to_bits(
    uint32_t input_bits,
    const AdjacentSwap *swaps,
    size_t swap_count
)
{
    uint32_t output_bits = input_bits;

    for (size_t i = 0; i < swap_count; ++i) {
        const uint32_t top_mask = UINT32_C(1) << swaps[i].row;
        const uint32_t bottom_mask = top_mask << 1;
        const bool top = (output_bits & top_mask) != 0;
        const bool bottom = (output_bits & bottom_mask) != 0;

        if (top != bottom) {
            output_bits ^= top_mask | bottom_mask;
        }
    }

    return output_bits;
}

static uint32_t low_bits_mask(int32_t bit_count)
{
    assert(bit_count > 0 && bit_count < 32);
    return (UINT32_C(1) << bit_count) - UINT32_C(1);
}

static void assert_chain_input(
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t signal_count,
    uint32_t input,
    uint32_t negative_row_mask
)
{
    const uint32_t all_rows = low_bits_mask(signal_count);
    const uint32_t expected = apply_swaps_to_bits(
        input,
        swaps,
        swap_count
    );
    Region region = {0};
    WangSolveResult result = {0};

    build_crossover_chain_region(
        &region,
        swaps,
        swap_count,
        signal_count,
        input,
        all_rows,
        expected
    );
    assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_SAT);

    for (int32_t y = 0; y < region.height; ++y) {
        const bool expected_signal =
            (expected & (UINT32_C(1) << y)) != 0;
        const TileId tile = solved_tile_at(
            &region,
            &result,
            region.width - 1,
            y
        );
        assert(TILESET[tile].edge[E] == signal_color(expected_signal));
    }

    wang_solve_result_destroy(&result);
    region_destroy(&region);

    /* Keep a complete binary interface while flipping selected output rows. */
    for (int32_t y = 0; y < signal_count; ++y) {
        const uint32_t row_mask = UINT32_C(1) << y;
        if ((negative_row_mask & row_mask) == 0) {
            continue;
        }

        build_crossover_chain_region(
            &region,
            swaps,
            swap_count,
            signal_count,
            input,
            all_rows,
            expected ^ row_mask
        );
        assert(wang_solve_serial(&region, NULL, &result) == WANG_SOLVE_UNSAT);
        wang_solve_result_destroy(&result);
        region_destroy(&region);
    }
}

static void assert_chain_binary_relation_exhaustive(
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t signal_count
)
{
    const uint32_t vector_count = UINT32_C(1) << signal_count;
    const uint32_t all_rows = low_bits_mask(signal_count);

    for (uint32_t input = 0; input < vector_count; ++input) {
        const uint32_t expected = apply_swaps_to_bits(
            input,
            swaps,
            swap_count
        );

        for (uint32_t output = 0; output < vector_count; ++output) {
            Region region = {0};
            WangSolveResult result = {0};

            build_crossover_chain_region(
                &region,
                swaps,
                swap_count,
                signal_count,
                input,
                all_rows,
                output
            );
            const WangSolveStatus status = wang_solve_serial(
                &region,
                NULL,
                &result
            );
            assert(status == (output == expected
                ? WANG_SOLVE_SAT
                : WANG_SOLVE_UNSAT));

            wang_solve_result_destroy(&result);
            region_destroy(&region);
        }
    }
}

static void assert_chain_forces_permutation(
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t signal_count
)
{
    const uint32_t input_count = UINT32_C(1) << signal_count;
    const uint32_t all_rows = low_bits_mask(signal_count);

    for (uint32_t input = 0; input < input_count; ++input) {
        assert_chain_input(
            swaps,
            swap_count,
            signal_count,
            input,
            all_rows
        );
    }
}

static void test_every_whole_block_position(void)
{
    for (uint32_t row = 0; row < EXHAUSTIVE_SIGNAL_COUNT - 1u; ++row) {
        const AdjacentSwap swap = { .row = row };
        assert_chain_binary_relation_exhaustive(
            &swap,
            1,
            EXHAUSTIVE_SIGNAL_COUNT
        );
    }
}

static void test_every_position_at_larger_scales(void)
{
    static const int32_t signal_counts[] = { 3, 11, 15, 23, 31 };

    for (size_t scale = 0;
         scale < sizeof(signal_counts) / sizeof(signal_counts[0]);
         ++scale) {
        const int32_t signal_count = signal_counts[scale];
        const uint32_t all_rows = low_bits_mask(signal_count);
        const uint32_t alternating = UINT32_C(0xaaaaaaaa) & all_rows;
        const uint32_t inputs[] = {
            alternating,
            alternating ^ all_rows,
        };

        for (uint32_t row = 0; row < (uint32_t)signal_count - 1u; ++row) {
            const AdjacentSwap swap = { .row = row };
            for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
                assert_chain_input(
                    &swap,
                    1,
                    signal_count,
                    inputs[i],
                    all_rows
                );
            }
        }
    }
}

static SignalToken token(uint32_t token_id)
{
    return (SignalToken){
        .kind = SIGNAL_REDUNDANT,
        .token_id = token_id,
    };
}

static void assert_built_permutation_routes_signals(
    const uint32_t target_order[EXHAUSTIVE_SIGNAL_COUNT],
    size_t expected_swap_count
)
{
    SignalToken source[EXHAUSTIVE_SIGNAL_COUNT];
    SignalToken target[EXHAUSTIVE_SIGNAL_COUNT];
    AdjacentSwap *swaps = NULL;
    size_t swap_count = 0;

    for (uint32_t i = 0; i < EXHAUSTIVE_SIGNAL_COUNT; ++i) {
        source[i] = token(i);
        target[i] = token(target_order[i]);
    }

    assert(yang_zhang_permutation_build(
        source,
        target,
        EXHAUSTIVE_SIGNAL_COUNT,
        &swaps,
        &swap_count
    ));
    assert(swap_count == expected_swap_count);

    assert_chain_forces_permutation(
        swaps,
        swap_count,
        EXHAUSTIVE_SIGNAL_COUNT
    );
    free(swaps);
}

static void test_composed_blocks_follow_permutation_builder(void)
{
    const uint32_t two_swap_target[EXHAUSTIVE_SIGNAL_COUNT] = {
        2, 0, 1, 3, 4, 5, 6
    };
    const uint32_t three_swap_target[EXHAUSTIVE_SIGNAL_COUNT] = {
        1, 3, 0, 2, 4, 5, 6
    };

    assert_built_permutation_routes_signals(two_swap_target, 2);
    assert_built_permutation_routes_signals(three_swap_target, 3);
}

static uint32_t fuzz_state = UINT32_C(0x91e10da5);

static uint32_t next_fuzz_value(void)
{
    uint32_t value = fuzz_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    fuzz_state = value;
    return value;
}

static void test_deterministic_crossover_chain_fuzz(void)
{
    enum { FUZZ_CASES = 192, MAX_FUZZ_SWAPS = 16 };
    AdjacentSwap swaps[MAX_FUZZ_SWAPS];

    for (size_t test_case = 0; test_case < FUZZ_CASES; ++test_case) {
        /* Every sampled height has the real Yang-Zhang form 4n - 1. */
        const int32_t signal_count =
            3 + 4 * (int32_t)(next_fuzz_value() % 4u);
        const size_t swap_count =
            1u + (size_t)(next_fuzz_value() % MAX_FUZZ_SWAPS);

        for (size_t i = 0; i < swap_count; ++i) {
            swaps[i].row = next_fuzz_value() %
                ((uint32_t)signal_count - 1u);
        }

        const uint32_t input =
            next_fuzz_value() & low_bits_mask(signal_count);
        assert_chain_input(
            swaps,
            swap_count,
            signal_count,
            input,
            low_bits_mask(signal_count)
        );
    }
}

static void test_large_crossover_chain_volumes(void)
{
    static const struct {
        int32_t signal_count;
        size_t swap_count;
    } cases[] = {
        { 15, 48 },
        { 23, 72 },
        { 31, 96 },
    };
    AdjacentSwap swaps[96];

    for (size_t test_case = 0;
         test_case < sizeof(cases) / sizeof(cases[0]);
         ++test_case) {
        const int32_t signal_count = cases[test_case].signal_count;
        const size_t swap_count = cases[test_case].swap_count;

        for (size_t i = 0; i < swap_count; ++i) {
            swaps[i].row = (uint32_t)(
                (17u * i + i / 3u + test_case) %
                ((size_t)signal_count - 1u)
            );
        }

        const uint32_t input = next_fuzz_value() &
            low_bits_mask(signal_count);
        const uint32_t sampled_negative_rows =
            UINT32_C(1) |
            (UINT32_C(1) << (signal_count / 2)) |
            (UINT32_C(1) << (signal_count - 1));

        assert_chain_input(
            swaps,
            swap_count,
            signal_count,
            input,
            sampled_negative_rows
        );
    }
}

int main(void)
{
    test_every_whole_block_position();
    test_every_position_at_larger_scales();
    test_composed_blocks_follow_permutation_builder();
    test_deterministic_crossover_chain_fuzz();
    test_large_crossover_chain_volumes();

    puts("test_yang_zhang_crossover_block: OK");
    return 0;
}
