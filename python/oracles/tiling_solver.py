"""Independent Wang-tiling Z3 oracle over the pure Python Region model."""

from dataclasses import dataclass
from enum import Enum

from z3 import ArithRef, Implies, Int, Or, Solver, sat, unsat

from model.region import Region
from model.tileset import (
    COLOR_COUNT,
    COLOR_NONE,
    DIR_COUNT,
    E,
    S,
    Tileset,
)


class TilingSolveStatus(Enum):
    SAT = "sat"
    UNSAT = "unsat"
    UNKNOWN = "unknown"


@dataclass(frozen=True, slots=True)
class TilingSolveResult:
    status: TilingSolveStatus
    tiling: tuple[int | None, ...] | None = None

    def __post_init__(self) -> None:
        has_tiling = self.tiling is not None
        if has_tiling != (self.status is TilingSolveStatus.SAT):
            raise ValueError("only SAT results carry a tiling")


def _validate_tileset(tileset: Tileset) -> None:
    if type(tileset) is not tuple:
        raise TypeError("tileset must be a tuple")
    if not tileset:
        raise ValueError("tileset must contain at least one tile")

    for tile in tileset:
        if type(tile) is not tuple or len(tile) != DIR_COUNT:
            raise ValueError("each tile must contain four immutable edges")
        if any(
            type(color) is not int or not 0 <= color < COLOR_COUNT
            for color in tile
        ):
            raise ValueError("tile contains an invalid color")


def solve_tiling(region: Region, tileset: Tileset) -> TilingSolveResult:
    """Solve an existing region without parsing or rebuilding its reduction.

    The immutable tileset is validated before use. SAT returns one dense
    row-major tile ID per active cell and ``None`` for inactive cells; UNSAT
    and UNKNOWN return no tiling.
    """
    _validate_tileset(tileset)

    solver = Solver()
    variables: list[ArithRef | None] = [
        Int(f"tile_{index}") if active else None
        for index, active in enumerate(region.active)
    ]
    compatibility = {
        direction: tuple(
            tuple(
                adjacent_id
                for adjacent_id, adjacent in enumerate(tileset)
                if tile[direction]
                == adjacent[(direction + 2) % DIR_COUNT]
            )
            for tile in tileset
        )
        for direction in (E, S)
    }

    for index, variable in enumerate(variables):
        if variable is None:
            continue

        solver.add(
            Or(
                *(variable == tile_id for tile_id in range(len(tileset)))
            )
        )
        for direction, required_color in enumerate(region.boundary[index]):
            if required_color != COLOR_NONE:
                solver.add(
                    Or(
                        *(
                            variable == tile_id
                            for tile_id, tile in enumerate(tileset)
                            if tile[direction] == required_color
                        )
                    )
                )

    for y in range(region.height):
        for x in range(region.width):
            index = y * region.width + x
            variable = variables[index]
            if variable is None:
                continue

            neighbors = []
            if x + 1 < region.width:
                neighbors.append((E, index + 1))
            if y + 1 < region.height:
                neighbors.append((S, index + region.width))

            for direction, neighbor_index in neighbors:
                neighbor = variables[neighbor_index]
                if neighbor is None:
                    continue
                for tile_id, adjacent_ids in enumerate(
                    compatibility[direction]
                ):
                    solver.add(
                        Implies(
                            variable == tile_id,
                            Or(
                                *(
                                    neighbor == adjacent_id
                                    for adjacent_id in adjacent_ids
                                )
                            ),
                        )
                    )

    status = solver.check()
    if status == sat:
        model = solver.model()
        tiling = tuple(
            None
            if variable is None
            else model.eval(variable, model_completion=True).as_long()
            for variable in variables
        )
        return TilingSolveResult(TilingSolveStatus.SAT, tiling)

    if status == unsat:
        return TilingSolveResult(TilingSolveStatus.UNSAT)

    return TilingSolveResult(TilingSolveStatus.UNKNOWN)
