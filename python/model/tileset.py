"""Canonical immutable Python representation of the 23 atomic Wang tiles.

Tuple position is the tile ID and each tile stores only ``(N, E, S, W)``
colors. Generalized-tile metadata is deliberately absent from solver input.
"""

from typing import Final, TypeAlias


Color: TypeAlias = int
Tile: TypeAlias = tuple[Color, Color, Color, Color]
Tileset: TypeAlias = tuple[Tile, ...]

N: Final = 0
E: Final = 1
S: Final = 2
W: Final = 3
DIR_COUNT: Final = 4

COLOR_B: Final = 0
COLOR_V: Final = 1
COLOR_0: Final = 2
COLOR_1: Final = 3
COLOR_0_PRIME: Final = 4
COLOR_L: Final = 5
COLOR_R: Final = 6
COLOR_V0_A: Final = 7
COLOR_V0_B: Final = 8
COLOR_C1: Final = 9
COLOR_R0: Final = 10
COLOR_R1: Final = 11
COLOR_X00: Final = 12
COLOR_X01: Final = 13
COLOR_X10: Final = 14
COLOR_X11: Final = 15
COLOR_COUNT: Final = 16
COLOR_NONE: Final = 255

TILE_V0_TOP: Final = 0
TILE_V0_MID: Final = 1
TILE_V0_BOTTOM: Final = 2
TILE_V1: Final = 3
TILE_C0: Final = 4
TILE_C1_TOP: Final = 5
TILE_C1_BOTTOM: Final = 6
TILE_F0: Final = 7
TILE_F1: Final = 8
TILE_L0: Final = 9
TILE_L1: Final = 10
TILE_R0_LEFT: Final = 11
TILE_R0_RIGHT: Final = 12
TILE_R1_LEFT: Final = 13
TILE_R1_RIGHT: Final = 14
TILE_X00_TOP: Final = 15
TILE_X00_BOTTOM: Final = 16
TILE_X01_TOP: Final = 17
TILE_X01_BOTTOM: Final = 18
TILE_X10_TOP: Final = 19
TILE_X10_BOTTOM: Final = 20
TILE_X11_TOP: Final = 21
TILE_X11_BOTTOM: Final = 22
TILE_COUNT: Final = 23

TILESET: Final[Tileset] = (
    (COLOR_B, COLOR_0, COLOR_V0_A, COLOR_V),
    (COLOR_V0_A, COLOR_0, COLOR_V0_B, COLOR_V),
    (COLOR_V0_B, COLOR_0, COLOR_B, COLOR_V),
    (COLOR_B, COLOR_1, COLOR_B, COLOR_V),
    (COLOR_B, COLOR_0_PRIME, COLOR_B, COLOR_0),
    (COLOR_B, COLOR_0_PRIME, COLOR_C1, COLOR_1),
    (COLOR_C1, COLOR_1, COLOR_B, COLOR_0),
    (COLOR_B, COLOR_0, COLOR_B, COLOR_0),
    (COLOR_B, COLOR_1, COLOR_B, COLOR_1),
    (COLOR_L, COLOR_0, COLOR_L, COLOR_0),
    (COLOR_L, COLOR_1, COLOR_L, COLOR_1),
    (COLOR_B, COLOR_R0, COLOR_R, COLOR_0),
    (COLOR_R, COLOR_0, COLOR_B, COLOR_R0),
    (COLOR_B, COLOR_R1, COLOR_R, COLOR_1),
    (COLOR_R, COLOR_1, COLOR_B, COLOR_R1),
    (COLOR_R, COLOR_0, COLOR_X00, COLOR_0),
    (COLOR_X00, COLOR_0, COLOR_L, COLOR_0),
    (COLOR_R, COLOR_0, COLOR_X01, COLOR_1),
    (COLOR_X01, COLOR_1, COLOR_L, COLOR_0),
    (COLOR_R, COLOR_1, COLOR_X10, COLOR_0),
    (COLOR_X10, COLOR_0, COLOR_L, COLOR_1),
    (COLOR_R, COLOR_1, COLOR_X11, COLOR_1),
    (COLOR_X11, COLOR_1, COLOR_L, COLOR_1),
)
