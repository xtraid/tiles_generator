from pathlib import Path
import unittest

from native.formula_adapter import load_formula
from oracles.boolean_solver import BooleanSolveStatus, solve_boolean
from oracles.witness_check import is_valid_assignment


INSTANCE_DIRECTORY = Path(__file__).resolve().parents[1] / "instances"


class CurrentPipelineTests(unittest.TestCase):
    def test_sat_file_through_native_parser_z3_and_witness_checker(self) -> None:
        formula = load_formula(INSTANCE_DIRECTORY / "pipeline_sat.cm13")

        result = solve_boolean(formula)

        self.assertEqual(result.status, BooleanSolveStatus.SAT)
        self.assertIsNotNone(result.assignment)
        self.assertTrue(is_valid_assignment(formula, result.assignment))

    def test_unsat_file_through_native_parser_and_z3(self) -> None:
        formula = load_formula(INSTANCE_DIRECTORY / "pipeline_unsat.cm13")

        result = solve_boolean(formula)

        self.assertEqual(result.status, BooleanSolveStatus.UNSAT)
        self.assertIsNone(result.assignment)


if __name__ == "__main__":
    unittest.main()
