/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * QNX Neutrino implementation of the Perftest Lite OS interface.
 *
 * Uses RTI Connext Micro OSAPI wherever possible (sleep, semaphore).
 * Uses QNX ClockCycles() for high-resolution time.
 * Uses getifaddrs for NIC address lookup.
 */
#include "os_interface.h"

#ifdef PERFTEST_LITE_OS_QNX

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "osapi/osapi_system.h"
#include "osapi/osapi_thread.h"
#include "osapi/osapi_semaphore.h"
#include "osapi/osapi_heap.h"

/* The opaque PerftestLiteSemaphore type from os_interface.h is mapped
 * directly to OSAPI_Semaphore_T via casts (no wrapper struct needed). */

static uint64_t qnx_time_now_us(void)
{
    uint64_t cycles_per_sec = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    uint64_t now = ClockCycles();

    if (cycles_per_sec == 0u) {
        return 0;
    }

    return (now / cycles_per_sec) * 1000000ull
         + ((now % cycles_per_sec) * 1000000ull) / cycles_per_sec;
}

static void qnx_sleep_us(uint32_t us)
{
    /* OSAPI_Thread_sleep takes ms; OSAPI_Thread_nanosleep takes ns
     * (32-bit argument, max ~4.29 s). Split: sleep whole-ms portion
     * via OSAPI_Thread_sleep, then nanosleep the sub-ms remainder. */
    uint32_t ms     = us / 1000u;
    uint32_t rem_us = us % 1000u;

    if (ms > 0) {
        OSAPI_Thread_sleep((RTI_UINT32)ms);
    }
    if (rem_us > 0) {
        OSAPI_Thread_nanosleep((RTI_UINT32)(rem_us * 1000u));
    }
}

static PerftestLiteSemaphore *qnx_sem_new(void)
{
    return (PerftestLiteSemaphore *)OSAPI_Semaphore_new();
}

static int qnx_sem_take(PerftestLiteSemaphore *s, int32_t timeout_ms)
{
    RTI_INT32 reason = 0;
    /* OSAPI uses -1 for infinite wait, matching our convention. */
    RTI_BOOL ok = OSAPI_Semaphore_take((OSAPI_Semaphore_T *)s,
                                       (RTI_INT32)timeout_ms,
                                       &reason);
    return ok ? 1 : -(int)reason;
}

static int qnx_sem_give(PerftestLiteSemaphore *s)
{
    return OSAPI_Semaphore_give((OSAPI_Semaphore_T *)s) ? 1 : 0;
}

static void qnx_sem_delete(PerftestLiteSemaphore *s)
{
    if (!s) return;
    (void)OSAPI_Semaphore_delete((OSAPI_Semaphore_T *)s);
}

static uint32_t qnx_nic_address_ipv4(const char *name)
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

static uint32_t qnx_nic_netmask_ipv4(const char *name)
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

static const PerftestLiteOsInterface g_qnx_os_iface = {
    .time_now_us       = qnx_time_now_us,
    .sleep_us          = qnx_sleep_us,
    .semaphore_new     = qnx_sem_new,
    .semaphore_take    = qnx_sem_take,
    .semaphore_give    = qnx_sem_give,
    .semaphore_delete  = qnx_sem_delete,
    .nic_address_ipv4  = qnx_nic_address_ipv4,
    .nic_netmask_ipv4  = qnx_nic_netmask_ipv4
};

const PerftestLiteOsInterface *perftest_lite_os_get(void)
{
    return &g_qnx_os_iface;
}

#endif /* PERFTEST_LITE_OS_QNX */
