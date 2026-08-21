#define _POSIX_C_SOURCE 200809L

#include "failed_leaf_trace.h"

#include "wang/tile.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define TRACE_HEADER_SIZE 64u
#define TRACE_RECORD_HEADER_SIZE 32u
#define TRACE_VERSION UINT32_C(1)
#define TRACE_FLAG_TRUNCATED UINT32_C(1)

static bool checked_add(size_t a, size_t b, size_t *out)
{
    if (a > SIZE_MAX - b) {
        return false;
    }
    *out = a + b;
    return true;
}

static bool checked_mul(size_t a, size_t b, size_t *out)
{
    if (a != 0 && b > SIZE_MAX / a) {
        return false;
    }
    *out = a * b;
    return true;
}

static bool checked_align_8(size_t value, size_t *out)
{
    const size_t remainder = value % 8u;
    return remainder == 0
        ? (*out = value, true)
        : checked_add(value, 8u - remainder, out);
}

static void discard_open_trace(FailedLeafWriter *writer)
{
    if (writer->fd >= 0) {
        (void)close(writer->fd);
        writer->fd = -1;
    }
    if (writer->path != NULL) {
        (void)unlink(writer->path);
        free(writer->path);
        writer->path = NULL;
    }
}

static void put_u32(unsigned char *destination, uint32_t value)
{
    for (unsigned byte = 0; byte < 4; ++byte) {
        destination[byte] = (unsigned char)(value >> (8u * byte));
    }
}

static void put_u64(unsigned char *destination, uint64_t value)
{
    for (unsigned byte = 0; byte < 8; ++byte) {
        destination[byte] = (unsigned char)(value >> (8u * byte));
    }
}

static void write_file_header(
    FailedLeafWriter *writer,
    const Region *region,
    size_t cell_count
)
{
    static const unsigned char magic[8] = {
        'W', '2', '3', 'L', 'E', 'A', 'F', '\0'
    };

    memset(writer->mapping, 0, TRACE_HEADER_SIZE);
    memcpy(writer->mapping, magic, sizeof(magic));
    put_u32(writer->mapping + 8, TRACE_VERSION);
    put_u32(writer->mapping + 12, TRACE_HEADER_SIZE);
    put_u32(writer->mapping + 16, (uint32_t)region->width);
    put_u32(writer->mapping + 20, (uint32_t)region->height);
    put_u32(writer->mapping + 24, TILE_COUNT);
    put_u64(writer->mapping + 32, (uint64_t)cell_count);
    put_u64(writer->mapping + 40, (uint64_t)writer->record_size);
    put_u64(writer->mapping + 48, (uint64_t)writer->capacity);
}

bool failed_leaf_writer_init(
    FailedLeafWriter *writer,
    const char *path,
    size_t capacity,
    const Region *region,
    size_t cell_count
)
{
    *writer = (FailedLeafWriter){ .fd = -1 };

    size_t domain_bytes;
    size_t raw_record_size;
    size_t record_bytes;
    size_t mapped_size;
    if (path == NULL || path[0] == '\0' || capacity == 0 ||
        !checked_mul(cell_count, sizeof(uint32_t), &domain_bytes) ||
        !checked_add(
            TRACE_RECORD_HEADER_SIZE,
            domain_bytes,
            &raw_record_size
        ) ||
        !checked_align_8(raw_record_size, &writer->record_size) ||
        !checked_mul(capacity, writer->record_size, &record_bytes) ||
        !checked_add(TRACE_HEADER_SIZE, record_bytes, &mapped_size)) {
        return false;
    }

    const off_t file_size = (off_t)mapped_size;
    if (file_size < 0 || (uintmax_t)file_size != (uintmax_t)mapped_size) {
        return false;
    }

    writer->path = strdup(path);
    if (writer->path == NULL) {
        return false;
    }

    writer->fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (writer->fd < 0) {
        free(writer->path);
        writer->path = NULL;
        return false;
    }
    if (ftruncate(writer->fd, file_size) != 0) {
        discard_open_trace(writer);
        return false;
    }

    void *mapping = mmap(
        NULL,
        mapped_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        writer->fd,
        0
    );
    if (mapping == MAP_FAILED) {
        discard_open_trace(writer);
        return false;
    }

    writer->mapping = mapping;
    writer->mapped_size = mapped_size;
    writer->capacity = capacity;
    writer->active = true;
    write_file_header(writer, region, cell_count);
    return true;
}

bool failed_leaf_writer_write(
    FailedLeafWriter *writer,
    const uint32_t *domains,
    size_t cell_count,
    size_t conflict_cell,
    size_t depth,
    size_t resolved_count
)
{
    if (!writer->active) {
        return true;
    }
    if (writer->failed) {
        return false;
    }
    if (writer->count == writer->capacity) {
        writer->truncated = true;
        return true;
    }

    size_t relative_offset;
    size_t record_offset;
    if (!checked_mul(writer->count, writer->record_size, &relative_offset) ||
        !checked_add(TRACE_HEADER_SIZE, relative_offset, &record_offset) ||
        record_offset > writer->mapped_size ||
        writer->record_size > writer->mapped_size - record_offset) {
        writer->failed = true;
        return false;
    }

    unsigned char *record = writer->mapping + record_offset;
    memset(record, 0, writer->record_size);
    put_u64(record, (uint64_t)writer->count);
    put_u64(record + 8, (uint64_t)conflict_cell);
    put_u64(record + 16, (uint64_t)depth);
    put_u64(record + 24, (uint64_t)resolved_count);

    unsigned char *output = record + TRACE_RECORD_HEADER_SIZE;
    for (size_t i = 0; i < cell_count; ++i) {
        put_u32(output + i * sizeof(uint32_t), domains[i]);
    }

    ++writer->count;
    return true;
}

bool failed_leaf_writer_finish(FailedLeafWriter *writer)
{
    if (!writer->active) {
        return true;
    }

    size_t used_size = 0;
    bool ok = !writer->failed;
    if (ok) {
        put_u32(
            writer->mapping + 28,
            writer->truncated ? TRACE_FLAG_TRUNCATED : 0
        );
        put_u64(writer->mapping + 56, (uint64_t)writer->count);

        size_t record_bytes;
        ok = checked_mul(
            writer->count,
            writer->record_size,
            &record_bytes
        ) && checked_add(TRACE_HEADER_SIZE, record_bytes, &used_size);
    }

    if (ok && msync(writer->mapping, used_size, MS_SYNC) != 0) {
        ok = false;
    }
    if (munmap(writer->mapping, writer->mapped_size) != 0) {
        ok = false;
    }
    writer->mapping = NULL;

    if (ok) {
        const off_t final_size = (off_t)used_size;
        if (final_size < 0 ||
            (uintmax_t)final_size != (uintmax_t)used_size ||
            ftruncate(writer->fd, final_size) != 0) {
            ok = false;
        }
    }
    if (close(writer->fd) != 0) {
        ok = false;
    }

    if (!ok) {
        (void)unlink(writer->path);
    }
    free(writer->path);
    *writer = (FailedLeafWriter){ .fd = -1 };
    return ok;
}
