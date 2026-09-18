/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

#include "os_interface.h"

#include <stdint.h>

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "osapi/osapi_semaphore.h"
#include "osapi/osapi_thread.h"
#include "platform_config.h"

#if PERFTEST_LITE_OS_INTERFACE_VERSION != 2
#error "Update the FreeRTOS/lwIP adapter for the new OS interface version"
#endif

static uint64_t freertos_lwip_time_now_us(void)
{
    return perftest_lite_platform_time_now_us();
}

static void freertos_lwip_sleep_us(uint32_t microseconds)
{
    uint32_t milliseconds = microseconds / 1000u;
    uint32_t remaining_microseconds = microseconds % 1000u;

    if (milliseconds > 0u) {
        OSAPI_Thread_sleep((RTI_UINT32)milliseconds);
    }
    if (remaining_microseconds > 0u) {
        OSAPI_Thread_nanosleep(
                (RTI_UINT32)(remaining_microseconds * 1000u));
    }
}

static PerftestLiteSemaphore *freertos_lwip_semaphore_new(void)
{
    return (PerftestLiteSemaphore *)OSAPI_Semaphore_new();
}

static int freertos_lwip_semaphore_take(
        PerftestLiteSemaphore *semaphore,
        int32_t timeout_ms)
{
    RTI_INT32 reason = 0;
    RTI_BOOL result = OSAPI_Semaphore_take(
            (OSAPI_Semaphore_T *)semaphore,
            (RTI_INT32)timeout_ms,
            &reason);

    return result ? 1 : -(int)reason;
}

static int freertos_lwip_semaphore_give(PerftestLiteSemaphore *semaphore)
{
    return OSAPI_Semaphore_give((OSAPI_Semaphore_T *)semaphore) ? 1 : 0;
}

static void freertos_lwip_semaphore_delete(PerftestLiteSemaphore *semaphore)
{
    if (semaphore != NULL) {
        (void)OSAPI_Semaphore_delete((OSAPI_Semaphore_T *)semaphore);
    }
}

static uint32_t freertos_lwip_nic_address_ipv4(const char *interface_name)
{
    struct netif *network_interface;

    if (interface_name == NULL) {
        return UINT32_MAX;
    }

    network_interface = netif_find(interface_name);
    if (network_interface == NULL || !netif_is_up(network_interface)) {
        return UINT32_MAX;
    }

    return ip4_addr_get_u32(netif_ip4_addr(network_interface));
}

static uint32_t freertos_lwip_nic_netmask_ipv4(const char *interface_name)
{
    struct netif *network_interface;

    if (interface_name == NULL) {
        return UINT32_MAX;
    }

    network_interface = netif_find(interface_name);
    if (network_interface == NULL || !netif_is_up(network_interface)) {
        return UINT32_MAX;
    }

    return ip4_addr_get_u32(netif_ip4_netmask(network_interface));
}

static const PerftestLiteOsInterface freertos_lwip_os = {
    .time_now_us = freertos_lwip_time_now_us,
    .sleep_us = freertos_lwip_sleep_us,
    .semaphore_new = freertos_lwip_semaphore_new,
    .semaphore_take = freertos_lwip_semaphore_take,
    .semaphore_give = freertos_lwip_semaphore_give,
    .semaphore_delete = freertos_lwip_semaphore_delete,
    .nic_address_ipv4 = freertos_lwip_nic_address_ipv4,
    .nic_netmask_ipv4 = freertos_lwip_nic_netmask_ipv4
};

const PerftestLiteOsInterface *perftest_lite_os_get(void)
{
    return &freertos_lwip_os;
}
