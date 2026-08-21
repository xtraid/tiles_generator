from contextlib import contextmanager
from ctypes import POINTER, c_uint32, cast
from pathlib import Path
import unittest
from unittest.mock import patch

from model.region import Region
from model.tileset import COLOR_NONE, TILESET
from native.formula_adapter import _Cm13Formula
from native.region_adapter import _YangZhangReduction
from native.witness_adapter import (
    NativeWitnessError,
    _solve_assignment_extension,
)
from oracles.boolean_solver import (
    BooleanSolveResult,
    BooleanSolveStatus,
)
from oracles.tiling_check import is_valid_tiling
from oracles.tiling_solver import TilingSolveResult, TilingSolveStatus, solve_tiling
from oracles.witness_check import is_valid_assignment
from crosscheck.witness_pipeline import (
    WitnessCrosscheckError,
    extract_wang_assignment,
    solve_boolean_native_extension,
    solve_native_and_extract,
)
from native.reduction_adapter import load_formula_and_region


INSTANCE_DIRECTORY = Path(__file__).resolve().parents[1] / "instances"
SAT_PATH = INSTANCE_DIRECTORY / "pipeline_sat.cm13"
UNSAT_PATH = INSTANCE_DIRECTORY / "pipeline_unsat.cm13"


class WitnessPipelineIntegrationTests(unittest.TestCase):
    def assert_forward_extension(self, *, optimized: bool) -> None:
        formula, region, boolean_result, wang_result = (
            solve_boolean_native_extension(SAT_PATH, optimized=optimized)
        )

        self.assertEqual(boolean_result.status, BooleanSolveStatus.SAT)
        self.assertIsNotNone(boolean_result.assignment)
        self.assertTrue(is_valid_assignment(formula, boolean_result.assignment))
        self.assertIsNotNone(wang_result)
        self.assertEqual(wang_result.status, TilingSolveStatus.SAT)
        self.assertIsNotNone(wang_result.tiling)
        self.assertTrue(is_valid_tiling(region, TILESET, wang_result.tiling))

        extracted = extract_wang_assignment(SAT_PATH, wang_result.tiling)
        self.assertEqual(extracted, boolean_result.assignment)

    def test_boolean_z3_assignment_extends_through_native_reference(self) -> None:
        self.assert_forward_extension(optimized=False)

    def test_boolean_z3_assignment_extends_through_native_optimized(self) -> None:
        self.assert_forward_extension(optimized=True)

    def test_unpinned_native_tilings_extract_valid_assignments(self) -> None:
        for optimized in (False, True):
            with self.subTest(optimized=optimized):
                formula, region, wang_result, assignment = (
                    solve_native_and_extract(SAT_PATH, optimized=optimized)
                )
                self.assertEqual(wang_result.status, TilingSolveStatus.SAT)
                self.assertIsNotNone(wang_result.tiling)
                self.assertTrue(
                    is_valid_tiling(region, TILESET, wang_result.tiling)
                )
                self.assertIsNotNone(assignment)
                self.assertTrue(is_valid_assignment(formula, assignment))

    def test_unpinned_native_unsat_preserves_status_without_witness(self) -> None:
        for optimized in (False, True):
            with self.subTest(optimized=optimized):
                _, _, wang_result, assignment = solve_native_and_extract(
                    UNSAT_PATH,
                    optimized=optimized,
                )

                self.assertEqual(wang_result.status, TilingSolveStatus.UNSAT)
                self.assertIsNone(wang_result.tiling)
                self.assertIsNone(assignment)

    def test_wang_z3_tiling_extracts_a_valid_assignment(self) -> None:
        formula, region = load_formula_and_region(SAT_PATH)
        wang_result = solve_tiling(region, TILESET)
        self.assertEqual(wang_result.status, TilingSolveStatus.SAT)
        self.assertIsNotNone(wang_result.tiling)

        assignment = extract_wang_assignment(SAT_PATH, wang_result.tiling)

        self.assertIsNotNone(assignment)
        self.assertTrue(is_valid_assignment(formula, assignment))

    def test_repeated_clause_positions_survive_the_forward_path(self) -> None:
        formula, _, boolean_result, wang_result = (
            solve_boolean_native_extension(SAT_PATH)
        )

        self.assertTrue(any(len(set(clause)) < 3 for clause in formula.clauses))
        self.assertTrue(is_valid_assignment(formula, boolean_result.assignment))
        self.assertIsNotNone(wang_result)
        self.assertEqual(wang_result.status, TilingSolveStatus.SAT)

    def test_boolean_unsat_does_not_produce_or_request_a_wang_witness(self) -> None:
        with patch(
            "crosscheck.witness_pipeline._solve_assignment_extension",
            side_effect=AssertionError("extension must not be called"),
        ):
            _, _, boolean_result, wang_result = solve_boolean_native_extension(
                UNSAT_PATH
            )

        self.assertEqual(boolean_result.status, BooleanSolveStatus.UNSAT)
        self.assertIsNone(wang_result)

    def test_boolean_unknown_does_not_call_native_extension(self) -> None:
        unknown = BooleanSolveResult(BooleanSolveStatus.UNKNOWN)
        with patch(
            "crosscheck.witness_pipeline.solve_boolean",
            return_value=unknown,
        ), patch(
            "crosscheck.witness_pipeline._solve_assignment_extension",
            side_effect=AssertionError("extension must not be called"),
        ):
            _, _, boolean_result, wang_result = solve_boolean_native_extension(
                SAT_PATH
            )

        self.assertIs(boolean_result, unknown)
        self.assertIsNone(wang_result)

    def test_invalid_but_normalized_tiling_is_a_negative_witness(self) -> None:
        _, region = load_formula_and_region(SAT_PATH)
        invalid = tuple(0 if active else None for active in region.active)

        self.assertIsNone(extract_wang_assignment(SAT_PATH, invalid))

    def test_malformed_python_tilings_are_rejected_before_native_call(self) -> None:
        _, region = load_formula_and_region(SAT_PATH)
        valid_shape = [0 if active else None for active in region.active]
        active_index = region.active.index(True)
        inactive_index = region.active.index(False)

        malformed = (
            valid_shape[:-1],
            [None if i == active_index else value
             for i, value in enumerate(valid_shape)],
            [True if i == active_index else value
             for i, value in enumerate(valid_shape)],
            [23 if i == active_index else value
             for i, value in enumerate(valid_shape)],
            [0 if i == inactive_index else value
             for i, value in enumerate(valid_shape)],
        )
        for tiling in malformed:
            with self.subTest(kind=tiling[active_index] if tiling else None):
                with self.assertRaises(ValueError):
                    extract_wang_assignment(SAT_PATH, tiling)


class WitnessAdapterFailureTests(unittest.TestCase):
    def setUp(self) -> None:
        self.region = Region(
            width=1,
            height=1,
            active=(True,),
            boundary=((COLOR_NONE, COLOR_NONE, COLOR_NONE, COLOR_NONE),),
        )
        self.formula = _Cm13Formula(
            variable_count=1,
            clauses=None,
            clause_count=1,
        )
        self.reduction = _YangZhangReduction()

    def test_native_error_and_unknown_status_raise_and_destroy_result(self) -> None:
        for status in (-1, 99):
            events: list[str] = []

            class RecordingLibrary:
                @staticmethod
                def yang_zhang_solve_assignment_extension(*args):
                    return status

                @staticmethod
                def wang_solve_result_destroy(result):
                    events.append("result")

            with self.subTest(status=status), patch(
                "native.witness_adapter._witness_library",
                return_value=RecordingLibrary(),
            ):
                with self.assertRaises(NativeWitnessError):
                    _solve_assignment_extension(
                        self.formula,
                        self.reduction,
                        self.region,
                        (True,),
                        optimized=False,
                    )

            self.assertEqual(events, ["result"])

    def test_malformed_sat_copy_out_still_destroys_result(self) -> None:
        events: list[str] = []

        class RecordingLibrary:
            domains = (c_uint32 * 1)(0)

            def yang_zhang_solve_assignment_extension(
                self,
                formula,
                reduction,
                assignment,
                assignment_count,
                solver,
                out_result,
            ):
                result = out_result._obj
                result.domains = cast(self.domains, POINTER(c_uint32))
                result.domain_count = 1
                return 1

            @staticmethod
            def wang_solve_result_destroy(result):
                events.append("result")

        with patch(
            "native.witness_adapter._witness_library",
            return_value=RecordingLibrary(),
        ):
            with self.assertRaises(NativeWitnessError):
                _solve_assignment_extension(
                    self.formula,
                    self.reduction,
                    self.region,
                    (True,),
                    optimized=False,
                )

        self.assertEqual(events, ["result"])


class WitnessCoordinatorFailureTests(unittest.TestCase):
    def test_native_error_cleans_up_result_reduction_then_formula(self) -> None:
        events: list[str] = []
        formula = _Cm13Formula(
            variable_count=3,
            clauses=None,
            clause_count=3,
        )
        reduction = _YangZhangReduction()
        python_formula, python_region = load_formula_and_region(SAT_PATH)
        boolean_result = BooleanSolveResult(
            BooleanSolveStatus.SAT,
            (False, True, False),
        )

        @contextmanager
        def loaded_formula(path):
            try:
                yield formula
            finally:
                events.append("formula")

        @contextmanager
        def built_reduction(native_formula):
            try:
                yield reduction
            finally:
                events.append("reduction")

        class ErrorLibrary:
            @staticmethod
            def yang_zhang_solve_assignment_extension(*args):
                return -1

            @staticmethod
            def wang_solve_result_destroy(result):
                events.append("result")

        with patch(
            "crosscheck.witness_pipeline._loaded_formula",
            loaded_formula,
        ), patch(
            "crosscheck.witness_pipeline._built_reduction",
            built_reduction,
        ), patch(
            "crosscheck.witness_pipeline._copy_formula",
            return_value=python_formula,
        ), patch(
            "crosscheck.witness_pipeline._copy_region",
            return_value=python_region,
        ), patch(
            "crosscheck.witness_pipeline.solve_boolean",
            return_value=boolean_result,
        ), patch(
            "native.witness_adapter._witness_library",
            return_value=ErrorLibrary(),
        ):
            with self.assertRaises(NativeWitnessError):
                solve_boolean_native_extension(SAT_PATH)

        self.assertEqual(events, ["result", "reduction", "formula"])

    def test_extraction_failure_releases_reduction_then_formula(self) -> None:
        events: list[str] = []
        formula = _Cm13Formula(
            variable_count=3,
            clauses=None,
            clause_count=3,
        )
        reduction = _YangZhangReduction()
        python_formula, python_region = load_formula_and_region(SAT_PATH)
        assignment = (False, True, False)
        boolean_result = BooleanSolveResult(BooleanSolveStatus.SAT, assignment)
        tiling = tuple(0 if active else None for active in python_region.active)
        wang_result = TilingSolveResult(TilingSolveStatus.SAT, tiling)

        @contextmanager
        def loaded_formula(path):
            try:
                yield formula
            finally:
                events.append("formula")

        @contextmanager
        def built_reduction(native_formula):
            try:
                yield reduction
            finally:
                events.append("reduction")

        with patch(
            "crosscheck.witness_pipeline._loaded_formula",
            loaded_formula,
        ), patch(
            "crosscheck.witness_pipeline._built_reduction",
            built_reduction,
        ), patch(
            "crosscheck.witness_pipeline._copy_formula",
            return_value=python_formula,
        ), patch(
            "crosscheck.witness_pipeline._copy_region",
            return_value=python_region,
        ), patch(
            "crosscheck.witness_pipeline.solve_boolean",
            return_value=boolean_result,
        ), patch(
            "crosscheck.witness_pipeline._solve_assignment_extension",
            return_value=wang_result,
        ), patch(
            "crosscheck.witness_pipeline._extract_assignment",
            side_effect=RuntimeError("copy failed"),
        ):
            with self.assertRaisesRegex(RuntimeError, "copy failed"):
                solve_boolean_native_extension(SAT_PATH)

        self.assertEqual(events, ["reduction", "formula"])

    def test_independent_mismatch_retains_copied_witnesses(self) -> None:
        formula, region, boolean_result, wang_result = (
            solve_boolean_native_extension(SAT_PATH)
        )
        self.assertIsNotNone(wang_result)
        different = tuple(not value for value in boolean_result.assignment)

        with patch(
            "crosscheck.witness_pipeline._extract_assignment",
            return_value=different,
        ):
            with self.assertRaises(WitnessCrosscheckError) as raised:
                solve_boolean_native_extension(SAT_PATH)

        self.assertIsNotNone(raised.exception.assignment)
        self.assertIsNotNone(raised.exception.tiling)
        self.assertEqual(raised.exception.extracted, different)
        self.assertTrue(is_valid_assignment(formula, boolean_result.assignment))
        self.assertTrue(is_valid_tiling(region, TILESET, wang_result.tiling))


if __name__ == "__main__":
    unittest.main()
