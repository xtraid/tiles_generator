#include "wang/verify.h"

#include <assert.h>
#include <stdio.h>

static void activate_all(Region *region)
{
    for (int32_t y = 0; y < region->height; ++y) {
        for (int32_t x = 0; x < region->width; ++x) {
            assert(region_set_active(region, x, y, true));
        }
    }
}

static void test_single_cell_and_boundary_mismatches(void)
{
    Region region = {0};
    const TileId tiles[] = { TILE_F0 };

    assert(region_init(&region, 1, 1));
    activate_all(&region);
    assert(region_set_boundary(&region, 0, 0, N, COLOR_B));
    assert(region_set_boundary(&region, 0, 0, E, COLOR_0));
    assert(region_set_boundary(&region, 0, 0, S, COLOR_B));
    assert(region_set_boundary(&region, 0, 0, W, COLOR_0));
    assert(wang_verify_tiling(&region, tiles, 1) == WANG_VERIFY_VALID);

    for (Dir dir = N; dir < DIR_COUNT; ++dir) {
        const ColorId original = region.cells[0].boundary[dir];
        region.cells[0].boundary[dir] = original == COLOR_1
            ? COLOR_0
            : COLOR_1;
        assert(wang_verify_tiling(&region, tiles, 1) ==
               WANG_VERIFY_BOUNDARY_MISMATCH);
        region.cells[0].boundary[dir] = original;
    }

    region_destroy(&region);
}

static void test_horizontal_and_vertical_adjacency(void)
{
    Region horizontal = {0};
    assert(region_init(&horizontal, 2, 1));
    activate_all(&horizontal);

    const TileId matching_horizontal[] = { TILE_F0, TILE_F0 };
    const TileId mismatching_horizontal[] = { TILE_F0, TILE_F1 };
    assert(wang_verify_tiling(&horizontal, matching_horizontal, 2) ==
           WANG_VERIFY_VALID);
    assert(wang_verify_tiling(&horizontal, mismatching_horizontal, 2) ==
           WANG_VERIFY_ADJACENCY_MISMATCH);
    region_destroy(&horizontal);

    Region vertical = {0};
    assert(region_init(&vertical, 1, 2));
    activate_all(&vertical);

    const TileId matching_vertical[] = { TILE_F0, TILE_F1 };
    const TileId mismatching_vertical[] = { TILE_F0, TILE_L0 };
    assert(wang_verify_tiling(&vertical, matching_vertical, 2) ==
           WANG_VERIFY_VALID);
    assert(wang_verify_tiling(&vertical, mismatching_vertical, 2) ==
           WANG_VERIFY_ADJACENCY_MISMATCH);
    region_destroy(&vertical);
}

static void test_incomplete_invalid_and_inactive_assignments(void)
{
    Region region = {0};
    assert(region_init(&region, 3, 1));
    assert(region_set_active(&region, 0, 0, true));
    assert(region_set_active(&region, 2, 0, true));

    const TileId valid[] = { TILE_F0, TILE_NONE, TILE_F1 };
    const TileId incomplete[] = { TILE_NONE, TILE_NONE, TILE_F1 };
    const TileId invalid[] = { TILE_COUNT, TILE_NONE, TILE_F1 };
    const TileId inactive_assigned[] = { TILE_F0, TILE_F0, TILE_F1 };

    assert(wang_verify_tiling(&region, valid, 3) == WANG_VERIFY_VALID);
    assert(wang_verify_tiling(&region, incomplete, 3) ==
           WANG_VERIFY_INCOMPLETE);
    assert(wang_verify_tiling(&region, invalid, 3) ==
           WANG_VERIFY_INVALID_TILE_ID);
    assert(wang_verify_tiling(&region, inactive_assigned, 3) ==
           WANG_VERIFY_INACTIVE_ASSIGNED);
    assert(wang_verify_tiling(&region, valid, 2) ==
           WANG_VERIFY_INVALID_LENGTH);
    assert(wang_verify_tiling(NULL, valid, 3) ==
           WANG_VERIFY_INVALID_ARGUMENT);
    assert(wang_verify_tiling(&region, NULL, 3) ==
           WANG_VERIFY_INVALID_ARGUMENT);

    region_destroy(&region);
}

static void test_rejects_invalid_region_constraints(void)
{
    Region region = {0};
    const TileId tiles[] = { TILE_F0, TILE_F0 };

    assert(region_init(&region, 2, 1));
    activate_all(&region);

    region.cells[0].boundary[E] = COLOR_0;
    assert(wang_verify_tiling(&region, tiles, 2) ==
           WANG_VERIFY_INVALID_REGION);
    region.cells[0].boundary[E] = COLOR_NONE;

    region.cells[0].boundary[N] = (ColorId)COLOR_COUNT;
    assert(wang_verify_tiling(&region, tiles, 2) ==
           WANG_VERIFY_INVALID_REGION);
    region.cells[0].boundary[N] = COLOR_NONE;

    region.cells[1].active = false;
    region.cells[1].boundary[E] = COLOR_B;
    const TileId with_inactive[] = { TILE_F0, TILE_NONE };
    assert(wang_verify_tiling(&region, with_inactive, 2) ==
           WANG_VERIFY_INVALID_REGION);

    region_destroy(&region);
}

int main(void)
{
    test_single_cell_and_boundary_mismatches();
    test_horizontal_and_vertical_adjacency();
    test_incomplete_invalid_and_inactive_assignments();
    test_rejects_invalid_region_constraints();

    puts("test_verify: OK");
    return 0;
}
