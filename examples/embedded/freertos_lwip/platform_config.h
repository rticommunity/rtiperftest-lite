/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

#ifndef PERFTEST_LITE_EXAMPLE_PLATFORM_CONFIG_H
#define PERFTEST_LITE_EXAMPLE_PLATFORM_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Implement these two hooks in the board support package. The clock must be
 * monotonic and return accumulated microseconds. */
uint64_t perftest_lite_platform_time_now_us(void);
int perftest_lite_platform_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#define PERFTEST_LITE_PRINT(...) \
    perftest_lite_platform_printf(__VA_ARGS__)

#ifndef PERFTEST_LITE_DEFAULT_NIC
#define PERFTEST_LITE_DEFAULT_NIC "st0"
#endif

#ifndef PERFTEST_LITE_DEFAULT_PEER
#define PERFTEST_LITE_DEFAULT_PEER "192.0.2.20"
#endif

#ifndef PERFTEST_LITE_DEFAULT_DOMAIN_ID
#define PERFTEST_LITE_DEFAULT_DOMAIN_ID 0
#endif

#ifndef PERFTEST_LITE_DEFAULT_DATALEN
#define PERFTEST_LITE_DEFAULT_DATALEN 256
#endif

#ifndef PERFTEST_LITE_DEFAULT_EXEC_SECONDS
#define PERFTEST_LITE_DEFAULT_EXEC_SECONDS 10
#endif

/* Yield in an unlimited-rate publisher so the lwIP thread can drain its
 * mailbox. Tune this together with the target scheduler tick. */
#ifndef PERFTEST_LITE_PUB_MIN_SLEEP_US
#define PERFTEST_LITE_PUB_MIN_SLEEP_US 1
#endif

#endif /* PERFTEST_LITE_EXAMPLE_PLATFORM_CONFIG_H */
