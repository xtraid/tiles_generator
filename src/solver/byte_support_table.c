#include "byte_support_table.h"

#include <string.h>

static TileId first_set_tile(uint32_t domain)
{
    TileId tile = 0;
    while ((domain & UINT32_C(1)) == 0) {
        domain >>= 1;
        ++tile;
    }
    return tile;
}

void byte_support_tables_build(
    const ByteSupportCompat *compat,
    ByteSupportTables *byte_support
)
{
    memset(byte_support, 0, sizeof(*byte_support));

    for (Dir dir = N; dir < DIR_COUNT; ++dir) {
        for (size_t byte = 0; byte < WANG_DOMAIN_BYTE_COUNT; ++byte) {
            for (unsigned value = 1;
                 value < WANG_SUPPORT_BYTE_VALUE_COUNT;
                 ++value) {
                const unsigned previous = value & (value - 1u);
                const size_t tile = byte * WANG_DOMAIN_BYTE_BITS +
                    first_set_tile((uint32_t)value);
                uint32_t supported =
                    byte_support->support[dir][byte][previous];
                if (tile < TILE_COUNT) {
                    supported |= (*compat)[dir][tile];
                }
                byte_support->support[dir][byte][value] = supported;
            }
        }
    }
}
