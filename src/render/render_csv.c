/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * CSV renderer (machine-readable). One header line + one row per size.
 */
#include "perftest_lite_render.h"

#include <stdio.h>

static PerftestLiteRole g_role;

static void csv_begin(void *user, PerftestLiteRole role,
                      const PerftestLiteInputArgs *args)
{
    (void)user; (void)args;
    g_role = role;
    if (role == PERFTEST_LITE_ROLE_PUBLISHER) {
        PERFTEST_LITE_PRINT(
            "size_bytes,total_samples,avg_latency_us,min_latency_us,"
            "max_latency_us,p50_latency_us,p90_latency_us,p99_latency_us,"
            "elapsed_us\n");
    } else {
        PERFTEST_LITE_PRINT(
            "size_bytes,total_samples,lost_samples,avg_throughput_mbps,"
            "elapsed_us\n");
    }
}

static void csv_row(void *user, const PerftestLiteSizeResult *s)
{
    (void)user;
    if (g_role == PERFTEST_LITE_ROLE_PUBLISHER) {
        PERFTEST_LITE_PRINT("%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
            s->size_bytes,
            (unsigned long long)s->total_samples,
            (unsigned long long)s->avg_latency_us,
            (unsigned long long)s->min_latency_us,
            (unsigned long long)s->max_latency_us,
            (unsigned long long)s->p50_latency_us,
            (unsigned long long)s->p90_latency_us,
            (unsigned long long)s->p99_latency_us,
            (unsigned long long)s->elapsed_us);
    } else {
        PERFTEST_LITE_PRINT("%d,%llu,%llu,%.6f,%llu\n",
            s->size_bytes,
            (unsigned long long)s->total_samples,
            (unsigned long long)s->lost_samples,
            s->avg_throughput_mbps,
            (unsigned long long)s->elapsed_us);
    }
}

static void csv_end(void *user) { (void)user; }

static const PerftestLiteResultRenderer g_csv_renderer = {
    csv_begin, csv_row, csv_end
};

const PerftestLiteResultRenderer *perftest_lite_renderer_csv(void)
{
    return &g_csv_renderer;
}
