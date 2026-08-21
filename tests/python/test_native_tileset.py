from ctypes import Structure, c_int, c_uint8
import unittest

from model.tileset import DIR_COUNT, TILESET, TILE_COUNT
from native._lib import library


class _WangTile(Structure):
    _fields_ = [
        ("id", c_uint8),
        ("edge", c_uint8 * DIR_COUNT),
        ("kind", c_int),
    ]


class NativeTilesetParityTests(unittest.TestCase):
    def test_python_tileset_matches_every_native_id_and_edge(self) -> None:
        native_tileset = (_WangTile * TILE_COUNT).in_dll(
            library(),
            "TILESET",
        )

        for tile_id, (native, python_edges) in enumerate(
            zip(native_tileset, TILESET, strict=True)
        ):
            with self.subTest(tile_id=tile_id):
                self.assertEqual(native.id, tile_id)
                self.assertEqual(tuple(native.edge), python_edges)


if __name__ == "__main__":
    unittest.main()
