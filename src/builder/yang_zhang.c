#include "wang/yang_zhang.h"

#include <stdlib.h>
#include <string.h>

static bool compute_crossover_width(
    const AdjacentSwap *swaps,
    size_t swap_count,
    int32_t height,
    int32_t *out_width
)
{
    if (out_width == NULL) {
        return false;
    }

    if (height <= 0) {
        return false;
    }

    if (swap_count > 0 && swaps == NULL) {
        return false;
    }

    uint64_t width = 0;

    for (size_t i = 0; i < swap_count; ++i) {

        if (swaps[i].row >= (uint32_t)(height - 1)) {
            return false;
        }

        width += (uint64_t)swaps[i].row + 1u;

        if (width > INT32_MAX) {
            return false;
        }
    }

    *out_width = (int32_t)width;

    return true;
}

static bool copy_swaps(
    YangZhangLayout *layout,
    const AdjacentSwap *swaps,
    size_t swap_count
)
{
    if (layout == NULL) {
        return false;
    }

    if (swap_count == 0) {
        layout->swaps = NULL;
        layout->swap_count = 0;
        return true;
    }

    if (swaps == NULL) {
        return false;
    }

    if (swap_count > SIZE_MAX / sizeof(AdjacentSwap)) {
        return false;
    }

    layout->swaps = malloc(
        swap_count * sizeof(AdjacentSwap)
    );

    if (layout->swaps == NULL) {
        return false;
    }

    memcpy(
        layout->swaps,
        swaps,
        swap_count * sizeof(AdjacentSwap)
    );

    layout->swap_count = swap_count;

    return true;
}


/*
 * [V] [F F] [ crossover area ] [F F] [clauses]
    1    2                       2       2    */

void yang_zhang_layout_destroy(YangZhangLayout *layout)
{
    if (layout == NULL) {
        return;
    }

    free(layout->swaps);

    layout->swaps = NULL;
    layout->swap_count = 0;
    layout->height = 0;
    layout->width = 0;
}

bool yang_zhang_layout_init(
    YangZhangLayout *layout,
    uint32_t variable_count,
    const AdjacentSwap *swaps,
    size_t swap_count
)
{
    if (layout == NULL || variable_count == 0) {
        return false;
    }

    if (swap_count > 0 && swaps == NULL) {
        return false;
    }

    layout->height = 0;
    layout->width = 0;
    layout->swap_count = 0;
    layout->swaps = NULL;

  if (variable_count > YANG_ZHANG_MAX_VARIABLES) {
    return false;
  }
  layout->height = (int32_t)(4 * variable_count - 1);
  int32_t crossover_width;
  if (!compute_crossover_width(
        swaps,
        swap_count,
        layout->height,
        &crossover_width))
  {
    return false;
  }
  uint64_t total_width =
    (uint64_t)YANG_ZHANG_VARIABLE_WIDTH +
    (uint64_t)YANG_ZHANG_LEFT_FORWARD_WIDTH +
    (uint64_t)crossover_width +
    (uint64_t)YANG_ZHANG_RIGHT_FORWARD_WIDTH +
    (uint64_t)YANG_ZHANG_CLAUSE_WIDTH;

  if (total_width > INT32_MAX) {
    return false;
  }

  layout->width = (int32_t)total_width;
  if (!copy_swaps(layout, swaps, swap_count)) {
      return false;
    }

  return true;
}
