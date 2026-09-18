/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Lifecycle-scoped memory checkpoints for Connext Micro builds.
 *
 * The emitted values are OSAPI heap counters, not process RSS or an
 * allocator high-water mark.  A caller supplies the process-start baseline
 * so reports can distinguish middleware startup allocations from application
 * allocations made later.
 */
#ifndef PERFTEST_LITE_MEMORY_CHECKPOINT_H
#define PERFTEST_LITE_MEMORY_CHECKPOINT_H

#include <stdint.h>

uint64_t perftest_lite_memory_get_osapi_heap_bytes(void);

void perftest_lite_memory_emit_checkpoint(const char *name,
                                          const char *category,
                                          uint64_t process_start_bytes);

#endif /* PERFTEST_LITE_MEMORY_CHECKPOINT_H */
