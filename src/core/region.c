#include "wang/region.h"

#include <stdlib.h>


/* =========================
 * Lifetime
 * ========================= */

bool region_init(Region *region, int32_t width, int32_t height)
{
    if (region == NULL) {
        return false;
    }

    region->width = 0;
    region->height = 0;
    region->cells = NULL;

    if (width <= 0 || height <= 0) {
        return false;
    }

    const size_t row_size = (size_t)width;
    const size_t row_count = (size_t)height;

    if (row_count > SIZE_MAX / row_size) {
        return false;
    }

    const size_t cell_count = row_size * row_count;

    if (cell_count > SIZE_MAX / sizeof(RegionCell)) {
        return false;
    }

    RegionCell *cells = malloc(cell_count * sizeof(*cells));
    if (cells == NULL) {
        return false;
    }

    for (size_t i = 0; i < cell_count; ++i) {
        cells[i].active = false;

        for (Dir dir = N; dir < DIR_COUNT; ++dir) {
            cells[i].boundary[dir] = COLOR_NONE;
        }
    }

    region->width = width;
    region->height = height;
    region->cells = cells;

    return true;
}

void region_destroy(Region *region)
{
    if (region == NULL) {
        return;
    }

    free(region->cells);
    region->width = 0;
    region->height = 0;
    region->cells = NULL;
}


/* =========================
 * Coordinate access
 * ========================= */

bool region_in_bounds(const Region *region, int32_t x, int32_t y)
{
    return region != NULL
        && region->cells != NULL
        && x >= 0
        && y >= 0
        && x < region->width
        && y < region->height;
}

size_t region_index(const Region *region, int32_t x, int32_t y)
{
    return (size_t)y * (size_t)region->width + (size_t)x;
}

RegionCell *region_cell(Region *region, int32_t x, int32_t y)
{
    if (!region_in_bounds(region, x, y)) {
        return NULL;
    }

    return &region->cells[region_index(region, x, y)];
}

const RegionCell *region_cell_const(
    const Region *region,
    int32_t x,
    int32_t y
)
{
    if (!region_in_bounds(region, x, y)) {
        return NULL;
    }

    return &region->cells[region_index(region, x, y)];
}


/* =========================
 * Geometry
 * ========================= */

bool region_set_active(Region *region, int32_t x, int32_t y, bool active)
{
    RegionCell *cell = region_cell(region, x, y);
    if (cell == NULL) {
        return false;
    }

    cell->active = active;
    return true;
}


/* =========================
 * Boundary constraints
 * ========================= */

bool region_set_boundary(
    Region *region,
    int32_t x,
    int32_t y,
    Dir dir,
    ColorId color
)
{
    if ((unsigned)dir >= (unsigned)DIR_COUNT) {
        return false;
    }

    if (color != COLOR_NONE && color >= COLOR_COUNT) {
        return false;
    }

    RegionCell *cell = region_cell(region, x, y);
    if (cell == NULL || !cell->active) {
        return false;
    }

    int32_t neighbor_x = x;
    int32_t neighbor_y = y;

    switch (dir) {
    case N:
        --neighbor_y;
        break;
    case E:
        ++neighbor_x;
        break;
    case S:
        ++neighbor_y;
        break;
    case W:
        --neighbor_x;
        break;
    case DIR_COUNT:
        return false;
    }

    const RegionCell *neighbor = region_cell_const(
        region,
        neighbor_x,
        neighbor_y
    );

    if (neighbor != NULL && neighbor->active) {
        return false;
    }

    cell->boundary[dir] = color;
    return true;
}
