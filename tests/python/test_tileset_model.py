import unittest

from model.tileset import (
    COLOR_0,
    COLOR_B,
    COLOR_COUNT,
    COLOR_V,
    COLOR_V0_A,
    DIR_COUNT,
    TILESET,
    TILE_COUNT,
    TILE_F0,
    TILE_V0_TOP,
)


class TilesetModelTests(unittest.TestCase):
    def test_has_23_immutable_atomic_tiles(self) -> None:
        self.assertIs(type(TILESET), tuple)
        self.assertEqual(TILE_COUNT, 23)
        self.assertEqual(len(TILESET), TILE_COUNT)

        for tile in TILESET:
            self.assertIs(type(tile), tuple)
            self.assertEqual(len(tile), DIR_COUNT)
            self.assertTrue(all(0 <= color < COLOR_COUNT for color in tile))

    def test_preserves_positional_tile_ids_and_edge_order(self) -> None:
        self.assertEqual(TILE_V0_TOP, 0)
        self.assertEqual(TILE_F0, 7)
        self.assertEqual(
            TILESET[TILE_V0_TOP],
            (COLOR_B, COLOR_0, COLOR_V0_A, COLOR_V),
        )
        self.assertEqual(
            TILESET[TILE_F0],
            (COLOR_B, COLOR_0, COLOR_B, COLOR_0),
        )


if __name__ == "__main__":
    unittest.main()
