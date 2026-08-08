#include "wang/yang_zhang.h"

#include <assert.h>
#include <stdio.h>


static void test_layout_normal(void)
{
    AdjacentSwap swaps[] = {
        { .row = 7 },  /* crossover width = 8 */
        { .row = 6 },  /* crossover width = 7 */
        { .row = 5 }   /* crossover width = 6 */
    };

    YangZhangLayout layout;

    assert(yang_zhang_layout_init(
        &layout,
        3,
        swaps,
        3
    ));

    /*
     * n = 3
     *
     * height = 4n - 1
     *        = 11
     */
    assert(layout.height == 11);

    /*
     * crossover width:
     *
     * 8 + 7 + 6 = 21
     *
     * total width:
     *
     * variable       = 1
     * left forward   = 2
     * crossover      = 21
     * right forward  = 2
     * clause         = 2
     *
     * total = 28
     */
    assert(layout.width == 28);

    assert(layout.swap_count == 3);
    assert(layout.swaps != NULL);

    assert(layout.swaps[0].row == 7);
    assert(layout.swaps[1].row == 6);
    assert(layout.swaps[2].row == 5);

    /*
     * The layout must own its own copy.
     */
    assert(layout.swaps != swaps);

    /*
     * Changing the original array must not affect
     * the copy stored in the layout.
     */
    swaps[0].row = 0;

    assert(layout.swaps[0].row == 7);

    yang_zhang_layout_destroy(&layout);

    assert(layout.height == 0);
    assert(layout.width == 0);
    assert(layout.swap_count == 0);
    assert(layout.swaps == NULL);
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

    /*
     * n = 1
     *
     * height = 4(1) - 1 = 3
     */
    assert(layout.height == 3);

    /*
     * No crossover blocks:
     *
     * 1 variable
     * + 2 left forward
     * + 0 crossover
     * + 2 right forward
     * + 2 clause
     *
     * = 7
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
     * n = 2
     *
     * height = 4(2) - 1 = 7
     *
     * Valid adjacent swaps have:
     *
     * row < height - 1
     * row < 6
     *
     * Therefore row = 6 is invalid.
     */
    AdjacentSwap swaps[] = {
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
    /*
     * Must behave like free(NULL):
     * simply do nothing.
     */
    yang_zhang_layout_destroy(NULL);
}


int main(void)
{
    test_layout_normal();
    test_layout_without_swaps();

    test_zero_variables_rejected();
    test_null_swaps_rejected();
    test_invalid_swap_row_rejected();
    test_null_layout_rejected();

    test_destroy_null_is_safe();

    puts("test_yang_zhang: OK");

    return 0;
}
