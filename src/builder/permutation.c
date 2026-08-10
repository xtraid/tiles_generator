#include "wang/permutation.h"

#include <stdlib.h>
#include <string.h>

enum { INITIAL_SWAP_CAPACITY = 16 };

typedef struct {
    AdjacentSwap *items;
    size_t count;
    size_t capacity;
} SwapBuffer;

static bool token_is_valid(const SignalToken *token)
{
    return token->kind == SIGNAL_REDUNDANT ||
        (token->kind == SIGNAL_VARIABLE && token->occurrence < 3u);
}

static bool tokens_are_valid_and_unique(
    const SignalToken *tokens,
    size_t token_count
)
{
    for (size_t i = 0; i < token_count; ++i) {
        if (!token_is_valid(&tokens[i])) {
            return false;
        }

        for (size_t j = i + 1; j < token_count; ++j) {
            if (tokens[i].token_id == tokens[j].token_id) {
                return false;
            }
        }
    }

    return true;
}

static bool tokens_match(const SignalToken *left, const SignalToken *right)
{
    if (left->kind != right->kind || left->token_id != right->token_id) {
        return false;
    }

    return left->kind == SIGNAL_REDUNDANT ||
        (left->variable == right->variable &&
         left->occurrence == right->occurrence);
}

static bool swap_buffer_append(SwapBuffer *buffer, size_t row)
{
    if (row > UINT32_MAX) {
        return false;
    }

    if (buffer->count == buffer->capacity) {
        const size_t max_capacity = SIZE_MAX / sizeof(*buffer->items);
        size_t new_capacity;

        if (buffer->capacity == 0) {
            new_capacity = max_capacity < INITIAL_SWAP_CAPACITY
                ? max_capacity
                : INITIAL_SWAP_CAPACITY;
        } else if (buffer->capacity > max_capacity / 2) {
            new_capacity = max_capacity;
        } else {
            new_capacity = buffer->capacity * 2;
        }

        if (new_capacity <= buffer->capacity) {
            return false;
        }

        AdjacentSwap *items = realloc(
            buffer->items,
            new_capacity * sizeof(*buffer->items)
        );

        if (items == NULL) {
            return false;
        }

        buffer->items = items;
        buffer->capacity = new_capacity;
    }

    buffer->items[buffer->count++] = (AdjacentSwap){
        .row = (uint32_t)row,
    };
    return true;
}

static void swap_tokens(SignalToken *top, SignalToken *bottom)
{
    SignalToken tmp = *top;
    *top = *bottom;
    *bottom = tmp;
}

bool yang_zhang_permutation_build(
    const SignalToken *source,
    const SignalToken *target,
    size_t signal_count,
    AdjacentSwap **out_swaps,
    size_t *out_swap_count
)
{
    SignalToken *tmp = NULL;
    SwapBuffer swaps = {0};

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

    if (!tokens_are_valid_and_unique(source, signal_count) ||
        !tokens_are_valid_and_unique(target, signal_count)) {
        return false;
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

        if (!tokens_match(&tmp[j], &target[i])) {
            goto fail;
        }

        while (j > i) {
            const size_t row = j - 1;

            if (!swap_buffer_append(&swaps, row)) {
                goto fail;
            }

            swap_tokens(&tmp[row], &tmp[row + 1]);
            --j;
        }
    }

    free(tmp);

    *out_swaps = swaps.items;
    *out_swap_count = swaps.count;
    return true;

fail:
    free(swaps.items);
    free(tmp);
    return false;
}

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

    for (size_t i = 0; i < swap_count; ++i) {
        const size_t row = swaps[i].row;
        swap_tokens(&signals[row], &signals[row + 1]);
    }

    return true;
}
