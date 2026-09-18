/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Subscriber engine.
 *
 * Receives samples on the ping topic, echoes back PING samples on the
 * pong topic, computes throughput per second, and stops when a
 * FINALIZATION sample arrives.
 */
#include "perftest_lite.h"
#include "../os/os_interface.h"
#include "../type/type_interface.h"
#include "../middleware/middleware_interface.h"

#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
#include "../middleware/memory_checkpoint.h"
#endif

#include <stdio.h>
#include <string.h>

typedef struct {
    const PerftestLiteOsInterface  *os;
    const PerftestLiteTypeInterface *T;
    PerftestLiteWriter             *pong_w;

    /* Per-test state (reset between sizes when keep-alive). */
    volatile uint64_t total_received;
    volatile int32_t  current_size_bytes;
    volatile int32_t  last_seq;
    volatile uint64_t lost_samples;
    volatile uint64_t test_start_us;
    volatile double   avg_throughput_mbps;
    volatile int32_t  finished;

    int32_t           matched_pubs;
} SubCtx;

static void sub_on_reader_matched(void *u, int32_t change)
{
    SubCtx *c = (SubCtx *)u;
    if (change > 0) c->matched_pubs++; else if (change < 0) c->matched_pubs--;
}

static void sub_on_sample(void *user, PerftestSample *sample)
{
    SubCtx *c = (SubCtx *)user;
    const PerftestLiteTypeInterface *T = c->T;
    PerftestTypeKind kind = T->get_kind(sample);
    int32_t size = T->get_sample_size(sample);

    if (kind == PERFTEST_TYPE_KIND_FINALIZATION) {
        if (!c->finished) {
            /* Trailing-loss accounting: publisher stamps the final data
             * seq it sent on the FINALIZATION message. */
            int32_t exp = T->get_sequence_number(sample);
            if (exp > c->last_seq) {
                c->lost_samples += (uint64_t)(exp - c->last_seq);
            }
            c->finished = 1;
        }
        /* Echo back so publisher knows we got it. */
        c->pong_w->write(c->pong_w, sample);
        return;
    }
    if (kind == PERFTEST_TYPE_KIND_INITIALIZATION) {
        /* Echo back so the publisher can drain and warm the pong path
         * (matches rti-perftest subscriber behaviour). */
        c->pong_w->write(c->pong_w, sample);
        return;
    }

    /* DATA / DATA_PING */
    int32_t seq = T->get_sequence_number(sample);
    if (c->last_seq != 0 && seq > c->last_seq + 1) {
        c->lost_samples += (seq - c->last_seq - 1);
    }
    c->last_seq = seq;
    c->total_received += 1;
    c->current_size_bytes = size;
    if (c->test_start_us == 0) {
        c->test_start_us = c->os->time_now_us();
    }

    if (kind == PERFTEST_TYPE_KIND_DATA_PING) {
        /* Echo back. We pass the same sample (the middleware copies). */
        c->pong_w->write(c->pong_w, sample);
    }
}

static void sub_on_data_available(void *user, PerftestLiteReader *reader)
{
    reader->take(reader, sub_on_sample, user);
}

PerftestLiteRetcode perftest_lite_subscriber(const PerftestLiteInputArgs *args,
                                             PerftestLiteResults *results)
{
    const PerftestLiteOsInterface  *os = perftest_lite_os_get();
    const PerftestLiteTypeInterface *T = perftest_lite_type_get();
    PerftestLiteMiddleware *mw         = perftest_lite_middleware_get();
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    uint64_t process_start_heap_bytes =
        perftest_lite_memory_get_osapi_heap_bytes();
#endif

    SubCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.os = os; ctx.T = T;
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    perftest_lite_memory_emit_checkpoint(
        "process_start", "application", process_start_heap_bytes);
#endif

    PerftestLiteMiddlewareConfig cfg = {
        .type_iface = T,
        .domain_id = args->domain_id,
        .participant_name = PERFTEST_LITE_PARTICIPANT_NAME_SUB,
        .participant_id   = PERFTEST_LITE_PARTICIPANT_ID_SUB,
        .peer = args->peer, .nic = args->nic,
        .transport_id = args->transport_id,
        .reliable = args->reliable, .is_publisher = 0,
        .latency_test = args->latency_test,
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
        .process_start_heap_bytes = process_start_heap_bytes,
#endif
        .remote_participant_name = PERFTEST_LITE_PARTICIPANT_NAME_PUB
    };
    if (mw->init(mw, &cfg) != 0) return PERFTEST_LITE_ERR_MIDDLEWARE;

    /* Create the pong writer first so it is ready when the listener fires.
     * Use the selected reliability mode for both the forward and return paths. */
    ctx.pong_w = mw->create_writer(mw, PERFTEST_LITE_PONG_TOPIC_NAME,
                                   args->reliable);

    PerftestLiteListener listener = {0};
    listener.user_data = &ctx;
    listener.on_subscription_matched = sub_on_reader_matched;
    listener.on_data_available       = sub_on_data_available;
    PerftestLiteReader *ping_r = mw->create_reader(mw,
        PERFTEST_LITE_PING_TOPIC_NAME, args->reliable, &listener);

    if (!ctx.pong_w || !ping_r) {
        mw->shutdown(mw);
        return PERFTEST_LITE_ERR_MIDDLEWARE;
    }

#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    perftest_lite_memory_emit_checkpoint(
        "endpoints_ready", "middleware", process_start_heap_bytes);
    perftest_lite_memory_emit_checkpoint(
        "application_ready", "application", process_start_heap_bytes);
#endif

    PERFTEST_LITE_PRINT("[sub] waiting for a perftest publisher...\n");
    while (ctx.matched_pubs < 1) os->sleep_us(50000);
    PERFTEST_LITE_PRINT("[sub] matched.\n");

    {
        ctx.total_received = 0; ctx.last_seq = 0;
        ctx.lost_samples = 0; ctx.test_start_us = 0;
        ctx.avg_throughput_mbps = 0.0;
        ctx.finished = 0;

        if (args->print_every_second) {
            PERFTEST_LITE_PRINT("%6s %12s %12s %12s %12s\n",
                "t(s)", "size(B)", "samples/s", "Mbps", "lost");
        }

        uint64_t t_last_print = os->time_now_us();
        uint64_t last_count = 0;

        while (!ctx.finished) {
            os->sleep_us((uint32_t)args->sub_sleep_us);
            uint64_t now = os->time_now_us();
            if (args->print_every_second && now - t_last_print >= 1000000ull) {
                uint64_t delta = ctx.total_received - last_count;
                last_count = ctx.total_received;
                uint64_t elapsed_us = now - t_last_print;
                t_last_print = now;
                double sps  = (double)delta * 1000000.0 / (double)elapsed_us;
                double mbps = sps * (double)ctx.current_size_bytes * 8.0 / 1.0e6;
                ctx.avg_throughput_mbps = mbps;
                PERFTEST_LITE_PRINT("%6llu %12d %12.1f %12.1f %12llu\n",
                    (unsigned long long)((now - (ctx.test_start_us
                        ? ctx.test_start_us : now)) / 1000000ull),
                    ctx.current_size_bytes, sps, mbps,
                    (unsigned long long)ctx.lost_samples);
            }
        }

        /* Capture result */
        if (results && results->sizes
            && results->size_count < results->size_capacity) {
            uint64_t elapsed = ctx.test_start_us
                ? (os->time_now_us() - ctx.test_start_us) : 0;
            double final_mbps = 0.0;
            if (elapsed > 0 && ctx.total_received > 0) {
                /* bytes*8 / us == Mbps directly */
                final_mbps = (double)ctx.total_received
                           * (double)ctx.current_size_bytes * 8.0
                           / (double)elapsed;
            }
            ctx.avg_throughput_mbps = final_mbps;
            PerftestLiteSizeResult *r = &results->sizes[results->size_count++];
            r->size_bytes = ctx.current_size_bytes;
            r->total_samples = ctx.total_received;
            r->avg_latency_us = 0;
            r->min_latency_us = 0;
            r->max_latency_us = 0;
            r->avg_throughput_mbps = final_mbps;
            r->lost_samples = ctx.lost_samples;
            r->elapsed_us = elapsed;
        }

        PERFTEST_LITE_PRINT("[sub] size=%d done: total=%llu lost=%llu thr=%.2f Mbps\n",
            ctx.current_size_bytes,
            (unsigned long long)ctx.total_received,
            (unsigned long long)ctx.lost_samples,
            ctx.avg_throughput_mbps);

    }

    mw->shutdown(mw);
    return PERFTEST_LITE_OK;
}
