"""Immutable Python data contract for finite Wang regions."""

from dataclasses import dataclass


COLOR_COUNT = 16
COLOR_NONE = 255
_DIRECTION_OFFSETS = ((0, -1), (1, 0), (0, 1), (-1, 0))


@dataclass(frozen=True, slots=True)
class Region:
    """A validated dense row-major region with ``(N, E, S, W)`` sides."""

    width: int
    height: int
    active: tuple[bool, ...]  # active[i] says if cell exists
    boundary: tuple[tuple[int, int, int, int], ...]  # (N, E, S, W)

    def __post_init__(self) -> None:
        if type(self.width) is not int or self.width <= 0:
            raise ValueError("width must be a positive integer")
        if type(self.height) is not int or self.height <= 0:
            raise ValueError("height must be a positive integer")
        if type(self.active) is not tuple:
            raise TypeError("active storage must be a tuple")
        if type(self.boundary) is not tuple:
            raise TypeError("boundary storage must be a tuple")
        cell_count = self.width * self.height
        if len(self.active) != cell_count:
            raise ValueError("active storage must match the region area")
        if len(self.boundary) != cell_count:
            raise ValueError("boundary storage must match the region area")
        if any(
            type(sides) is not tuple or len(sides) != 4
            for sides in self.boundary
        ):
            raise ValueError("each boundary entry must contain four immutable sides")
        if any(
            type(color) is not int
            or (color != COLOR_NONE and not 0 <= color < COLOR_COUNT)
            for sides in self.boundary
            for color in sides
        ):
            raise ValueError("boundary contains an invalid color")
        if any(type(value) is not bool for value in self.active):
            raise ValueError("active storage must contain only booleans")
        for index, sides in enumerate(self.boundary):
            if not self.active[index]:
                if any(color != COLOR_NONE for color in sides):
                    raise ValueError(
                        "inactive cells cannot have boundary constraints"
                    )
                continue

            x = index % self.width
            y = index // self.width
            for direction, (dx, dy) in enumerate(_DIRECTION_OFFSETS):
                neighbor_x = x + dx
                neighbor_y = y + dy
                if not (
                    0 <= neighbor_x < self.width
                    and 0 <= neighbor_y < self.height
                ):
                    continue

                neighbor_index = neighbor_y * self.width + neighbor_x
                if (
                    self.active[neighbor_index]
                    and sides[direction] != COLOR_NONE
                ):
                    raise ValueError(
                        "internal edges cannot have boundary constraints"
                    )
