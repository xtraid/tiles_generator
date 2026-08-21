"""Independent pure Python checker for Wang-tiling witnesses."""

from collections.abc import Sequence

from model.region import Region
from model.tileset import COLOR_NONE, E, N, S, W, Tileset


def is_valid_tiling(
    region: Region,
    tileset: Tileset,
    tiling: Sequence[int | None],
) -> bool:
    """Check dense storage, boundaries, and adjacency without using Z3."""
    if len(tiling) != len(region.active):
        return False

    for index, (active, tile_id) in enumerate(
        zip(region.active, tiling, strict=True)
    ):
        if not active:
            if tile_id is not None:
                return False
            continue

        if type(tile_id) is not int or tile_id < 0 or tile_id >= len(tileset):
            return False
        tile = tileset[tile_id]
        for direction, required_color in enumerate(region.boundary[index]):
            if (
                required_color != COLOR_NONE
                and tile[direction] != required_color
            ):
                return False

    for y in range(region.height):
        for x in range(region.width):
            index = y * region.width + x
            if not region.active[index]:
                continue

            tile_id = tiling[index]
            assert type(tile_id) is int
            tile = tileset[tile_id]

            if x + 1 < region.width and region.active[index + 1]:
                right_id = tiling[index + 1]
                assert type(right_id) is int
                if tile[E] != tileset[right_id][W]:
                    return False

            if y + 1 < region.height and region.active[index + region.width]:
                below_id = tiling[index + region.width]
                assert type(below_id) is int
                if tile[S] != tileset[below_id][N]:
                    return False

    return True
