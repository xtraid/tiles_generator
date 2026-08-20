#include "../../src/solver/byte_support_table.h"

#include "wang/tile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void build_compat(ByteSupportCompat compat)
{
    for (Dir dir = N; dir < DIR_COUNT; ++dir) {
        for (TileId source = 0; source < TILE_COUNT; ++source) {
            compat[dir][source] = 0;
            for (TileId adjacent = 0; adjacent < TILE_COUNT; ++adjacent) {
                if (wang_tiles_match(
                        &TILESET[source],
                        dir,
                        &TILESET[adjacent]
                    )) {
                    compat[dir][source] |= UINT32_C(1) << adjacent;
                }
            }
        }
    }
}

static uint32_t expected_support(
    ByteSupportCompat compat,
    Dir dir,
    size_t byte,
    unsigned value
)
{
    uint32_t supported = 0;
    for (unsigned bit = 0; bit < WANG_DOMAIN_BYTE_BITS; ++bit) {
        const size_t tile = byte * WANG_DOMAIN_BYTE_BITS + bit;
        if ((value & (1u << bit)) != 0 && tile < TILE_COUNT) {
            supported |= compat[dir][tile];
        }
    }
    return supported;
}

static void test_every_byte_support_entry(void)
{
    ByteSupportCompat compat;
    ByteSupportTables tables;
    build_compat(compat);
    byte_support_tables_build(
        (const ByteSupportCompat *)&compat,
        &tables
    );

    assert(sizeof(tables) == DIR_COUNT * WANG_DOMAIN_BYTE_COUNT *
           WANG_SUPPORT_BYTE_VALUE_COUNT * sizeof(uint32_t));
    for (Dir dir = N; dir < DIR_COUNT; ++dir) {
        for (size_t byte = 0; byte < WANG_DOMAIN_BYTE_COUNT; ++byte) {
            for (unsigned value = 0;
                 value < WANG_SUPPORT_BYTE_VALUE_COUNT;
                 ++value) {
                assert(tables.support[dir][byte][value] ==
                       expected_support(compat, dir, byte, value));
            }
        }
    }
}

int main(void)
{
    test_every_byte_support_entry();
    puts("test_byte_support_table: OK");
    return 0;
}
