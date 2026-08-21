#define _POSIX_C_SOURCE 200809L

#include "../../src/solver/failed_leaf_trace.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_record_bound_rejection_removes_trace(void)
{
    char path[] = "/tmp/wang-leaf-bound-XXXXXX";
    const int temporary_fd = mkstemp(path);
    assert(temporary_fd >= 0);
    assert(close(temporary_fd) == 0);

    Region region = {0};
    assert(region_init(&region, 1, 1));
    region.cells[0].active = true;

    FailedLeafWriter writer = { .fd = -1 };
    assert(failed_leaf_writer_init(&writer, path, 1, &region, 1));

    const size_t mapped_size = writer.mapped_size;
    writer.mapped_size = 0;

    const uint32_t domains[] = {0};
    assert(!failed_leaf_writer_write(&writer, domains, 1, 0, 0, 0));

    writer.mapped_size = mapped_size;
    assert(!failed_leaf_writer_write(&writer, domains, 1, 0, 0, 0));
    assert(!failed_leaf_writer_finish(&writer));
    assert(access(path, F_OK) != 0);

    region_destroy(&region);
}

int main(void)
{
    test_record_bound_rejection_removes_trace();
    puts("test_failed_leaf_trace: OK");
    return 0;
}
