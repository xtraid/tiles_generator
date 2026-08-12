from dataclasses import FrozenInstanceError
import unittest

from model.formula import Formula
from oracles.boolean_solver import (
    BooleanSolveResult,
    BooleanSolveStatus,
    solve_boolean,
)
from oracles.witness_check import is_valid_assignment


class FormulaModelTests(unittest.TestCase):
    def test_formula_is_immutable_and_preserves_clause_positions(self) -> None:
        formula = Formula(variable_count=2, clauses=((0, 0, 1), (0, 1, 1)))

        self.assertEqual(formula.clauses[0], (0, 0, 1))
        with self.assertRaises(FrozenInstanceError):
            formula.variable_count = 3  # type: ignore[misc]

    def test_rejects_non_canonical_or_mutable_storage(self) -> None:
        with self.assertRaises(TypeError):
            Formula(variable_count=1, clauses=[(0, 0, 0)])  # type: ignore[arg-type]
        with self.assertRaises(ValueError):
            Formula(variable_count=1, clauses=((0, 0),))  # type: ignore[arg-type]
        with self.assertRaises(ValueError):
            Formula(variable_count=2, clauses=((0, 0, 1), (0, 0, 1)))


class BooleanWitnessTests(unittest.TestCase):
    def test_accepts_valid_assignment(self) -> None:
        formula = Formula(
            variable_count=3,
            clauses=((0, 0, 1), (0, 1, 2), (1, 2, 2)),
        )

        self.assertTrue(is_valid_assignment(formula, (False, True, False)))

    def test_counts_repeated_clause_positions(self) -> None:
        formula = Formula(variable_count=2, clauses=((0, 0, 1), (0, 1, 1)))

        # Deduplicating either clause would incorrectly accept this witness.
        self.assertFalse(is_valid_assignment(formula, (True, False)))

    def test_rejects_wrong_length_and_non_boolean_values(self) -> None:
        formula = Formula(variable_count=1, clauses=((0, 0, 0),))

        self.assertFalse(is_valid_assignment(formula, ()))
        self.assertFalse(is_valid_assignment(formula, (1,)))


class BooleanSolverScaffoldTests(unittest.TestCase):
    def test_result_enforces_assignment_presence(self) -> None:
        assignment = (True, False)
        self.assertEqual(
            BooleanSolveResult(BooleanSolveStatus.SAT, assignment).assignment,
            assignment,
        )
        self.assertIsNone(BooleanSolveResult(BooleanSolveStatus.UNSAT).assignment)
        self.assertIsNone(BooleanSolveResult(BooleanSolveStatus.UNKNOWN).assignment)

        with self.assertRaises(ValueError):
            BooleanSolveResult(BooleanSolveStatus.SAT)
        with self.assertRaises(ValueError):
            BooleanSolveResult(BooleanSolveStatus.UNSAT, assignment)

    def test_solver_is_explicitly_not_implemented(self) -> None:
        formula = Formula(variable_count=1, clauses=((0, 0, 0),))

        with self.assertRaises(NotImplementedError):
            solve_boolean(formula)


if __name__ == "__main__":
    unittest.main()
