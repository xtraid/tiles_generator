#ifndef TILES_H
#define TILES_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t ColorId;
typedef uint8_t TileId;

#define COLOR_NONE ((ColorId)UINT8_MAX)

/* =========================
 * Directions
 * ========================= */

typedef enum {
    N = 0,
    E,
    S,
    W,
    DIR_COUNT
} Dir;


/* =========================
 * Colors
 * ========================= */

/*
 * External colors are the named colors in Yang-Zhang Figure 1.
 * Internal colors are implementation names for the unique glue
 * colors used to force atomic Wang tiles into generalized tiles.
 */
enum {
    COLOR_B = 0,
    COLOR_V,
    COLOR_0,
    COLOR_1,
    COLOR_0_PRIME,
    COLOR_L,
    COLOR_R,

    /* unique internal glue colors */
    COLOR_V0_A,
    COLOR_V0_B,

    COLOR_C1,

    COLOR_R0,
    COLOR_R1,

    COLOR_X00,
    COLOR_X01,
    COLOR_X10,
    COLOR_X11,

    COLOR_COUNT
};


/* =========================
 * Generalized Wang tiles
 * metadata only
 * ========================= */

typedef enum {
    GEN_V0,
    GEN_V1,

    GEN_C0,
    GEN_C1,

    GEN_F0,
    GEN_F1,

    GEN_L0,
    GEN_L1,

    GEN_R0,
    GEN_R1,

    GEN_X00,
    GEN_X01,
    GEN_X10,
    GEN_X11,

    GEN_COUNT
} GenWangTile;


/* =========================
 * Atomic Wang tile IDs
 * ========================= */

enum {
    TILE_V0_TOP = 0,
    TILE_V0_MID,
    TILE_V0_BOTTOM,

    TILE_V1,

    TILE_C0,

    TILE_C1_TOP,
    TILE_C1_BOTTOM,

    TILE_F0,
    TILE_F1,

    TILE_L0,
    TILE_L1,

    TILE_R0_LEFT,
    TILE_R0_RIGHT,

    TILE_R1_LEFT,
    TILE_R1_RIGHT,

    TILE_X00_TOP,
    TILE_X00_BOTTOM,

    TILE_X01_TOP,
    TILE_X01_BOTTOM,

    TILE_X10_TOP,
    TILE_X10_BOTTOM,

    TILE_X11_TOP,
    TILE_X11_BOTTOM,

    TILE_COUNT
};


_Static_assert(TILE_COUNT == 23,
               "Yang-Zhang tileset must contain exactly 23 tiles");

_Static_assert(GEN_COUNT == 14,
               "Yang-Zhang construction must contain 14 generalized tiles");

_Static_assert(TILE_COUNT <= 32,
               "32-bit tile domains require at most 32 tiles");

_Static_assert(COLOR_COUNT < UINT8_MAX,
               "ColorId cannot represent all colors plus COLOR_NONE");


/* =========================
 * Atomic Wang tile
 * ========================= */

typedef struct {
    TileId id;
    ColorId edge[DIR_COUNT];  /* N, E, S, W */

    /*
     * Diagnostic/builder metadata only.
     * The Wang solver must not use generalized-tile semantics.
     */
    GenWangTile kind;
} WangTile;


extern const WangTile TILESET[TILE_COUNT];


/* =========================
 * Public API
 * ========================= */

Dir opposite(Dir dir);

bool wang_tiles_match(
    const WangTile *a,
    Dir direction_to_b,
    const WangTile *b
);

#endif /* TILES_H */
