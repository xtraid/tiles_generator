"""Independent pure Python checker for Boolean 1-in-3 witnesses."""

from collections.abc import Sequence

from model.formula import Formula


def is_valid_assignment(formula: Formula, assignment: Sequence[bool]) -> bool:
    """Return whether ``assignment`` makes exactly one position true per clause."""

    if len(assignment) != formula.variable_count:
        return False
    if any(type(value) is not bool for value in assignment):
        return False

    return all(
        sum(assignment[variable] for variable in clause) == 1
        for clause in formula.clauses
    )
