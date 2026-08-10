#ifndef WANG_PERMUTATION_H
#define WANG_PERMUTATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t row; /* 0-based: swaps row with row + 1 */
} AdjacentSwap;

/*
 * A signal is one logical row of the Yang-Zhang construction.
 *
 * Variable occurrences are distinct because the three occurrences of the
 * same variable may have to be routed to different clause rows.
 *
 * Redundant rows are also represented as tokens so that source and target
 * are permutations of the same set of objects.
 */
typedef enum {
    SIGNAL_VARIABLE,
    SIGNAL_REDUNDANT
} SignalKind;

typedef struct {
    SignalKind kind;

    uint32_t token_id;

    /* Meaningful only for SIGNAL_VARIABLE; 0-based and in the range 0..2. */
    uint32_t variable;
    uint8_t occurrence;
} SignalToken;


/*
 * Compute the adjacent swaps that transform source into target.
 *
 * Each emitted AdjacentSwap uses this convention:
 *
 *     swap.row == r
 *
 * means exchanging rows r and r + 1.
 *
 * On success:
 *   - *out_swaps points to a newly allocated array;
 *   - *out_swap_count is its number of elements;
 *   - applying the swaps in order transforms source exactly into target.
 *
 * An empty permutation succeeds with *out_swaps == NULL and
 * *out_swap_count == 0. On failure, the outputs are also reset to NULL/0.
 *
 * The caller owns *out_swaps and must release it with free().
 */
bool yang_zhang_permutation_build(
    const SignalToken *source,
    const SignalToken *target,
    size_t signal_count,
    AdjacentSwap **out_swaps,
    size_t *out_swap_count
);


/*
 * Apply a sequence of adjacent swaps in place.
 *
 * The complete swap sequence is validated before signals is modified, so a
 * failure never leaves a partially permuted array.
 *
 * Mainly useful for validation and tests, but it operates on the same
 * public types used by the builder.
 */
bool yang_zhang_permutation_apply(
    SignalToken *signals,
    size_t signal_count,
    const AdjacentSwap *swaps,
    size_t swap_count
);

#endif /* WANG_PERMUTATION_H */
