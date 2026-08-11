#include "wang/region.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#define STRESS_MAX_WIDTH  16
#define STRESS_MAX_HEIGHT 16
#define STRESS_CASES      128
#define STRESS_ACTIONS    512

typedef struct {
    bool active;
    ColorId boundary[DIR_COUNT];
} ReferenceCell;

static uint32_t random_state = UINT32_C(0x6d2b79f5);

static uint32_t next_random(void)
{
    uint32_t value = random_state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random_state = value;
    return value;
}

static bool reference_in_bounds(
    int32_t width,
    int32_t height,
    int32_t x,
    int32_t y
)
{
    return x >= 0 && y >= 0 && x < width && y < height;
}

static size_t reference_index(int32_t width, int32_t x, int32_t y)
{
    return (size_t)y * (size_t)width + (size_t)x;
}

static void assert_matches_reference(
    const Region *region,
    const ReferenceCell *reference
)
{
    for (int32_t y = 0; y < region->height; ++y) {
        for (int32_t x = 0; x < region->width; ++x) {
            const size_t index = reference_index(region->width, x, y);
            const RegionCell *cell = region_cell_const(region, x, y);

            assert(cell != NULL);
            assert(cell->active == reference[index].active);

            for (Dir dir = N; dir < DIR_COUNT; ++dir) {
                assert(cell->boundary[dir] == reference[index].boundary[dir]);
            }
        }
    }
}

static void test_lifetime_and_initial_state(void)
{
    assert(!region_init(NULL, 1, 1));

    Region invalid = { .width = 9, .height = 9, .cells = NULL };

    assert(!region_init(&invalid, 0, 1));
    assert(invalid.width == 0);
    assert(invalid.height == 0);
    assert(invalid.cells == NULL);
    assert(!region_init(&invalid, 1, 0));
    assert(!region_init(&invalid, -1, 1));
    assert(!region_init(&invalid, 1, -1));
    assert(!region_init(&invalid, INT32_MAX, INT32_MAX));

    Region region;

    assert(region_init(&region, 3, 2));
    assert(region.width == 3);
    assert(region.height == 2);
    assert(region.cell_count == 6);
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
    assert(region.width == 0);
    assert(region.height == 0);
    assert(region.cell_count == 0);
    assert(region.cells == NULL);

    region_destroy(&region);
    region_destroy(NULL);
}

static void test_coordinate_api_exhaustively(void)
{
    Region region;

    assert(region_init(&region, 7, 5));

    for (int32_t y = -2; y <= region.height + 1; ++y) {
        for (int32_t x = -2; x <= region.width + 1; ++x) {
            const bool expected = reference_in_bounds(
                region.width,
                region.height,
                x,
                y
            );

            assert(region_in_bounds(&region, x, y) == expected);
            assert((region_cell(&region, x, y) != NULL) == expected);
            assert((region_cell_const(&region, x, y) != NULL) == expected);

            if (expected) {
                const size_t index = reference_index(region.width, x, y);

                assert(region_index(&region, x, y) == index);
                assert(region_cell(&region, x, y) == &region.cells[index]);
            }
        }
    }

    assert(!region_in_bounds(NULL, 0, 0));
    assert(region_cell(NULL, 0, 0) == NULL);
    assert(region_cell_const(NULL, 0, 0) == NULL);

    Region destroyed = {0};
    assert(!region_in_bounds(&destroyed, 0, 0));

    region_destroy(&region);
}

static void test_region_validation(void)
{
    Region region = {0};
    assert(!region_validate(NULL));
    assert(!region_validate(&region));

    assert(region_init(&region, 2, 1));
    assert(region_validate(&region));
    assert(region_set_active(&region, 0, 0, true));
    assert(region_set_active(&region, 1, 0, true));
    assert(region_validate(&region));

    region.cells[0].boundary[E] = COLOR_0;
    assert(!region_validate(&region));
    region.cells[0].boundary[E] = COLOR_NONE;

    region.cells[0].boundary[N] = (ColorId)COLOR_COUNT;
    assert(!region_validate(&region));
    region.cells[0].boundary[N] = COLOR_NONE;
    assert(region_validate(&region));

    const int32_t original_width = region.width;
    region.width = original_width + 1;
    assert(!region_validate(&region));
    assert(!region_in_bounds(&region, 0, 0));
    assert(region_cell(&region, 0, 0) == NULL);
    region.width = original_width;

    const size_t original_cell_count = region.cell_count;
    region.cell_count = original_cell_count - 1;
    assert(!region_validate(&region));
    assert(region_cell_const(&region, 0, 0) == NULL);
    region.cell_count = original_cell_count;
    assert(region_validate(&region));

    region_destroy(&region);
}

static bool reference_set_boundary(
    ReferenceCell *reference,
    int32_t width,
    int32_t height,
    int32_t x,
    int32_t y,
    Dir dir,
    ColorId color
)
{
    if ((unsigned)dir >= (unsigned)DIR_COUNT
        || (color != COLOR_NONE && color >= COLOR_COUNT)
        || !reference_in_bounds(width, height, x, y)) {
        return false;
    }

    ReferenceCell *cell = &reference[reference_index(width, x, y)];
    if (!cell->active) {
        return false;
    }

    static const int32_t dx[DIR_COUNT] = { 0, 1, 0, -1 };
    static const int32_t dy[DIR_COUNT] = { -1, 0, 1, 0 };
    const int32_t neighbor_x = x + dx[dir];
    const int32_t neighbor_y = y + dy[dir];

    if (reference_in_bounds(width, height, neighbor_x, neighbor_y)
        && reference[reference_index(width, neighbor_x, neighbor_y)].active) {
        return false;
    }

    cell->boundary[dir] = color;
    return true;
}

static void test_deterministic_api_stress(void)
{
    ReferenceCell reference[STRESS_MAX_WIDTH * STRESS_MAX_HEIGHT];

    for (size_t test_case = 0; test_case < STRESS_CASES; ++test_case) {
        const int32_t width = 1 + (int32_t)(next_random() % STRESS_MAX_WIDTH);
        const int32_t height = 1 + (int32_t)(next_random() % STRESS_MAX_HEIGHT);
        const size_t cell_count = (size_t)width * (size_t)height;
        Region region;

        assert(region_init(&region, width, height));

        for (size_t i = 0; i < cell_count; ++i) {
            reference[i].active = false;
            for (Dir dir = N; dir < DIR_COUNT; ++dir) {
                reference[i].boundary[dir] = COLOR_NONE;
            }
        }

        for (size_t action = 0; action < STRESS_ACTIONS; ++action) {
            const int32_t x = (int32_t)(next_random() % (uint32_t)(width + 4)) - 2;
            const int32_t y = (int32_t)(next_random() % (uint32_t)(height + 4)) - 2;
            const bool active = (next_random() & 1u) != 0;
            const bool expected = reference_in_bounds(width, height, x, y);

            assert(region_set_active(&region, x, y, active) == expected);
            if (expected) {
                reference[reference_index(width, x, y)].active = active;
            }
            assert_matches_reference(&region, reference);
        }

        for (size_t action = 0; action < STRESS_ACTIONS; ++action) {
            const int32_t x = (int32_t)(next_random() % (uint32_t)(width + 4)) - 2;
            const int32_t y = (int32_t)(next_random() % (uint32_t)(height + 4)) - 2;
            const uint32_t dir_choice = next_random() % (DIR_COUNT + 2u);
            const uint32_t color_choice = next_random() % (COLOR_COUNT + 2u);
            const Dir dir = dir_choice == DIR_COUNT + 1u
                ? (Dir)-1
                : (Dir)dir_choice;
            const ColorId color = color_choice == COLOR_COUNT + 1u
                ? COLOR_NONE
                : (ColorId)color_choice;
            const bool expected = reference_set_boundary(
                reference,
                width,
                height,
                x,
                y,
                dir,
                color
            );

            assert(region_set_boundary(&region, x, y, dir, color) == expected);
            assert_matches_reference(&region, reference);
        }

        region_destroy(&region);
    }
}

int main(void)
{
    test_lifetime_and_initial_state();
    test_coordinate_api_exhaustively();
    test_region_validation();
    test_deterministic_api_stress();

    puts("test_region: OK");
    return 0;
}
