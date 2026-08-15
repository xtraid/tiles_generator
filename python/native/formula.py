"""Copy Cubic Monotone 1-in-3 SAT formulas from the native parser."""

from ctypes import (
    CDLL,
    POINTER,
    Structure,
    byref,
    c_char_p,
    c_int,
    c_size_t,
    c_uint32,
)
from enum import IntEnum
from functools import cache
import os
from pathlib import Path
from typing import TypeAlias

from model.formula import Formula


PathLike: TypeAlias = str | bytes | os.PathLike[str] | os.PathLike[bytes]
_LIBRARY_PATH = Path(__file__).resolve().parents[2] / "build" / "lib" / "libwang.so"


class FormulaParseStatus(IntEnum):
    """Stable status values exported by ``Cm13ParseStatus``."""

    OK = 0
    INVALID_ARGUMENT = 1
    IO_ERROR = 2
    SYNTAX_ERROR = 3
    DOMAIN_ERROR = 4
    OUT_OF_MEMORY = 5


_STATUS_MESSAGES = {
    FormulaParseStatus.INVALID_ARGUMENT: "invalid native parser argument",
    FormulaParseStatus.IO_ERROR: "I/O error",
    FormulaParseStatus.SYNTAX_ERROR: "syntax error",
    FormulaParseStatus.DOMAIN_ERROR: "formula domain error",
    FormulaParseStatus.OUT_OF_MEMORY: "native parser ran out of memory",
}


class FormulaLoadError(RuntimeError):
    """A native parser failure, including its optional source location."""

    def __init__(
        self,
        path: str | bytes,
        status: FormulaParseStatus,
        line: int,
        column: int,
    ) -> None:
        self.path = path
        self.status = status
        self.line = line
        self.column = column

        detail = _STATUS_MESSAGES[status]
        if line != 0 or column != 0:
            detail += f" at line {line}, column {column}"
        super().__init__(f"could not load CM13 formula {path!r}: {detail}")


class _Cm13Clause(Structure):
    _fields_ = [("variable_index", c_uint32 * 3)]


class _Cm13Formula(Structure):
    _fields_ = [
        ("variable_count", c_uint32),
        ("clauses", POINTER(_Cm13Clause)),
        ("clause_count", c_size_t),
    ]


class _Cm13ParseLocation(Structure):
    _fields_ = [("line", c_size_t), ("column", c_size_t)]


@cache
def _library() -> CDLL:
    library = CDLL(str(_LIBRARY_PATH))
    library.cm13_formula_load_path.argtypes = [
        c_char_p,
        POINTER(_Cm13Formula),
        POINTER(_Cm13ParseLocation),
    ]
    library.cm13_formula_load_path.restype = c_int
    library.cm13_formula_destroy.argtypes = [POINTER(_Cm13Formula)]
    library.cm13_formula_destroy.restype = None
    return library


def _copy_formula(native_formula: _Cm13Formula) -> Formula:
    clauses = tuple(
        tuple(
            int(native_formula.clauses[clause].variable_index[position])
            for position in range(3)
        )
        for clause in range(native_formula.clause_count)
    )
    return Formula(variable_count=int(native_formula.variable_count), clauses=clauses)


def load_formula(path: PathLike) -> Formula:
    """Parse ``path`` in C and return a fully Python-owned formula.

    Native allocations are released before this function returns or raises.
    Parser failures raise :class:`FormulaLoadError`; failures to load the shared
    library itself retain the standard :class:`OSError` from ``ctypes``.
    """

    filesystem_path = os.fspath(path)
    encoded_path = os.fsencode(filesystem_path)
    native_formula = _Cm13Formula()
    location = _Cm13ParseLocation()
    library = _library()

    try:
        status_code = library.cm13_formula_load_path(
            encoded_path,
            byref(native_formula),
            byref(location),
        )
        try:
            status = FormulaParseStatus(status_code)
        except ValueError as error:
            raise RuntimeError(
                f"native parser returned unknown status {status_code}"
            ) from error
        if status is not FormulaParseStatus.OK:
            raise FormulaLoadError(
                filesystem_path,
                status,
                int(location.line),
                int(location.column),
            )
        return _copy_formula(native_formula)
    finally:
        library.cm13_formula_destroy(byref(native_formula))
