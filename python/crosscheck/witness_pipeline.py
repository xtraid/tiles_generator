"""End-to-end Boolean/Wang witness cross-validation over one native lifetime."""

from collections.abc import Sequence

from model.formula import Formula
from model.region import Region
from model.tileset import TILESET
from native.formula_adapter import PathLike, _copy_formula, _loaded_formula
from native.region_adapter import _built_reduction, _copy_region
from native.witness_adapter import (
    _extract_assignment,
    _solve_assignment_extension,
    _solve_native,
)
from oracles.boolean_solver import (
    BooleanSolveResult,
    BooleanSolveStatus,
    solve_boolean,
)
from oracles.tiling_check import is_valid_tiling
from oracles.tiling_solver import TilingSolveResult, TilingSolveStatus
from oracles.witness_check import is_valid_assignment


class WitnessCrosscheckError(RuntimeError):
    """An independently checked assignment/tiling relation did not hold."""

    def __init__(
        self,
        message: str,
        *,
        assignment: tuple[bool, ...] | None,
        tiling: tuple[int | None, ...] | None,
        extracted: tuple[bool, ...] | None,
    ) -> None:
        self.assignment = assignment
        self.tiling = tiling
        self.extracted = extracted
        super().__init__(message)


def _raise_crosscheck_failure(
    message: str,
    assignment: tuple[bool, ...] | None,
    wang_result: TilingSolveResult | None,
    extracted: tuple[bool, ...] | None,
) -> None:
    raise WitnessCrosscheckError(
        message,
        assignment=assignment,
        tiling=None if wang_result is None else wang_result.tiling,
        extracted=extracted,
    )


def solve_boolean_native_extension(
    path: PathLike,
    optimized: bool = False,
) -> tuple[
    Formula,
    Region,
    BooleanSolveResult,
    TilingSolveResult | None,
]:
    """Extend the exact Boolean-Z3 witness with one selected native solver."""
    with _loaded_formula(path) as native_formula:
        formula = _copy_formula(native_formula)
        with _built_reduction(native_formula) as native_reduction:
            region = _copy_region(native_reduction.region)
            boolean_result = solve_boolean(formula)
            if boolean_result.status is not BooleanSolveStatus.SAT:
                return formula, region, boolean_result, None

            assignment = boolean_result.assignment
            extracted: tuple[bool, ...] | None = None
            if assignment is None:
                _raise_crosscheck_failure(
                    "Boolean SAT result did not carry an assignment",
                    None,
                    None,
                    None,
                )

            wang_result = _solve_assignment_extension(
                native_formula,
                native_reduction,
                region,
                assignment,
                optimized=optimized,
            )
            if wang_result.status is TilingSolveStatus.SAT:
                if wang_result.tiling is None:
                    _raise_crosscheck_failure(
                        "native SAT result did not carry a tiling",
                        assignment,
                        wang_result,
                        None,
                    )
                extracted = _extract_assignment(
                    native_formula,
                    native_reduction,
                    region,
                    wang_result.tiling,
                )

            if not is_valid_assignment(formula, assignment):
                _raise_crosscheck_failure(
                    "Boolean Z3 returned an assignment rejected by the "
                    "Python checker",
                    assignment,
                    wang_result,
                    extracted,
                )
            if wang_result.status is not TilingSolveStatus.SAT:
                _raise_crosscheck_failure(
                    "valid Boolean assignment did not have a native Wang "
                    "extension",
                    assignment,
                    wang_result,
                    extracted,
                )
            if wang_result.tiling is None or not is_valid_tiling(
                region,
                TILESET,
                wang_result.tiling,
            ):
                _raise_crosscheck_failure(
                    "native SAT tiling was rejected by the Python checker",
                    assignment,
                    wang_result,
                    extracted,
                )
            if extracted != assignment:
                _raise_crosscheck_failure(
                    "native Wang tiling did not encode the requested "
                    "assignment",
                    assignment,
                    wang_result,
                    extracted,
                )
            return formula, region, boolean_result, wang_result


def solve_native_and_extract(
    path: PathLike,
    optimized: bool = False,
) -> tuple[
    Formula,
    Region,
    TilingSolveResult,
    tuple[bool, ...] | None,
]:
    """Solve one reduction natively and decode its Boolean witness."""
    with _loaded_formula(path) as native_formula:
        formula = _copy_formula(native_formula)
        with _built_reduction(native_formula) as native_reduction:
            region = _copy_region(native_reduction.region)
            extracted: tuple[bool, ...] | None = None
            wang_result = _solve_native(
                native_reduction,
                region,
                optimized=optimized,
            )
            if wang_result.status is TilingSolveStatus.SAT:
                if wang_result.tiling is None:
                    _raise_crosscheck_failure(
                        "native SAT result did not carry a tiling",
                        None,
                        wang_result,
                        None,
                    )
                extracted = _extract_assignment(
                    native_formula,
                    native_reduction,
                    region,
                    wang_result.tiling,
                )

            if wang_result.status is TilingSolveStatus.UNSAT:
                return formula, region, wang_result, None
            if wang_result.status is not TilingSolveStatus.SAT:
                _raise_crosscheck_failure(
                    "native Wang solver returned an unsupported status",
                    extracted,
                    wang_result,
                    extracted,
                )
            if wang_result.tiling is None or not is_valid_tiling(
                region,
                TILESET,
                wang_result.tiling,
            ):
                _raise_crosscheck_failure(
                    "native SAT tiling was rejected by the Python checker",
                    extracted,
                    wang_result,
                    extracted,
                )
            if extracted is None:
                _raise_crosscheck_failure(
                    "native SAT tiling did not encode a Boolean assignment",
                    None,
                    wang_result,
                    None,
                )
            if not is_valid_assignment(formula, extracted):
                _raise_crosscheck_failure(
                    "native SAT tiling decoded to an invalid Boolean "
                    "assignment",
                    extracted,
                    wang_result,
                    extracted,
                )
            return formula, region, wang_result, extracted


def extract_wang_assignment(
    path: PathLike,
    tiling: Sequence[int | None],
) -> tuple[bool, ...] | None:
    """Decode a normalized tiling without evaluating the resulting assignment."""
    with _loaded_formula(path) as native_formula:
        with _built_reduction(native_formula) as native_reduction:
            region = _copy_region(native_reduction.region)
            return _extract_assignment(
                native_formula,
                native_reduction,
                region,
                tiling,
            )
