/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Linux implementation of the Perftest Lite OS interface.
 *
 * Uses RTI Connext Micro OSAPI wherever possible (time, sleep, semaphore,
 * heap). Falls back to POSIX only for NIC address lookup, which OSAPI
 * does not expose.
 */
#include "os_interface.h"

#ifdef PERFTEST_LITE_OS_LINUX

#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include "osapi/osapi_system.h"
#include "osapi/osapi_thread.h"
#include "osapi/osapi_semaphore.h"
#include "osapi/osapi_heap.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

/* The opaque PerftestLiteSemaphore type from os_interface.h is mapped
 * directly to OSAPI_Semaphore_T via casts (no wrapper struct needed). */

static uint64_t linux_time_now_us(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

static void linux_sleep_us(uint32_t us)
{
    /* OSAPI_Thread_sleep takes ms; OSAPI_Thread_nanosleep takes ns
     * (and its argument is a 32-bit value, so it can only express
     * up to ~4.29 seconds). Split the request: sleep the whole-ms
     * portion via OSAPI_Thread_sleep, then nanosleep the remainder. */
    uint32_t ms       = us / 1000u;
    uint32_t rem_us   = us % 1000u;

    if (ms > 0) {
        OSAPI_Thread_sleep((RTI_UINT32)ms);
    }
    if (rem_us > 0) {
        OSAPI_Thread_nanosleep((RTI_UINT32)(rem_us * 1000u));
    }
}

static PerftestLiteSemaphore *linux_sem_new(void)
{
    return (PerftestLiteSemaphore *)OSAPI_Semaphore_new();
}

static int linux_sem_take(PerftestLiteSemaphore *s, int32_t timeout_ms)
{
    RTI_INT32 reason = 0;
    /* OSAPI uses -1 for infinite wait, matching our convention. */
    RTI_BOOL ok = OSAPI_Semaphore_take((OSAPI_Semaphore_T *)s,
                                       (RTI_INT32)timeout_ms,
                                       &reason);
    return ok ? 1 : -(int)reason;
}

static int linux_sem_give(PerftestLiteSemaphore *s)
{
    return OSAPI_Semaphore_give((OSAPI_Semaphore_T *)s) ? 1 : 0;
}

static void linux_sem_delete(PerftestLiteSemaphore *s)
{
    if (!s) return;
    (void)OSAPI_Semaphore_delete((OSAPI_Semaphore_T *)s);
}

static uint32_t linux_nic_address_ipv4(const char *name)
{
    struct ifaddrs *ifs = NULL, *ifa;
    uint32_t addr = 0xFFFFFFFFu;
    if (!name) return addr;
    if (getifaddrs(&ifs) != 0) return addr;
    for (ifa = ifs; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, name) != 0) continue;
        addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
        break;
    }
    freeifaddrs(ifs);
    return addr;
}

static uint32_t linux_nic_netmask_ipv4(const char *name)
{
    struct ifaddrs *ifs = NULL, *ifa;
    uint32_t netmask = UINT32_MAX;
    if (!name) return netmask;
    if (getifaddrs(&ifs) != 0) return netmask;
    for (ifa = ifs; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!ifa->ifa_netmask || strcmp(ifa->ifa_name, name) != 0) continue;
        netmask = ((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
        break;
    }
    freeifaddrs(ifs);
    return netmask;
}

static const PerftestLiteOsInterface g_linux_os_iface = {
    .time_now_us       = linux_time_now_us,
    .sleep_us          = linux_sleep_us,
    .semaphore_new     = linux_sem_new,
    .semaphore_take    = linux_sem_take,
    .semaphore_give    = linux_sem_give,
    .semaphore_delete  = linux_sem_delete,
    .nic_address_ipv4  = linux_nic_address_ipv4,
    .nic_netmask_ipv4  = linux_nic_netmask_ipv4
};

const PerftestLiteOsInterface *perftest_lite_os_get(void)
{
    return &g_linux_os_iface;
}

#endif /* PERFTEST_LITE_OS_LINUX */
