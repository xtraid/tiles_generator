#ifndef WANG_YANG_ZHANG_H
#define WANG_YANG_ZHANG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wang/permutation.h"

/*
 * Yang-Zhang layout conventions used by this project.
 *
 * Paper geometry:
 *   - signal height = 4n - 1
 *   - a paper crossover of width w swaps rows w and w + 1 (1-based)
 *
 * C representation:
 *   AdjacentSwap.row = w - 1
 *   crossover width  = row + 1
 *
 * Project convention:
 *   two forwarder columns are kept before and after the crossover chain.
 *   They are NOT claimed to be mandatory in the paper; they are explicit
 *   neutral signal-propagation bands used to make gadget boundaries and
 *   signal entry/exit points easier to inspect.
 */
#define YANG_ZHANG_MAX_VARIABLES \
    ((uint32_t)((((uint64_t)INT32_MAX) + 1u) / 4u))

#define YANG_ZHANG_VARIABLE_WIDTH       1u
#define YANG_ZHANG_LEFT_FORWARD_WIDTH   2u
#define YANG_ZHANG_RIGHT_FORWARD_WIDTH  2u
#define YANG_ZHANG_CLAUSE_WIDTH         2u

/*
 * Compute the dimensions of the coarse project layout.
 *
 * On success:
 *
 *   height = 4 * variable_count - 1
 *
 *   width =
 *       YANG_ZHANG_VARIABLE_WIDTH
 *     + YANG_ZHANG_LEFT_FORWARD_WIDTH
 *     + sum(swaps[i].row + 1)
 *     + YANG_ZHANG_RIGHT_FORWARD_WIDTH
 *     + YANG_ZHANG_CLAUSE_WIDTH
 *
 * The swap sequence is borrowed for the duration of this call. It is never
 * copied, modified, or released here.
 */
bool yang_zhang_compute_dimensions(
    uint32_t variable_count,
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t *out_height,
    int32_t *out_width
);

#endif /* WANG_YANG_ZHANG_H */
