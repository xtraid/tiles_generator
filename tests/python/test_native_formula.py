import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from native.formula_adapter import (
    FormulaLoadError,
    FormulaParseStatus,
    load_formula,
)


VALID_FORMULA = (
    "p cm13 3 3\n"
    "1 1 3 0\n"
    "2 2 3 0\n"
    "1 2 3 0\n"
)


class NativeFormulaTests(unittest.TestCase):
    def test_importing_native_adapters_does_not_load_shared_library(self) -> None:
        repository = Path(__file__).resolve().parents[2]
        environment = dict(os.environ)
        environment["PYTHONPATH"] = str(repository / "python")
        program = (
            "from unittest.mock import patch\n"
            "with patch('native._lib.CDLL', "
            "side_effect=AssertionError('library loaded')):\n"
            "    import native.reduction_adapter\n"
        )

        completed = subprocess.run(
            [sys.executable, "-c", program],
            cwd=repository,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_loads_and_copies_formula_from_path(self) -> None:
        with TemporaryDirectory() as directory:
            path = Path(directory) / "formula.cm13"
            path.write_text(VALID_FORMULA, encoding="ascii")

            formula = load_formula(path)

        self.assertEqual(formula.variable_count, 3)
        self.assertEqual(
            formula.clauses,
            ((0, 0, 2), (1, 1, 2), (0, 1, 2)),
        )

    def test_reports_parser_status_and_exact_location(self) -> None:
        with TemporaryDirectory() as directory:
            path = Path(directory) / "invalid.cm13"
            path.write_text("p WRONG 1 1\n", encoding="ascii")

            with self.assertRaises(FormulaLoadError) as raised:
                load_formula(path)

        self.assertEqual(raised.exception.status, FormulaParseStatus.SYNTAX_ERROR)
        self.assertEqual((raised.exception.line, raised.exception.column), (1, 3))
        self.assertIn("line 1, column 3", str(raised.exception))

    def test_reports_missing_path_as_io_error_without_location(self) -> None:
        with TemporaryDirectory() as directory:
            path = Path(directory) / "missing.cm13"

            with self.assertRaises(FormulaLoadError) as raised:
                load_formula(path)

        self.assertEqual(raised.exception.status, FormulaParseStatus.IO_ERROR)
        self.assertEqual((raised.exception.line, raised.exception.column), (0, 0))

    def test_releases_native_formula_when_python_copy_fails(self) -> None:
        class RecordingLibrary:
            destroyed = False

            @staticmethod
            def cm13_formula_load_path(path, formula, location):
                return FormulaParseStatus.OK

            def cm13_formula_destroy(self, formula):
                self.destroyed = True

        library = RecordingLibrary()
        with patch(
            "native.formula_adapter._formula_library",
            return_value=library,
        ), patch(
            "native.formula_adapter._copy_formula",
            side_effect=RuntimeError("copy failed"),
        ):
            with self.assertRaisesRegex(RuntimeError, "copy failed"):
                load_formula("ignored.cm13")

        self.assertTrue(library.destroyed)


if __name__ == "__main__":
    unittest.main()
