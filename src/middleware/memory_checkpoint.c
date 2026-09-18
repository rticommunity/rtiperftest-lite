/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/* See memory_checkpoint.h. */
#include "memory_checkpoint.h"
#include "../core/perftest_lite_config.h"

#include "osapi/osapi_heap.h"
#include "rti_me_psl/osapi/osapi_heap_test.h"

#include <stdio.h>

uint64_t perftest_lite_memory_get_osapi_heap_bytes(void)
{
    return (uint64_t)OSAPI_Heap_get_allocated_byte_count();
}

void perftest_lite_memory_emit_checkpoint(const char *name,
                                          const char *category,
                                          uint64_t process_start_bytes)
{
    uint64_t total = perftest_lite_memory_get_osapi_heap_bytes();
    uint64_t delta = total >= process_start_bytes
        ? total - process_start_bytes : 0;

    /* Keep every field as a single token so memory_metrics.py can preserve
     * the complete lifecycle event without relying on a fixed marker shape. */
    PERFTEST_LITE_PRINT(
        "MEM_CHECKPOINT name=%s category=%s osapi_total_bytes=%llu "
        "delta_from_process_start_bytes=%llu\n",
        name, category,
        (unsigned long long)total,
        (unsigned long long)delta);
    fflush(stdout);
}