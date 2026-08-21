import unittest

from model.region import Region
from model.tileset import (
    COLOR_0,
    COLOR_B,
    COLOR_COUNT,
    COLOR_NONE,
    COLOR_V,
    COLOR_V0_A,
    TILESET,
    TILE_F0,
    TILE_F1,
    TILE_L0,
)
from oracles.tiling_check import is_valid_tiling
from oracles.tiling_solver import (
    TilingSolveResult,
    TilingSolveStatus,
    solve_tiling,
)


NO_BOUNDARY = (COLOR_NONE, COLOR_NONE, COLOR_NONE, COLOR_NONE)


class TilingSolveResultTests(unittest.TestCase):
    def test_carries_a_tiling_only_for_sat(self) -> None:
        tiling = (7, None)

        self.assertEqual(
            TilingSolveResult(TilingSolveStatus.SAT, tiling).tiling,
            tiling,
        )
        self.assertIsNone(
            TilingSolveResult(TilingSolveStatus.UNSAT).tiling
        )
        self.assertIsNone(
            TilingSolveResult(TilingSolveStatus.UNKNOWN).tiling
        )

        with self.assertRaises(ValueError):
            TilingSolveResult(TilingSolveStatus.SAT)
        with self.assertRaises(ValueError):
            TilingSolveResult(TilingSolveStatus.UNSAT, tiling)


class TilingCheckerTests(unittest.TestCase):
    def test_accepts_a_valid_boundary_constrained_tiling(self) -> None:
        region = Region(
            width=1,
            height=1,
            active=(True,),
            boundary=((COLOR_B, COLOR_0, COLOR_B, COLOR_0),),
        )

        self.assertTrue(is_valid_tiling(region, TILESET, (TILE_F0,)))
        self.assertFalse(is_valid_tiling(region, TILESET, (TILE_F1,)))

    def test_rejects_invalid_dense_witness_storage(self) -> None:
        region = Region(
            width=2,
            height=1,
            active=(True, False),
            boundary=(NO_BOUNDARY, NO_BOUNDARY),
        )

        self.assertTrue(is_valid_tiling(region, TILESET, (TILE_F0, None)))
        self.assertFalse(is_valid_tiling(region, TILESET, (TILE_F0,)))
        self.assertFalse(is_valid_tiling(region, TILESET, (None, None)))
        self.assertFalse(is_valid_tiling(region, TILESET, (True, None)))
        self.assertFalse(is_valid_tiling(region, TILESET, (len(TILESET), None)))
        self.assertFalse(is_valid_tiling(region, TILESET, (TILE_F0, TILE_F0)))

    def test_checks_horizontal_and_vertical_adjacency(self) -> None:
        horizontal = Region(
            width=2,
            height=1,
            active=(True, True),
            boundary=(NO_BOUNDARY, NO_BOUNDARY),
        )
        vertical = Region(
            width=1,
            height=2,
            active=(True, True),
            boundary=(NO_BOUNDARY, NO_BOUNDARY),
        )

        self.assertTrue(
            is_valid_tiling(horizontal, TILESET, (TILE_F0, TILE_F0))
        )
        self.assertFalse(
            is_valid_tiling(horizontal, TILESET, (TILE_F0, TILE_F1))
        )
        self.assertTrue(
            is_valid_tiling(vertical, TILESET, (TILE_F0, TILE_F1))
        )
        self.assertFalse(
            is_valid_tiling(vertical, TILESET, (TILE_F0, TILE_L0))
        )

    def test_does_not_match_across_inactive_cells(self) -> None:
        region = Region(
            width=3,
            height=1,
            active=(True, False, True),
            boundary=(NO_BOUNDARY, NO_BOUNDARY, NO_BOUNDARY),
        )

        self.assertTrue(
            is_valid_tiling(region, TILESET, (TILE_F0, None, TILE_F1))
        )


class TilingSolverTests(unittest.TestCase):
    def test_solves_a_single_forced_tile(self) -> None:
        region = Region(
            width=1,
            height=1,
            active=(True,),
            boundary=((COLOR_B, COLOR_0, COLOR_B, COLOR_0),),
        )

        result = solve_tiling(region, TILESET)

        self.assertEqual(result.status, TilingSolveStatus.SAT)
        self.assertEqual(result.tiling, (TILE_F0,))
        self.assertIsNotNone(result.tiling)
        self.assertTrue(is_valid_tiling(region, TILESET, result.tiling))

    def test_reports_unsat_for_an_impossible_boundary(self) -> None:
        region = Region(
            width=1,
            height=1,
            active=(True,),
            boundary=((COLOR_V, COLOR_NONE, COLOR_NONE, COLOR_NONE),),
        )

        result = solve_tiling(region, TILESET)

        self.assertEqual(result.status, TilingSolveStatus.UNSAT)
        self.assertIsNone(result.tiling)

    def test_solves_inactive_and_unconstrained_regions(self) -> None:
        inactive = Region(
            width=2,
            height=1,
            active=(False, False),
            boundary=(NO_BOUNDARY, NO_BOUNDARY),
        )
        unconstrained = Region(
            width=2,
            height=2,
            active=(True, True, True, True),
            boundary=(NO_BOUNDARY,) * 4,
        )

        for region in (inactive, unconstrained):
            with self.subTest(active=region.active):
                result = solve_tiling(region, TILESET)

                self.assertEqual(result.status, TilingSolveStatus.SAT)
                self.assertIsNotNone(result.tiling)
                self.assertTrue(
                    is_valid_tiling(region, TILESET, result.tiling)
                )

    def test_reports_unsat_for_forced_adjacency_mismatches(self) -> None:
        horizontal = Region(
            width=2,
            height=1,
            active=(True, True),
            boundary=(
                (COLOR_B, COLOR_NONE, COLOR_V0_A, COLOR_V),
                (COLOR_B, COLOR_0, COLOR_V0_A, COLOR_NONE),
            ),
        )
        vertical = Region(
            width=1,
            height=2,
            active=(True, True),
            boundary=(
                (COLOR_B, COLOR_0, COLOR_NONE, COLOR_V),
                (COLOR_NONE, COLOR_0, COLOR_V0_A, COLOR_V),
            ),
        )

        for region in (horizontal, vertical):
            with self.subTest(width=region.width, height=region.height):
                result = solve_tiling(region, TILESET)

                self.assertEqual(result.status, TilingSolveStatus.UNSAT)
                self.assertIsNone(result.tiling)

    def test_rejects_invalid_tileset_storage(self) -> None:
        region = Region(
            width=1,
            height=1,
            active=(True,),
            boundary=(NO_BOUNDARY,),
        )

        with self.assertRaises(TypeError):
            solve_tiling(region, [TILESET[0]])  # type: ignore[arg-type]
        with self.assertRaises(ValueError):
            solve_tiling(region, ())
        with self.assertRaises(ValueError):
            solve_tiling(region, ((COLOR_B, COLOR_B, COLOR_B),))
        with self.assertRaises(ValueError):
            solve_tiling(
                region,
                ((COLOR_B, COLOR_B, COLOR_B, COLOR_COUNT),),
            )


if __name__ == "__main__":
    unittest.main()
