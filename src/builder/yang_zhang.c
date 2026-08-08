#include "wang/yang_zhang.h"

#include <stdlib.h>
#include <string.h>

static bool compute_height(
    uint32_t variable_count,
    int32_t *out_height
)
{
    if (out_height == NULL || variable_count == 0) {
        return false;
    }

    const uint64_t height =
        4u * (uint64_t)variable_count - 1u;

    if (height > (uint64_t)INT32_MAX) {
        return false;
    }

    *out_height = (int32_t)height;
    return true;
}

static bool compute_crossover_width(
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t height,
    int32_t *out_width
)
{
    if (out_width == NULL || height <= 0) {
        return false;
    }

    if (swap_count > 0 && swaps == NULL) {
        return false;
    }

    uint64_t width = 0;

    for (size_t i = 0; i < swap_count; ++i) {
        /*
         * row is 0-based and swaps row with row + 1.
         * The final row therefore cannot be used as swap.row.
         */
        if (swaps[i].row >= (uint32_t)(height - 1)) {
            return false;
        }

        /*
         * Yang-Zhang paper convention:
         * crossover width w swaps rows w and w + 1 (1-based).
         *
         * C zero-based row = w - 1, hence width = row + 1.
         */
        const uint64_t block_width =
            (uint64_t)swaps[i].row + 1u;

        if (block_width > (uint64_t)INT32_MAX - width) {
            return false;
        }

        width += block_width;
    }

    *out_width = (int32_t)width;
    return true;
}

static bool copy_swaps(
    const AdjacentSwap *swaps,
    size_t swap_count,
    AdjacentSwap **out_copy
)
{
    if (out_copy == NULL) {
        return false;
    }

    *out_copy = NULL;

    if (swap_count == 0) {
        return true;
    }

    if (swaps == NULL) {
        return false;
    }

    if (swap_count > SIZE_MAX / sizeof(*swaps)) {
        return false;
    }

    AdjacentSwap *copy =
        malloc(swap_count * sizeof(*copy));

    if (copy == NULL) {
        return false;
    }

    memcpy(copy, swaps, swap_count * sizeof(*copy));
    *out_copy = copy;

    return true;
}

void yang_zhang_layout_destroy(YangZhangLayout *layout)
{
    if (layout == NULL) {
        return;
    }

    free(layout->swaps);
    *layout = (YangZhangLayout){0};
}

bool yang_zhang_layout_init(
    YangZhangLayout *layout,
    uint32_t variable_count,
    const AdjacentSwap *swaps,
    size_t swap_count
)
{
    if (layout == NULL) {
        return false;
    }

    if (variable_count == 0 ||
        variable_count > YANG_ZHANG_MAX_VARIABLES) {
        return false;
    }

    if (swap_count > 0 && swaps == NULL) {
        return false;
    }

    int32_t height = 0;
    if (!compute_height(variable_count, &height)) {
        return false;
    }

    int32_t crossover_width = 0;
    if (!compute_crossover_width(
            swaps,
            swap_count,
            height,
            &crossover_width)) {
        return false;
    }

    /*
     * Coarse layout used by this project:
     *
     * [V] [FF] [ crossover chain ] [FF] [clauses]
     *
     * The two forwarder bands are a project convention for clearer,
     * explicit signal entry/exit boundaries. They are not presented as
     * a mathematical necessity of the Yang-Zhang construction.
     */
    const uint64_t total_width =
        (uint64_t)YANG_ZHANG_VARIABLE_WIDTH +
        (uint64_t)YANG_ZHANG_LEFT_FORWARD_WIDTH +
        (uint64_t)crossover_width +
        (uint64_t)YANG_ZHANG_RIGHT_FORWARD_WIDTH +
        (uint64_t)YANG_ZHANG_CLAUSE_WIDTH;

    if (total_width > (uint64_t)INT32_MAX) {
        return false;
    }

    AdjacentSwap *swap_copy = NULL;
    if (!copy_swaps(swaps, swap_count, &swap_copy)) {
        return false;
    }

    /*
     * Commit only after every fallible operation succeeded.
     *
     * Precondition: layout is not already a live initialized object.
     */
    *layout = (YangZhangLayout){
        .height = height,
        .width = (int32_t)total_width,
        .swap_count = swap_count,
        .swaps = swap_copy,
    };

    return true;
}
