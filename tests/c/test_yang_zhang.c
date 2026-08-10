#include "wang/yang_zhang.h"

#include <assert.h>
#include <stdio.h>

static void test_dimensions_normal(void)
{
    const AdjacentSwap swaps[] = {
        { .row = 7 }, /* paper swap(8), width 8 */
        { .row = 6 }, /* paper swap(7), width 7 */
        { .row = 5 }  /* paper swap(6), width 6 */
    };

    int32_t height = 0;
    int32_t width = 0;

    assert(yang_zhang_compute_dimensions(
        3,
        swaps,
        3,
        &height,
        &width
    ));

    assert(height == 11);

    /*
     * Project convention:
     *
     * variable       = 1
     * left forward   = 2
     * crossover      = 8 + 7 + 6 = 21
     * right forward  = 2
     * clause         = 2
     *
     * total = 28
     */
    assert(width == 28);

    /* The borrowed swap sequence is not modified. */
    assert(swaps[0].row == 7);
    assert(swaps[1].row == 6);
    assert(swaps[2].row == 5);
}

static void test_paper_example_swap_sequence_with_project_padding(void)
{
    /*
     * Paper example, converted from 1-based swap(k) to 0-based row:
     *
     * 8,7,6,5,4,3,4,5,6,9,8,7,9,8
     * ->
     * 7,6,5,4,3,2,3,4,5,8,7,6,8,7
     */
    const AdjacentSwap swaps[] = {
        { .row = 7 },
        { .row = 6 },
        { .row = 5 },
        { .row = 4 },
        { .row = 3 },
        { .row = 2 },
        { .row = 3 },
        { .row = 4 },
        { .row = 5 },
        { .row = 8 },
        { .row = 7 },
        { .row = 6 },
        { .row = 8 },
        { .row = 7 }
    };

    int32_t height = 0;
    int32_t width = 0;

    assert(yang_zhang_compute_dimensions(
        3,
        swaps,
        sizeof(swaps) / sizeof(swaps[0]),
        &height,
        &width
    ));

    assert(height == 11);

    /*
     * Paper crossover widths sum to 89.
     *
     * Project coarse width:
     *   1 + 2 + 89 + 2 + 2 = 96
     */
    assert(width == 96);
}

static void test_layout_without_swaps(void)
{
    int32_t height = 0;
    int32_t width = 0;

    assert(yang_zhang_compute_dimensions(
        1,
        NULL,
        0,
        &height,
        &width
    ));

    assert(height == 3);

    /*
     * Even without a crossover block, the project convention retains
     * the two signal-propagation bands.
     *
     * 1 + 2 + 0 + 2 + 2 = 7
     */
    assert(width == 7);
}

static void test_zero_variables_rejected(void)
{
    int32_t height = 11;
    int32_t width = 28;

    assert(!yang_zhang_compute_dimensions(
        0,
        NULL,
        0,
        &height,
        &width
    ));

    assert(height == 11);
    assert(width == 28);
}

static void test_null_swaps_rejected(void)
{
    int32_t height = 0;
    int32_t width = 0;

    assert(!yang_zhang_compute_dimensions(
        2,
        NULL,
        1,
        &height,
        &width
    ));
}

static void test_invalid_swap_row_rejected(void)
{
    /*
     * n = 2 -> height = 7.
     * Valid zero-based swap rows are 0..5.
     */
    const AdjacentSwap swaps[] = {
        { .row = 6 }
    };

    int32_t height = 0;
    int32_t width = 0;

    assert(!yang_zhang_compute_dimensions(
        2,
        swaps,
        1,
        &height,
        &width
    ));
}

static void test_null_outputs_rejected(void)
{
    int32_t height = 0;
    int32_t width = 0;

    assert(!yang_zhang_compute_dimensions(
        1,
        NULL,
        0,
        NULL,
        &width
    ));

    assert(!yang_zhang_compute_dimensions(
        1,
        NULL,
        0,
        &height,
        NULL
    ));
}

static void test_variable_count_overflow_rejected(void)
{
    int32_t height = 0;
    int32_t width = 0;

    assert(!yang_zhang_compute_dimensions(
        YANG_ZHANG_MAX_VARIABLES + 1u,
        NULL,
        0,
        &height,
        &width
    ));
}

static void test_width_overflow_rejected(void)
{
    const AdjacentSwap swaps[] = {
        { .row = (uint32_t)INT32_MAX - 7u }
    };
    int32_t height = 0;
    int32_t width = 0;

    assert(!yang_zhang_compute_dimensions(
        YANG_ZHANG_MAX_VARIABLES,
        swaps,
        1,
        &height,
        &width
    ));
}

int main(void)
{
    test_dimensions_normal();
    test_paper_example_swap_sequence_with_project_padding();
    test_layout_without_swaps();

    test_zero_variables_rejected();
    test_null_swaps_rejected();
    test_invalid_swap_row_rejected();
    test_null_outputs_rejected();
    test_variable_count_overflow_rejected();
    test_width_overflow_rejected();

    puts("test_yang_zhang: OK");
    return 0;
}
