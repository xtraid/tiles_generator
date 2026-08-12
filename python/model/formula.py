"""Pure Python data contract for Cubic Monotone 1-in-3 SAT formulas."""

from dataclasses import dataclass
from typing import TypeAlias


Clause: TypeAlias = tuple[int, int, int]


@dataclass(frozen=True, slots=True)
class Formula:
    """An immutable formula with three preserved positions per clause."""

    variable_count: int
    clauses: tuple[Clause, ...]

    def __post_init__(self) -> None:
        if type(self.variable_count) is not int or self.variable_count <= 0:
            raise ValueError("variable_count must be a positive integer")
        if type(self.clauses) is not tuple:
            raise TypeError("clauses must be a tuple")
        if len(self.clauses) != self.variable_count:
            raise ValueError("a cubic CM1-in-3 formula has one clause per variable")

        occurrences = [0] * self.variable_count
        for clause in self.clauses:
            if type(clause) is not tuple or len(clause) != 3:
                raise ValueError("every clause must preserve exactly three positions")
            for variable in clause:
                if (type(variable) is not int or variable < 0 or
                        variable >= self.variable_count):
                    raise ValueError("clause variable is outside the canonical domain")
                occurrences[variable] += 1

        if any(count != 3 for count in occurrences):
            raise ValueError("every variable must occur exactly three times")
