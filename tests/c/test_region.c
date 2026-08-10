#include "wang/region.h"

#include <assert.h>
#include <stdio.h>

static void test_init_valid_region(void)
{
    Region region;

    assert(region_init(&region, 3, 2));
    assert(region.width == 3);
    assert(region.height == 2);
    assert(region.cells != NULL);

    for (int32_t y = 0; y < region.height; ++y) {
        for (int32_t x = 0; x < region.width; ++x) {
            const RegionCell *cell = region_cell_const(&region, x, y);

            assert(cell != NULL);
            assert(!cell->active);

            for (Dir dir = N; dir < DIR_COUNT; ++dir) {
                assert(cell->boundary[dir] == COLOR_NONE);
            }
        }
    }

    region_destroy(&region);
}

int main(void)
{
    test_init_valid_region();

    puts("test_region: OK");
    return 0;
}
