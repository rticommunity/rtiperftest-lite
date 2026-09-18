/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Human-readable table renderer.
 */
#include "perftest_lite_render.h"

#include <stdio.h>

static PerftestLiteRole g_role;

static void table_begin(void *user, PerftestLiteRole role,
                        const PerftestLiteInputArgs *args)
{
    (void)user; (void)args;
    g_role = role;
    PERFTEST_LITE_PRINT("\n=== Perftest Lite Final Results ===\n");
    if (role == PERFTEST_LITE_ROLE_PUBLISHER) {
        PERFTEST_LITE_PRINT("%12s %14s %14s %12s %12s %12s %12s %12s\n",
            "size(B)", "samples", "avgLat(us)", "min(us)", "max(us)",
            "p50(us)", "p90(us)", "p99(us)");
    } else {
        PERFTEST_LITE_PRINT("%12s %14s %12s %12s\n",
            "size(B)", "samples", "lost", "thr(Mbps)");
    }
}

static void table_row(void *user, const PerftestLiteSizeResult *s)
{
    (void)user;
    if (g_role == PERFTEST_LITE_ROLE_PUBLISHER) {
        PERFTEST_LITE_PRINT("%12d %14llu %14llu %12llu %12llu %12llu %12llu %12llu\n",
            s->size_bytes,
            (unsigned long long)s->total_samples,
            (unsigned long long)s->avg_latency_us,
            (unsigned long long)s->min_latency_us,
            (unsigned long long)s->max_latency_us,
            (unsigned long long)s->p50_latency_us,
            (unsigned long long)s->p90_latency_us,
            (unsigned long long)s->p99_latency_us);
    } else {
        PERFTEST_LITE_PRINT("%12d %14llu %12llu %12.2f\n",
            s->size_bytes,
            (unsigned long long)s->total_samples,
            (unsigned long long)s->lost_samples,
            s->avg_throughput_mbps);
    }
}

static void table_end(void *user)
{
    (void)user;
    PERFTEST_LITE_PRINT("===================================\n");
}

static const PerftestLiteResultRenderer g_table_renderer = {
    table_begin, table_row, table_end
};

const PerftestLiteResultRenderer *perftest_lite_renderer_table(void)
{
    return &g_table_renderer;
}
