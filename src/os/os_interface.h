/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * OS Interface (per design: OS Interface section).
 *
 * Encapsulates the OS-specific operations used by Perftest Lite. Porting
 * to a new platform means implementing this interface (and registering
 * it via perftest_lite_os_get()).
 *
 * Operations exposed:
 *   - High-resolution monotonic clock (microseconds since epoch).
 *   - Sleep with microsecond granularity.
 *   - Lightweight binary semaphore (used for the latency test).
 *   - Print log message hook (left to PERFTEST_LITE_PRINT macro).
 *
 * Threads are not exposed: the engine is single-threaded. Listener
 * callbacks run on the middleware's internal threads, which is the
 * middleware's concern, not the OS interface's.
 */
#ifndef PERFTEST_LITE_OS_INTERFACE_H
#define PERFTEST_LITE_OS_INTERFACE_H

#include <stdint.h>

/* Source-level version of the customer-implemented OS adapter contract. */
#define PERFTEST_LITE_OS_INTERFACE_VERSION 2

/* -----------------------------------------------------------------------
 * OS backend selector.
 *
 * Exactly one of the PERFTEST_LITE_OS_* macros must be defined. Each
 * backend implementation file (os_linux.c, os_forge.c, os_qnx.c, ...)
 * gates its own body on the matching macro so that a build system that
 * compiles every os_*.c (e.g. Forge's auto-glob) still produces a single
 * definition of perftest_lite_os_get().
 *
 * If nothing is defined, default to Linux to keep the host build working.
 * -----------------------------------------------------------------------
 */
#if !defined(PERFTEST_LITE_OS_LINUX)    \
 && !defined(PERFTEST_LITE_OS_QNX)      \
 && !defined(PERFTEST_LITE_OS_AUTOSAR)  \
 && !defined(PERFTEST_LITE_OS_FORGE)    \
 && !defined(PERFTEST_LITE_OS_CUSTOM)
#  define PERFTEST_LITE_OS_LINUX 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PerftestLiteSemaphoreImpl PerftestLiteSemaphore;

typedef struct PerftestLiteOsInterface {
    /* Monotonic accumulated microseconds; the epoch is platform-defined. */
    uint64_t (*time_now_us)(void);
    /* Delay for at least the requested duration, subject to scheduler resolution. */
    void     (*sleep_us)(uint32_t us);

    PerftestLiteSemaphore *(*semaphore_new)(void);
    /* Return positive on success and non-positive on timeout or failure.
     * A negative timeout requests an infinite wait. */
    int  (*semaphore_take)(PerftestLiteSemaphore *sem, int32_t timeout_ms);
    /* Return positive on success and non-positive on failure. */
    int  (*semaphore_give)(PerftestLiteSemaphore *sem);
    void (*semaphore_delete)(PerftestLiteSemaphore *sem);

    /* Return the interface's IPv4 address in network byte order. Return
     * UINT32_MAX if the platform cannot resolve the interface. */
    uint32_t (*nic_address_ipv4)(const char *interface_name);
    /* Return the interface's IPv4 netmask in network byte order. Return
     * UINT32_MAX if the platform cannot resolve the interface. */
    uint32_t (*nic_netmask_ipv4)(const char *interface_name);
} PerftestLiteOsInterface;

/* Returns the OS interface implementation for the current platform.
 * Selected at compile time by the build system. */
const PerftestLiteOsInterface *perftest_lite_os_get(void);

#ifdef __cplusplus
}
#endif

#endif /* PERFTEST_LITE_OS_INTERFACE_H */
