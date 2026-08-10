#include "wang/permutation.h"

#include <stdlib.h>
#include <string.h>

static void single_swap(SignalToken *top, SignalToken *bottom)
{
    /* Swap the complete tokens: identity and metadata must move together. */
    SignalToken tmp = *top;
    *top = *bottom;
    *bottom = tmp;
}

/*
 * Build a routing program, not a second token representation.
 *
 * SignalToken.token_id is the identity used to match source with target.
 * AdjacentSwap deliberately stores only a row: it says where two complete
 * SignalToken objects must exchange places, never which tokens they contain.
 *
 * The temporary array is the only mutable permutation. source and target are
 * read-only specifications; result records every move applied to that copy.
 */
bool yang_zhang_permutation_build(
    const SignalToken *source,
    const SignalToken *target,
    size_t signal_count,
    AdjacentSwap **out_swaps,
    size_t *out_swap_count
)
{
    SignalToken *tmp = NULL;
    AdjacentSwap *result = NULL;
    size_t result_count = 0;
    size_t result_capacity = 0;

    if (out_swaps == NULL || out_swap_count == NULL) {
        return false;
    }

    *out_swaps = NULL;
    *out_swap_count = 0;

    if (signal_count == 0) {
        return true;
    }

    if (source == NULL || target == NULL) {
        return false;
    }

    if (signal_count > SIZE_MAX / sizeof(*tmp)) {
        return false;
    }

    /* Ambiguous identities are invalid: routing duplicate IDs is undefined. */
    for (size_t i = 0; i < signal_count; ++i) {
        if ((source[i].kind != SIGNAL_VARIABLE &&
             source[i].kind != SIGNAL_REDUNDANT) ||
            (target[i].kind != SIGNAL_VARIABLE &&
             target[i].kind != SIGNAL_REDUNDANT) ||
            (source[i].kind == SIGNAL_VARIABLE &&
             source[i].occurrence >= 3u) ||
            (target[i].kind == SIGNAL_VARIABLE &&
             target[i].occurrence >= 3u)) {
            return false;
        }

        for (size_t j = i + 1; j < signal_count; ++j) {
            if (source[i].token_id == source[j].token_id ||
                target[i].token_id == target[j].token_id) {
                return false;
            }
        }
    }

    tmp = malloc(signal_count * sizeof(*tmp));
    if (tmp == NULL) {
        return false;
    }

    memcpy(tmp, source, signal_count * sizeof(*tmp));

    for (size_t i = 0; i < signal_count; ++i) {
        size_t j = i;

        /* Find the exact token required by target at position i. */
        while (j < signal_count &&
               tmp[j].token_id != target[i].token_id) {
            ++j;
        }

        if (j == signal_count) {
            goto fail;
        }

        if (tmp[j].kind != target[i].kind) {
            goto fail;
        }

        if (tmp[j].kind == SIGNAL_VARIABLE &&
            (tmp[j].variable != target[i].variable ||
             tmp[j].occurrence != target[i].occurrence)) {
            goto fail;
        }

        /* Bubble that token upward and record the same adjacent moves. */
        while (j > i) {
            const size_t row = j - 1;

            if (row > UINT32_MAX) {
                goto fail;
            }

            if (result_count == result_capacity) {
                const size_t max_capacity =
                    SIZE_MAX / sizeof(*result);
                size_t new_capacity;

                if (result_capacity == 0) {
                    new_capacity =
                        max_capacity < 16 ? max_capacity : 16;
                } else if (result_capacity > max_capacity / 2) {
                    new_capacity = max_capacity;
                } else {
                    new_capacity = result_capacity * 2;
                }

                if (new_capacity <= result_capacity) {
                    goto fail;
                }

                AdjacentSwap *grown = realloc(
                    result,
                    new_capacity * sizeof(*result)
                );

                if (grown == NULL) {
                    goto fail;
                }

                result = grown;
                result_capacity = new_capacity;
            }

            result[result_count++] = (AdjacentSwap){
                .row = (uint32_t)row,
            };

            single_swap(&tmp[row], &tmp[row + 1]);
            --j;
        }
    }

    free(tmp);

    *out_swaps = result;
    *out_swap_count = result_count;
    return true;

fail:
    free(result);
    free(tmp);
    return false;
}

/*
 * Execute the routing program in place.
 * signals owns the mutable SignalToken sequence; swaps is a read-only list of
 * row operations. This function neither allocates memory nor interprets token
 * identities: it validates the rows, then moves the complete structs.
 */
bool yang_zhang_permutation_apply(
    SignalToken *signals,
    size_t signal_count,
    const AdjacentSwap *swaps,
    size_t swap_count
)
{
    if (signal_count > 0 && signals == NULL) {
        return false;
    }

    if (swap_count > 0 && swaps == NULL) {
        return false;
    }

    if (signal_count == 0) {
        return swap_count == 0;
    }

    /*
     * Validate the whole program first. Do not merge this with execution:
     * one bad row must not leave signals half-permuted.
     */
    for (size_t i = 0; i < swap_count; ++i) {
        if ((size_t)swaps[i].row >= signal_count - 1) {
            return false;
        }
    }

    /* AdjacentSwap has no token semantics; it moves complete structs by row. */
    for (size_t i = 0; i < swap_count; ++i) {
        const size_t row = swaps[i].row;
        single_swap(&signals[row], &signals[row + 1]);
    }

    return true;
}
