#ifndef WANG_REGION_H
#define WANG_REGION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wang/tile.h"

/*
 * One entry in the dense row-major bounding box of a Region.
 *
 * An active cell belongs to the tiled region. boundary[d] is either the
 * required color on an exposed side or COLOR_NONE when that side has no
 * boundary constraint.
 */
typedef struct {
    bool active;
    ColorId boundary[DIR_COUNT];
} RegionCell;

/*
 * Finite square-grid geometry stored in a dense width-by-height array.
 * Region owns cells; callers borrow pointers returned by the accessors.
 */
typedef struct {
    int32_t width;
    int32_t height;
    RegionCell *cells;
} Region;

/*
 * Allocate an empty width-by-height region.
 *
 * Every cell starts inactive and unconstrained. Returns false for a NULL
 * region, non-positive dimensions, an overflowing size, or allocation
 * failure. On failure, region is left in the destroyed state.
 *
 * Precondition: region does not currently own allocated storage.
 */
bool region_init(Region *region, int32_t width, int32_t height);

/* Release owned storage and reset region. Accepts NULL. */
void region_destroy(Region *region);

/* Return whether (x, y) lies inside the bounding box. */
bool region_in_bounds(const Region *region, int32_t x, int32_t y);

/*
 * Return the row-major index for an in-bounds coordinate.
 * The caller must first establish region_in_bounds(region, x, y).
 */
size_t region_index(const Region *region, int32_t x, int32_t y);

/* Return the cell at (x, y), or NULL when it is out of bounds. */
RegionCell *region_cell(Region *region, int32_t x, int32_t y);

const RegionCell *region_cell_const(
    const Region *region,
    int32_t x,
    int32_t y
);

/*
 * Change whether a cell belongs to the region while building its geometry.
 *
 * Returns false for an invalid region or coordinate. The active mask must be
 * completed before boundary constraints are assigned. Once construction is
 * complete, solvers and verifiers treat the Region as immutable.
 */
bool region_set_active(Region *region, int32_t x, int32_t y, bool active);

/*
 * Set or clear the boundary constraint on one exposed side of an active cell.
 * Pass COLOR_NONE to clear it.
 *
 * The active mask must already be complete.
 *
 * Returns false for invalid input or when the requested side touches another
 * active cell and therefore is not part of the region boundary.
 */
bool region_set_boundary(
    Region *region,
    int32_t x,
    int32_t y,
    Dir dir,
    ColorId color
);

#endif /* WANG_REGION_H */
