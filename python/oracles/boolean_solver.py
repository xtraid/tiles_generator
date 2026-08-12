"""Scaffold for the Boolean Z3 oracle.

The future solver consumes an already constructed :class:`Formula`.  Parsing,
filesystem access, ctypes, the Wang region, and Yang-Zhang reduction logic do
not belong here.
"""

from dataclasses import dataclass
from enum import Enum

from model.formula import Formula


class BooleanSolveStatus(Enum):
    SAT = "sat"
    UNSAT = "unsat"
    UNKNOWN = "unknown"


@dataclass(frozen=True, slots=True)
class BooleanSolveResult:
    status: BooleanSolveStatus
    assignment: tuple[bool, ...] | None = None

    def __post_init__(self) -> None:
        has_assignment = self.assignment is not None
        if has_assignment != (self.status is BooleanSolveStatus.SAT):
            raise ValueError("only SAT results carry an assignment")


def solve_boolean(formula: Formula) -> BooleanSolveResult:
    """Solve ``formula`` once the Z3 encoding milestone is implemented."""

    del formula
    raise NotImplementedError("the Boolean Z3 oracle is scaffolded, not implemented")
