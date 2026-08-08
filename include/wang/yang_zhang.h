#ifndef WANG_YANG_ZHANG_H
#define WANG_YANG_ZHANG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Maximum n such that 4n - 1 fits in int32_t. */
#define YANG_ZHANG_MAX_VARIABLES ((uint32_t)INT32_MAX / 4u + 1u) // magia nera
#define YANG_ZHANG_VARIABLE_WIDTH       1u
#define YANG_ZHANG_LEFT_FORWARD_WIDTH   2u
#define YANG_ZHANG_RIGHT_FORWARD_WIDTH  2u
#define YANG_ZHANG_CLAUSE_WIDTH         2u


typedef struct {
    uint32_t row;   /* 0-based: swap row with row + 1 */
} AdjacentSwap;

typedef struct {
    int32_t height;
    int32_t width;

    size_t swap_count;
    AdjacentSwap *swaps;
} YangZhangLayout;

bool yang_zhang_layout_init(
    YangZhangLayout *layout,
    uint32_t variable_count,
    const AdjacentSwap *swaps,
    size_t swap_count
);

void yang_zhang_layout_destroy(YangZhangLayout *layout);


#endif /* WANG_YANG_ZHANG_H */
