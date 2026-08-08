#include "wang/tile.h"

#include <assert.h>
#include <stdio.h>


static void test_opposite(void)
{
    assert(opposite(N) == S);
    assert(opposite(S) == N);
    assert(opposite(E) == W);
    assert(opposite(W) == E);

    /* opposite must be an involution */
    for (Dir d = N; d < DIR_COUNT; ++d)
        assert(opposite(opposite(d)) == d);
}


static void test_known_matches(void)
{
    /*
     * Horizontal match:
     *
     * V1.E = COLOR_1
     * F1.W = COLOR_1
     */
    assert(wang_tiles_match(
        &TILESET[TILE_V1],
        E,
        &TILESET[TILE_F1]
    ));

    /*
     * Horizontal mismatch:
     *
     * V1.E = COLOR_1
     * F0.W = COLOR_0
     */
    assert(!wang_tiles_match(
        &TILESET[TILE_V1],
        E,
        &TILESET[TILE_F0]
    ));

    /*
     * Vertical internal glue of generalized V0:
     *
     * V0_TOP.S = COLOR_V0_A
     * V0_MID.N = COLOR_V0_A
     */
    assert(wang_tiles_match(
        &TILESET[TILE_V0_TOP],
        S,
        &TILESET[TILE_V0_MID]
    ));

    /*
     * Second internal V0 glue:
     *
     * V0_MID.S = COLOR_V0_B
     * V0_BOTTOM.N = COLOR_V0_B
     */
    assert(wang_tiles_match(
        &TILESET[TILE_V0_MID],
        S,
        &TILESET[TILE_V0_BOTTOM]
    ));

    /*
     * Wrong V0 pieces must not connect directly.
     */
    assert(!wang_tiles_match(
        &TILESET[TILE_V0_TOP],
        S,
        &TILESET[TILE_V0_BOTTOM]
    ));
}


static void test_match_symmetry(void)
{
    /*
     * Exhaustively check the public matching API.
     *
     * If A matches B in direction d,
     * B must match A in opposite(d).
     *
     * 23 * 23 * 4 = 2116 checks: essentially free.
     */
    for (TileId a = 0; a < TILE_COUNT; ++a) {
        for (TileId b = 0; b < TILE_COUNT; ++b) {
            for (Dir d = N; d < DIR_COUNT; ++d) {

                bool ab = wang_tiles_match(
                    &TILESET[a],
                    d,
                    &TILESET[b]
                );

                bool ba = wang_tiles_match(
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
    test_known_matches();
    test_match_symmetry();
    test_api_rejects_invalid_input();

    puts("test_tile: OK");

    return 0;
}
