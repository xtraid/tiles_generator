#include "wang/tile.h"
#include <stddef.h>

Dir opposite(Dir dir)
{
    switch (dir) {
    case N: return S;
    case E: return W;
    case S: return N;
    case W: return E;
    case DIR_COUNT:
        break;
    }

    return DIR_COUNT;
}

bool wang_tiles_match(
    const WangTile *a,
    Dir direction_to_b,
    const WangTile *b)
{
    if (a == NULL || b == NULL || (unsigned)direction_to_b >= DIR_COUNT)
        return false;

    return a->edge[direction_to_b]
        == b->edge[opposite(direction_to_b)];
}

const WangTile TILESET[TILE_COUNT] = {

    /* =====================
     * V0 - 3 Wang tiles
     * ===================== */

    [TILE_V0_TOP] = {
        .id   = TILE_V0_TOP,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_0,
            [S] = COLOR_V0_A,
            [W] = COLOR_V,
        },
        .kind = GEN_V0,
    },

    [TILE_V0_MID] = {
        .id   = TILE_V0_MID,
        .edge = {
            [N] = COLOR_V0_A,
            [E] = COLOR_0,
            [S] = COLOR_V0_B,
            [W] = COLOR_V,
        },
        .kind = GEN_V0,
    },

    [TILE_V0_BOTTOM] = {
        .id   = TILE_V0_BOTTOM,
        .edge = {
            [N] = COLOR_V0_B,
            [E] = COLOR_0,
            [S] = COLOR_B,
            [W] = COLOR_V,
        },
        .kind = GEN_V0,
    },


    /* =====================
     * V1 - 1 Wang tile
     * ===================== */

    [TILE_V1] = {
        .id   = TILE_V1,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_1,
            [S] = COLOR_B,
            [W] = COLOR_V,
        },
        .kind = GEN_V1,
    },


    /* =====================
     * C0 - 1 Wang tile
     * ===================== */

    [TILE_C0] = {
        .id   = TILE_C0,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_0_PRIME,
            [S] = COLOR_B,
            [W] = COLOR_0,
        },
        .kind = GEN_C0,
    },


    /* =====================
     * C1 - 2 Wang tiles
     * ===================== */

    [TILE_C1_TOP] = {
        .id   = TILE_C1_TOP,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_0_PRIME,
            [S] = COLOR_C1,
            [W] = COLOR_1,
        },
        .kind = GEN_C1,
    },

    [TILE_C1_BOTTOM] = {
        .id   = TILE_C1_BOTTOM,
        .edge = {
            [N] = COLOR_C1,
            [E] = COLOR_1,
            [S] = COLOR_B,
            [W] = COLOR_0,
        },
        .kind = GEN_C1,
    },


    /* =====================
     * Forwarders F0, F1
     * ===================== */

    [TILE_F0] = {
        .id   = TILE_F0,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_0,
            [S] = COLOR_B,
            [W] = COLOR_0,
        },
        .kind = GEN_F0,
    },

    [TILE_F1] = {
        .id   = TILE_F1,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_1,
            [S] = COLOR_B,
            [W] = COLOR_1,
        },
        .kind = GEN_F1,
    },


    /* =====================
     * Left anchors L0, L1
     * ===================== */

    [TILE_L0] = {
        .id   = TILE_L0,
        .edge = {
            [N] = COLOR_L,
            [E] = COLOR_0,
            [S] = COLOR_L,
            [W] = COLOR_0,
        },
        .kind = GEN_L0,
    },

    [TILE_L1] = {
        .id   = TILE_L1,
        .edge = {
            [N] = COLOR_L,
            [E] = COLOR_1,
            [S] = COLOR_L,
            [W] = COLOR_1,
        },
        .kind = GEN_L1,
    },


    /* =====================
     * Right anchor R0
     * ===================== */

    [TILE_R0_LEFT] = {
        .id   = TILE_R0_LEFT,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_R0,
            [S] = COLOR_R,
            [W] = COLOR_0,
        },
        .kind = GEN_R0,
    },

    [TILE_R0_RIGHT] = {
        .id   = TILE_R0_RIGHT,
        .edge = {
            [N] = COLOR_R,
            [E] = COLOR_0,
            [S] = COLOR_B,
            [W] = COLOR_R0,
        },
        .kind = GEN_R0,
    },


    /* =====================
     * Right anchor R1
     * ===================== */

    [TILE_R1_LEFT] = {
        .id   = TILE_R1_LEFT,
        .edge = {
            [N] = COLOR_B,
            [E] = COLOR_R1,
            [S] = COLOR_R,
            [W] = COLOR_1,
        },
        .kind = GEN_R1,
    },

    [TILE_R1_RIGHT] = {
        .id   = TILE_R1_RIGHT,
        .edge = {
            [N] = COLOR_R,
            [E] = COLOR_1,
            [S] = COLOR_B,
            [W] = COLOR_R1,
        },
        .kind = GEN_R1,
    },


    /* =====================
     * X00
     * ===================== */

    [TILE_X00_TOP] = {
        .id   = TILE_X00_TOP,
        .edge = {
            [N] = COLOR_R,
            [E] = COLOR_0,
            [S] = COLOR_X00,
            [W] = COLOR_0,
        },
        .kind = GEN_X00,
    },

    [TILE_X00_BOTTOM] = {
        .id   = TILE_X00_BOTTOM,
        .edge = {
            [N] = COLOR_X00,
            [E] = COLOR_0,
            [S] = COLOR_L,
            [W] = COLOR_0,
        },
        .kind = GEN_X00,
    },


    /* =====================
     * X01
     * ===================== */

    [TILE_X01_TOP] = {
        .id   = TILE_X01_TOP,
        .edge = {
            [N] = COLOR_R,
            [E] = COLOR_0,
            [S] = COLOR_X01,
            [W] = COLOR_1,
        },
        .kind = GEN_X01,
    },

    [TILE_X01_BOTTOM] = {
        .id   = TILE_X01_BOTTOM,
        .edge = {
            [N] = COLOR_X01,
            [E] = COLOR_1,
            [S] = COLOR_L,
            [W] = COLOR_0,
        },
        .kind = GEN_X01,
    },


    /* =====================
     * X10
     * ===================== */

    [TILE_X10_TOP] = {
        .id   = TILE_X10_TOP,
        .edge = {
            [N] = COLOR_R,
            [E] = COLOR_1,
            [S] = COLOR_X10,
            [W] = COLOR_0,
        },
        .kind = GEN_X10,
    },

    [TILE_X10_BOTTOM] = {
        .id   = TILE_X10_BOTTOM,
        .edge = {
            [N] = COLOR_X10,
            [E] = COLOR_0,
            [S] = COLOR_L,
            [W] = COLOR_1,
        },
        .kind = GEN_X10,
    },


    /* =====================
     * X11
     * ===================== */

    [TILE_X11_TOP] = {
        .id   = TILE_X11_TOP,
        .edge = {
            [N] = COLOR_R,
            [E] = COLOR_1,
            [S] = COLOR_X11,
            [W] = COLOR_1,
        },
        .kind = GEN_X11,
    },

    [TILE_X11_BOTTOM] = {
        .id   = TILE_X11_BOTTOM,
        .edge = {
            [N] = COLOR_X11,
            [E] = COLOR_1,
            [S] = COLOR_L,
            [W] = COLOR_1,
        },
        .kind = GEN_X11,
    },
};
