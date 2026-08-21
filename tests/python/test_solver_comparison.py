from pathlib import Path
import sys
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "benchmarks/python"))

from compare_solvers import (  # noqa: E402
    CASES,
    ENGINES,
    SCOPES,
    _parse_key_value_line,
    _selected_combinations,
    _summary_records,
)
from native.formula_adapter import load_formula  # noqa: E402
from oracles.boolean_solver import (  # noqa: E402
    BooleanSolveStatus,
    solve_boolean,
)


class SolverComparisonProtocolTests(unittest.TestCase):
    def test_versioned_corpus_has_expected_formula_shape(self) -> None:
        for spec in CASES.values():
            with self.subTest(case=spec.name):
                formula = load_formula(spec.path)
                self.assertEqual(formula.variable_count, spec.variable_count)
                self.assertEqual(len(formula.clauses), spec.variable_count)
                result = solve_boolean(formula)
                self.assertEqual(
                    result.status,
                    BooleanSolveStatus[spec.expected],
                )
                self.assertIn(
                    result.status,
                    (BooleanSolveStatus.SAT, BooleanSolveStatus.UNSAT),
                )

    def test_boolean_solver_is_only_in_file_scope(self) -> None:
        combinations = _selected_combinations(
            [CASES["pipeline_unsat"]],
            list(ENGINES),
            list(SCOPES),
        )

        selected = {(engine, scope) for _, engine, scope in combinations}
        self.assertNotIn(
            ("python-boolean-z3", "wang-solve-verified"),
            selected,
        )
        self.assertIn(
            ("python-boolean-z3", "file-to-verified-decision"),
            selected,
        )
        self.assertEqual(len(selected), 7)

    def test_native_record_parser_ignores_non_fields(self) -> None:
        self.assertEqual(
            _parse_key_value_line(
                "benchmark result case=pipeline_unsat expected=UNSAT"
            ),
            {"case": "pipeline_unsat", "expected": "UNSAT"},
        )

    def test_timeout_is_censored_from_timing_summary(self) -> None:
        common = {
            "case": "pipeline_unsat",
            "engine": "python-wang-z3",
            "problem": "wang-region",
            "scope": "wang-solve-verified",
            "expected": "UNSAT",
        }
        samples = [
            {
                **common,
                "status": "UNSAT",
                "ns_per_iteration": 100,
                "process_peak_rss_kib": 50,
            },
            {
                **common,
                "status": "TIMEOUT",
                "ns_per_iteration": None,
                "process_peak_rss_kib": None,
            },
        ]

        summary = _summary_records(samples)[0]

        self.assertEqual(summary["completed_samples"], 1)
        self.assertEqual(summary["timeouts"], 1)
        self.assertEqual(summary["median_ns_per_iteration"], 100)
        self.assertEqual(summary["median_process_peak_rss_kib"], 50)


if __name__ == "__main__":
    unittest.main()
