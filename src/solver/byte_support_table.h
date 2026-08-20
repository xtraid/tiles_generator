#ifndef WANG_BYTE_SUPPORT_TABLE_H
#define WANG_BYTE_SUPPORT_TABLE_H

#include <stddef.h>
#include <stdint.h>

#include "wang/tile.h"

enum {
    WANG_DOMAIN_BYTE_BITS = 8,
    WANG_DOMAIN_BYTE_COUNT =
        (TILE_COUNT + WANG_DOMAIN_BYTE_BITS - 1) / WANG_DOMAIN_BYTE_BITS,
    WANG_SUPPORT_BYTE_VALUE_COUNT = UINT8_MAX + 1u
};

typedef struct {
    uint32_t support[DIR_COUNT]
                    [WANG_DOMAIN_BYTE_COUNT]
                    [WANG_SUPPORT_BYTE_VALUE_COUNT];
} ByteSupportTables;

typedef uint32_t ByteSupportCompat[DIR_COUNT][TILE_COUNT];

void byte_support_tables_build(
    const ByteSupportCompat *compat,
    ByteSupportTables *byte_support
);

#endif /* WANG_BYTE_SUPPORT_TABLE_H */
