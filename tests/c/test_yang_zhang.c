#include "wang/yang_zhang.h"

#include <assert.h>
#include <stdio.h>

static void test_layout_normal(void)
{
    const AdjacentSwap swaps[] = {
        { .row = 7 }, /* paper swap(8), width 8 */
        { .row = 6 }, /* paper swap(7), width 7 */
        { .row = 5 }  /* paper swap(6), width 6 */
    };

    YangZhangLayout layout;

    assert(yang_zhang_layout_init(
        &layout,
        3,
        swaps,
        3
    ));

    assert(layout.height == 11);

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
    assert(layout.width == 28);

    assert(layout.swap_count == 3);
    assert(layout.swaps != NULL);
    assert(layout.swaps != swaps);

    assert(layout.swaps[0].row == 7);
    assert(layout.swaps[1].row == 6);
    assert(layout.swaps[2].row == 5);

    yang_zhang_layout_destroy(&layout);

    assert(layout.height == 0);
    assert(layout.width == 0);
    assert(layout.swap_count == 0);
    assert(layout.swaps == NULL);
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

    YangZhangLayout layout;

    assert(yang_zhang_layout_init(
        &layout,
        3,
        swaps,
        sizeof(swaps) / sizeof(swaps[0])
    ));

    assert(layout.height == 11);

    /*
     * Paper crossover widths sum to 89.
     *
     * Project coarse width:
     *   1 + 2 + 89 + 2 + 2 = 96
     */
    assert(layout.width == 96);
    assert(layout.swap_count == 14);

    yang_zhang_layout_destroy(&layout);
}

static void test_layout_without_swaps(void)
{
    YangZhangLayout layout;

    assert(yang_zhang_layout_init(
        &layout,
        1,
        NULL,
        0
    ));

    assert(layout.height == 3);

    /*
     * Even without a crossover block, the project convention retains
     * the two signal-propagation bands.
     *
     * 1 + 2 + 0 + 2 + 2 = 7
     */
    assert(layout.width == 7);

    assert(layout.swap_count == 0);
    assert(layout.swaps == NULL);

    yang_zhang_layout_destroy(&layout);
}

static void test_zero_variables_rejected(void)
{
    YangZhangLayout layout;

    assert(!yang_zhang_layout_init(
        &layout,
        0,
        NULL,
        0
    ));
}

static void test_null_swaps_rejected(void)
{
    YangZhangLayout layout;

    assert(!yang_zhang_layout_init(
        &layout,
        2,
        NULL,
        1
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

    YangZhangLayout layout;

    assert(!yang_zhang_layout_init(
        &layout,
        2,
        swaps,
        1
    ));
}

static void test_null_layout_rejected(void)
{
    assert(!yang_zhang_layout_init(
        NULL,
        1,
        NULL,
        0
    ));
}

static void test_destroy_null_is_safe(void)
{
    yang_zhang_layout_destroy(NULL);
}

int main(void)
{
    test_layout_normal();
    test_paper_example_swap_sequence_with_project_padding();
    test_layout_without_swaps();

    test_zero_variables_rejected();
    test_null_swaps_rejected();
    test_invalid_swap_row_rejected();
    test_null_layout_rejected();

    test_destroy_null_is_safe();

    puts("test_yang_zhang: OK");
    return 0;
}
