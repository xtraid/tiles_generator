#include "wang/yang_zhang.h"

bool yang_zhang_compute_dimensions(
    uint32_t variable_count,
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t *out_height,
    int32_t *out_width
)
{
    if (out_height == NULL || out_width == NULL) {
        return false;
    }

    if (variable_count == 0 ||
        variable_count > YANG_ZHANG_MAX_VARIABLES ||
        (swap_count > 0 && swaps == NULL)) {
        return false;
    }

    const uint64_t height =
        4u * (uint64_t)variable_count - 1u;

    if (height > (uint64_t)INT32_MAX) {
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
    uint64_t width =
        (uint64_t)YANG_ZHANG_VARIABLE_WIDTH +
        (uint64_t)YANG_ZHANG_LEFT_FORWARD_WIDTH +
        (uint64_t)YANG_ZHANG_RIGHT_FORWARD_WIDTH +
        (uint64_t)YANG_ZHANG_CLAUSE_WIDTH;

    for (size_t i = 0; i < swap_count; ++i) {
        /*
         * row is 0-based and swaps row with row + 1.
         * The final row therefore cannot be used as swap.row.
         */
        if ((uint64_t)swaps[i].row >= height - 1u) {
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

        if (width > (uint64_t)INT32_MAX ||
            block_width > (uint64_t)INT32_MAX - width) {
            return false;
        }

        width += block_width;
    }

    if (width > (uint64_t)INT32_MAX) {
        return false;
    }

    *out_height = (int32_t)height;
    *out_width = (int32_t)width;
    return true;
}
