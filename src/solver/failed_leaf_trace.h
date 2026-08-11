#ifndef WANG_FAILED_LEAF_TRACE_H
#define WANG_FAILED_LEAF_TRACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wang/region.h"

typedef struct {
    int fd;
    char *path;
    unsigned char *mapping;
    size_t mapped_size;
    size_t record_size;
    size_t capacity;
    size_t count;
    bool truncated;
    bool active;
} FailedLeafWriter;

bool failed_leaf_writer_init(
    FailedLeafWriter *writer,
    const char *path,
    size_t capacity,
    const Region *region,
    size_t cell_count
);

bool failed_leaf_writer_write(
    FailedLeafWriter *writer,
    const uint32_t *domains,
    size_t cell_count,
    size_t conflict_cell,
    size_t depth,
    size_t resolved_count
);

bool failed_leaf_writer_finish(FailedLeafWriter *writer);

#endif /* WANG_FAILED_LEAF_TRACE_H */
