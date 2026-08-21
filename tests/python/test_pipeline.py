from pathlib import Path
import unittest

from model.tileset import TILESET
from native.reduction_adapter import load_formula_and_region
from oracles.boolean_solver import BooleanSolveStatus, solve_boolean
from oracles.tiling_check import is_valid_tiling
from oracles.tiling_solver import TilingSolveStatus, solve_tiling
from oracles.witness_check import is_valid_assignment


INSTANCE_DIRECTORY = Path(__file__).resolve().parents[1] / "instances"


class CurrentPipelineTests(unittest.TestCase):
    def test_sat_file_through_both_z3_paths_and_witness_checkers(self) -> None:
        formula, region = load_formula_and_region(
            INSTANCE_DIRECTORY / "pipeline_sat.cm13"
        )

        boolean_result = solve_boolean(formula)
        tiling_result = solve_tiling(region, TILESET)

        self.assertEqual(boolean_result.status, BooleanSolveStatus.SAT)
        self.assertIsNotNone(boolean_result.assignment)
        self.assertTrue(
            is_valid_assignment(formula, boolean_result.assignment)
        )
        self.assertEqual(tiling_result.status, TilingSolveStatus.SAT)
        self.assertIsNotNone(tiling_result.tiling)
        self.assertTrue(
            is_valid_tiling(region, TILESET, tiling_result.tiling)
        )

    def test_unsat_file_through_both_z3_paths(self) -> None:
        formula, region = load_formula_and_region(
            INSTANCE_DIRECTORY / "pipeline_unsat.cm13"
        )

        boolean_result = solve_boolean(formula)
        tiling_result = solve_tiling(region, TILESET)

        self.assertEqual(boolean_result.status, BooleanSolveStatus.UNSAT)
        self.assertIsNone(boolean_result.assignment)
        self.assertEqual(tiling_result.status, TilingSolveStatus.UNSAT)
        self.assertIsNone(tiling_result.tiling)


if __name__ == "__main__":
    unittest.main()
