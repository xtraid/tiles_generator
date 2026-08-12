#include "wang/tile.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static void test_opposite(void)
{
    assert(opposite(N) == S);
    assert(opposite(S) == N);
    assert(opposite(E) == W);
    assert(opposite(W) == E);

    for (Dir d = N; d < DIR_COUNT; ++d) {
        assert(opposite(opposite(d)) == d);
    }

    assert(opposite(DIR_COUNT) == DIR_COUNT);
}

static void test_tileset_identity_and_ranges(void)
{
    size_t macro_counts[GEN_COUNT] = {0};

    for (TileId id = 0; id < TILE_COUNT; ++id) {
        const WangTile *tile = &TILESET[id];

        assert(tile->id == id);
        assert((unsigned)tile->kind < GEN_COUNT);

        ++macro_counts[tile->kind];

        for (Dir d = N; d < DIR_COUNT; ++d) {
            assert(tile->edge[d] < COLOR_COUNT);
            assert(tile->edge[d] != COLOR_NONE);
        }
    }

    assert(macro_counts[GEN_V0]  == 3);
    assert(macro_counts[GEN_V1]  == 1);
    assert(macro_counts[GEN_C0]  == 1);
    assert(macro_counts[GEN_C1]  == 2);
    assert(macro_counts[GEN_F0]  == 1);
    assert(macro_counts[GEN_F1]  == 1);
    assert(macro_counts[GEN_L0]  == 1);
    assert(macro_counts[GEN_L1]  == 1);
    assert(macro_counts[GEN_R0]  == 2);
    assert(macro_counts[GEN_R1]  == 2);
    assert(macro_counts[GEN_X00] == 2);
    assert(macro_counts[GEN_X01] == 2);
    assert(macro_counts[GEN_X10] == 2);
    assert(macro_counts[GEN_X11] == 2);
}

static void test_tileset_golden_edges(void)
{
    static const ColorId expected[TILE_COUNT][DIR_COUNT] = {
        [TILE_V0_TOP]      = { COLOR_B,     COLOR_0,       COLOR_V0_A, COLOR_V },
        [TILE_V0_MID]      = { COLOR_V0_A,  COLOR_0,       COLOR_V0_B, COLOR_V },
        [TILE_V0_BOTTOM]   = { COLOR_V0_B,  COLOR_0,       COLOR_B,    COLOR_V },
        [TILE_V1]          = { COLOR_B,     COLOR_1,       COLOR_B,    COLOR_V },
        [TILE_C0]          = { COLOR_B,     COLOR_0_PRIME, COLOR_B,    COLOR_0 },
        [TILE_C1_TOP]      = { COLOR_B,     COLOR_0_PRIME, COLOR_C1,   COLOR_1 },
        [TILE_C1_BOTTOM]   = { COLOR_C1,    COLOR_1,       COLOR_B,    COLOR_0 },
        [TILE_F0]          = { COLOR_B,     COLOR_0,       COLOR_B,    COLOR_0 },
        [TILE_F1]          = { COLOR_B,     COLOR_1,       COLOR_B,    COLOR_1 },
        [TILE_L0]          = { COLOR_L,     COLOR_0,       COLOR_L,    COLOR_0 },
        [TILE_L1]          = { COLOR_L,     COLOR_1,       COLOR_L,    COLOR_1 },
        [TILE_R0_LEFT]     = { COLOR_B,     COLOR_R0,      COLOR_R,    COLOR_0 },
        [TILE_R0_RIGHT]    = { COLOR_R,     COLOR_0,       COLOR_B,    COLOR_R0 },
        [TILE_R1_LEFT]     = { COLOR_B,     COLOR_R1,      COLOR_R,    COLOR_1 },
        [TILE_R1_RIGHT]    = { COLOR_R,     COLOR_1,       COLOR_B,    COLOR_R1 },
        [TILE_X00_TOP]     = { COLOR_R,     COLOR_0,       COLOR_X00,  COLOR_0 },
        [TILE_X00_BOTTOM]  = { COLOR_X00,   COLOR_0,       COLOR_L,    COLOR_0 },
        [TILE_X01_TOP]     = { COLOR_R,     COLOR_0,       COLOR_X01,  COLOR_1 },
        [TILE_X01_BOTTOM]  = { COLOR_X01,   COLOR_1,       COLOR_L,    COLOR_0 },
        [TILE_X10_TOP]     = { COLOR_R,     COLOR_1,       COLOR_X10,  COLOR_0 },
        [TILE_X10_BOTTOM]  = { COLOR_X10,   COLOR_0,       COLOR_L,    COLOR_1 },
        [TILE_X11_TOP]     = { COLOR_R,     COLOR_1,       COLOR_X11,  COLOR_1 },
        [TILE_X11_BOTTOM]  = { COLOR_X11,   COLOR_1,       COLOR_L,    COLOR_1 },
    };

    for (TileId id = 0; id < TILE_COUNT; ++id) {
        for (Dir dir = N; dir < DIR_COUNT; ++dir) {
            assert(TILESET[id].edge[dir] == expected[id][dir]);
        }
    }
}

static void test_known_matches(void)
{
    assert(wang_tiles_match(
        &TILESET[TILE_V1],
        E,
        &TILESET[TILE_F1]
    ));

    assert(!wang_tiles_match(
        &TILESET[TILE_V1],
        E,
        &TILESET[TILE_F0]
    ));

    assert(wang_tiles_match(
        &TILESET[TILE_V0_TOP],
        S,
        &TILESET[TILE_V0_MID]
    ));

    assert(wang_tiles_match(
        &TILESET[TILE_V0_MID],
        S,
        &TILESET[TILE_V0_BOTTOM]
    ));

    assert(!wang_tiles_match(
        &TILESET[TILE_V0_TOP],
        S,
        &TILESET[TILE_V0_BOTTOM]
    ));
}

static void test_match_symmetry(void)
{
    for (TileId a = 0; a < TILE_COUNT; ++a) {
        for (TileId b = 0; b < TILE_COUNT; ++b) {
            for (Dir d = N; d < DIR_COUNT; ++d) {
                const bool ab = wang_tiles_match(
                    &TILESET[a],
                    d,
                    &TILESET[b]
                );

                const bool ba = wang_tiles_match(
                    &TILESET[b],
                    opposite(d),
                    &TILESET[a]
                );

                assert(ab == ba);
            }
        }
    }
}

static void test_api_rejects_invalid_input(void)
{
    assert(!wang_tiles_match(
        NULL,
        E,
        &TILESET[TILE_V1]
    ));

    assert(!wang_tiles_match(
        &TILESET[TILE_V1],
        E,
        NULL
    ));

    assert(!wang_tiles_match(
        &TILESET[TILE_V1],
        DIR_COUNT,
        &TILESET[TILE_F1]
    ));
}

int main(void)
{
    test_opposite();
    test_tileset_identity_and_ranges();
    test_tileset_golden_edges();
    test_known_matches();
    test_match_symmetry();
    test_api_rejects_invalid_input();

    puts("test_tile: OK");
    return 0;
}
