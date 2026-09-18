/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Publisher engine.
 *
 * Sends samples via the abstract middleware writer. Uses a binary
 * semaphore (from the OS interface) for ping-pong synchronization in
 * latency mode. Supports multi-size single-execution: iterates over the
 * provided datalen list (or just args->datalen) and writes one
 * PerftestLiteSizeResult per size.
 */
#include "perftest_lite.h"
#include "../os/os_interface.h"
#include "../type/type_interface.h"
#include "../middleware/middleware_interface.h"

#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
#include "../middleware/memory_checkpoint.h"
#include "osapi/osapi_heap.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    /* Match counters */
    volatile int32_t matched_subs;
    volatile int32_t matched_pubs;
    volatile int32_t finished_remote;

    /* Ping-pong state */
    PerftestLiteSemaphore *latency_sem;
    const PerftestLiteOsInterface *os;
    const PerftestLiteTypeInterface *type_iface;

    /* Latency stats */
    volatile uint64_t lat_sum_us;
    volatile uint64_t lat_count;
    volatile uint64_t lat_min_us;
    volatile uint64_t lat_max_us;

    /* History for percentile computation (heap-allocated, capped at
     * PERFTEST_LITE_LATENCY_HISTORY_SIZE; NULL when disabled). */
    uint32_t *lat_history;
    uint32_t  lat_history_count;

    FILE *latency_log_fp;
    int   latency_stream;     /* 1 = emit [LAT] lines via PERFTEST_LITE_PRINT */
    uint64_t test_start_us;

    /* Last sequence number written in the most recent size. Used to
    * stamp FINALIZATION samples so the subscriber can
     * account for trailing loss (samples lost after its last received). */
    int32_t last_seq;
} PubCtx;

/* ---- Listener callbacks --------------------------------------------- */

static void pub_on_writer_matched(void *u, int32_t change)
{
    PubCtx *c = (PubCtx *)u;
    if (change > 0) c->matched_subs++; else if (change < 0) c->matched_subs--;
}

static void pub_on_reader_matched(void *u, int32_t change)
{
    PubCtx *c = (PubCtx *)u;
    if (change > 0) c->matched_pubs++; else if (change < 0) c->matched_pubs--;
}

typedef struct {
    PubCtx *ctx;
} TakeCb;

static void pub_on_pong_sample(void *user, PerftestSample *sample)
{
    PubCtx *c = (PubCtx *)user;
    PerftestTypeKind kind = c->type_iface->get_kind(sample);
    if (kind == PERFTEST_TYPE_KIND_INITIALIZATION) {
        /* Discard — matches rti-perftest's LatencyListener which returns
         * immediately for INITIALIZE_SIZE without signalling the semaphore.
         * The init burst is fire-and-forget; no drain loop is needed. */
        return;
    }
    if (kind == PERFTEST_TYPE_KIND_FINALIZATION) {
        c->finished_remote = 1;
        return;
    }
    /* Round-trip latency divided by 2. */
    uint64_t now = c->os->time_now_us();
    uint64_t ts  = c->type_iface->get_timestamp_us(sample);

    uint64_t latency = (now > ts) ? (now - ts) / 2 : 0;
    c->lat_sum_us  += latency;
    c->lat_count   += 1;
    if (latency > c->lat_max_us) c->lat_max_us = latency;
    if (latency < c->lat_min_us) c->lat_min_us = latency;
#if PERFTEST_LITE_LATENCY_HISTORY_SIZE > 0
    if (c->lat_history && c->lat_history_count < PERFTEST_LITE_LATENCY_HISTORY_SIZE) {
        uint32_t v = latency > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)latency;
        c->lat_history[c->lat_history_count++] = v;
    }
#endif
    if (c->latency_log_fp || c->latency_stream) {
        uint64_t elapsed_us_since_test_start =
            (now > c->test_start_us) ? (now - c->test_start_us) : 0;
        if (c->latency_log_fp) {
            fprintf(c->latency_log_fp, "%llu,%llu\n",
                    (unsigned long long)elapsed_us_since_test_start,
                    (unsigned long long)latency);
        }
        if (c->latency_stream) {
            /* Use %u / unsigned-int casts: on ARM 32-bit (AAPCS), a bare
             * unsigned-long-long first arg is placed in r2:r3 (aligned pair),
             * leaving r1 unused.  A printf that reads %llu as 4 bytes picks
             * up r1=0 rather than r2=latency.  %u forces 32-bit (r1, r2),
             * which is sufficient for latency (< 4 s) and elapsed (< 71 min). */
            PERFTEST_LITE_PRINT("[LAT] %u,%u\n",
                    (unsigned int)elapsed_us_since_test_start,
                    (unsigned int)latency);
        }
    }
    c->os->semaphore_give(c->latency_sem);
}

static void pub_on_data_available(void *user, PerftestLiteReader *reader)
{
    PubCtx *c = (PubCtx *)user;
    reader->take(reader, pub_on_pong_sample, c);
}

/* ---- qsort comparator for uint32_t latency history ------------------- */

#if PERFTEST_LITE_LATENCY_HISTORY_SIZE > 0
static int cmp_uint32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a;
    uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}
#endif

/* ---- Per-size measurement ------------------------------------------- */

static void run_one_size(PubCtx *ctx,
                         const PerftestLiteInputArgs *args,
                         PerftestLiteWriter *writer,
                         PerftestSample *sample,
                         int32_t size_bytes,
                         PerftestLiteSizeResult *result_out)
{
    const PerftestLiteOsInterface *os = ctx->os;
    const PerftestLiteTypeInterface *T = ctx->type_iface;
    int32_t init_sample_size = PERFTEST_LITE_INIT_SAMPLE_SIZE;

    /* Warm-up burst — matches rti-perftest exactly:
    *  1. Set the reported sample size to PERFTEST_LITE_INIT_SAMPLE_SIZE
    *     (1234 B, matching rti-perftest's INITIALIZE_SIZE). Fixed-size and
    *     smaller bounded types retain the already-validated test size.
     *  2. Fire PERFTEST_LITE_INIT_BURST samples back-to-back, no waiting.
     *  3. Sleep 1 s (matches rti-perftest's milliSleep(1000)).
     * The subscriber echoes INITIALIZATION samples back, but the pong
     * callback discards them without touching the semaphore — no drain
     * loop needed.  Restore the test payload after the sleep. */
    T->set_kind(sample, PERFTEST_TYPE_KIND_INITIALIZATION);
    if (T->set_sample_size(sample, init_sample_size) != 0) {
        T->set_sample_size(sample, size_bytes);
    }
    for (int i = 0; i < PERFTEST_LITE_INIT_BURST; ++i) {
        T->set_timestamp_us(sample, os->time_now_us());
        writer->write(writer, sample);
    }
    os->sleep_us(1000000); /* 1 s settle — matches rti-perftest milliSleep(1000) */
    T->set_sample_size(sample, size_bytes); /* restore test sample size */

    /* Reset per-size stats only after the warm-up is complete. */
    ctx->lat_sum_us = 0; ctx->lat_count = 0;
    ctx->lat_min_us = (uint64_t)-1; ctx->lat_max_us = 0;
    ctx->lat_history_count = 0;

    PERFTEST_LITE_PRINT("[pub] starting size=%d bytes for %d s\n",
                        size_bytes, args->exec_seconds);
    if (args->print_every_second) {
        PERFTEST_LITE_PRINT("%6s %12s %14s %12s %12s\n",
            "t(s)", "size(B)", "totalSamples",
            "avgLat(us)", "min/max(us)");
    }

    uint64_t t0 = os->time_now_us();
    ctx->test_start_us = t0;
    uint64_t t_next_print = t0 + 1000000ull;
    uint64_t timeout_us = (uint64_t)args->exec_seconds * 1000000ull;
    uint64_t total = 0;
    int32_t  seq = 0;
    /* sleep_us = inter-sample sleep. With -pubrate it's the rate gap;
     * otherwise fall back to PERFTEST_LITE_PUB_MIN_SLEEP_US (non-zero on
     * embedded targets so we don't starve the network stack). */
    int      sleep_us = PERFTEST_LITE_PUB_MIN_SLEEP_US;
    if (args->pubrate > 0) sleep_us = 1000000 / args->pubrate;

    while ((os->time_now_us() - t0) < timeout_us) {
        seq++;
        T->set_sequence_number(sample, seq);
        T->set_timestamp_us(sample, os->time_now_us());

        int is_ping = args->latency_test ||
                      (args->latency_count > 0 && (seq % args->latency_count) == 0);
        T->set_kind(sample, is_ping
                            ? PERFTEST_TYPE_KIND_DATA_PING
                            : PERFTEST_TYPE_KIND_DATA);

        int write_succeeded = writer->write(writer, sample) == 0;
        if (write_succeeded) {
            total++;
        }

        /* In throughput mode periodic pings provide latency telemetry but
         * must not serialize the data stream by waiting for their pongs.
         * Latency-test mode deliberately sends only ping samples, so it
         * retains the ping-pong wait. */
        if (write_succeeded && is_ping && args->latency_test) {
            os->semaphore_take(ctx->latency_sem,
                args->reliable ? -1 : 200);
        }

        if (sleep_us > 0) {
            os->sleep_us(sleep_us);
        }

        uint64_t now = os->time_now_us();
        if (args->print_every_second && now >= t_next_print) {
            t_next_print = now + 1000000ull;
            uint64_t avg = ctx->lat_count
                ? ctx->lat_sum_us / ctx->lat_count : 0;
            uint64_t mn  = ctx->lat_min_us == (uint64_t)-1 ? 0 : ctx->lat_min_us;
            char range[32];
            snprintf(range, sizeof(range), "%llu/%llu",
                     (unsigned long long)mn,
                     (unsigned long long)ctx->lat_max_us);
            PERFTEST_LITE_PRINT("%6llu %12d %14llu %12llu %12s\n",
                (unsigned long long)((now - t0) / 1000000ull),
                size_bytes,
                (unsigned long long)total,
                (unsigned long long)avg,
                range);
        }
    }

    /* Compute percentiles from sorted history */
#if PERFTEST_LITE_LATENCY_HISTORY_SIZE > 0
    if (ctx->lat_history && ctx->lat_history_count > 0) {
        qsort(ctx->lat_history, ctx->lat_history_count,
              sizeof(uint32_t), cmp_uint32);
        uint32_t n = ctx->lat_history_count;
        result_out->p50_latency_us = ctx->lat_history[n * 50u / 100u];
        result_out->p90_latency_us = ctx->lat_history[n * 90u / 100u];
        result_out->p99_latency_us = ctx->lat_history[n * 99u / 100u];
    } else {
        result_out->p50_latency_us = 0;
        result_out->p90_latency_us = 0;
        result_out->p99_latency_us = 0;
    }
#endif
    /* Fill in results for this size */
    uint64_t elapsed_us = os->time_now_us() - t0;
    ctx->last_seq = seq;
    result_out->size_bytes               = size_bytes;
    result_out->total_samples            = total;
    result_out->latency_samples_received = ctx->lat_count;
    result_out->avg_latency_us           = ctx->lat_count
        ? ctx->lat_sum_us / ctx->lat_count : 0;
    result_out->min_latency_us           =
        ctx->lat_min_us == (uint64_t)-1 ? 0 : ctx->lat_min_us;
    result_out->max_latency_us           = ctx->lat_max_us;
    /* Publisher does not measure throughput; subscriber will. */
    result_out->avg_throughput_mbps      = 0.0;
    result_out->lost_samples             = 0;
    result_out->elapsed_us               = elapsed_us;
}

/* ---- Entry point ---------------------------------------------------- */

PerftestLiteRetcode perftest_lite_publisher(const PerftestLiteInputArgs *args,
                                            PerftestLiteResults *results)
{
    const PerftestLiteOsInterface  *os = perftest_lite_os_get();
    const PerftestLiteTypeInterface *T = perftest_lite_type_get();
    PerftestLiteMiddleware *mw         = perftest_lite_middleware_get();
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    uint64_t process_start_heap_bytes =
        perftest_lite_memory_get_osapi_heap_bytes();
#endif

    PubCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.os = os; ctx.type_iface = T;
    ctx.lat_min_us = (uint64_t)-1;
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    perftest_lite_memory_emit_checkpoint(
        "process_start", "application", process_start_heap_bytes);
#endif

    PerftestLiteMiddlewareConfig cfg = {
        .type_iface = T,
        .domain_id = args->domain_id,
        .participant_name = PERFTEST_LITE_PARTICIPANT_NAME_PUB,
        .participant_id   = PERFTEST_LITE_PARTICIPANT_ID_PUB,
        .peer = args->peer, .nic = args->nic,
        .transport_id = args->transport_id,
        .reliable = args->reliable, .is_publisher = 1,
        .latency_test = args->latency_test,
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
        .process_start_heap_bytes = process_start_heap_bytes,
#endif
        .remote_participant_name = PERFTEST_LITE_PARTICIPANT_NAME_SUB
    };
    if (mw->init(mw, &cfg) != 0) {
        return PERFTEST_LITE_ERR_MIDDLEWARE;
    }

    PerftestLiteListener writer_listener = {0};
    writer_listener.user_data = &ctx;
    writer_listener.on_publication_matched = pub_on_writer_matched;
    /* Note: writer matched is reported via on_subscription_matched on the
     * data writer side in DDS, but our listener struct merges them. We
     * register the matched count via the reader-side wiring below; for
     * now we treat any matched event as enough to start. */

    PerftestLiteWriter *ping_w = mw->create_writer(mw,
        PERFTEST_LITE_PING_TOPIC_NAME, args->reliable);

    PerftestLiteListener reader_listener = {0};
    reader_listener.user_data = &ctx;
    reader_listener.on_subscription_matched = pub_on_reader_matched;
    reader_listener.on_data_available       = pub_on_data_available;
    PerftestLiteReader *pong_r = mw->create_reader(mw,
        PERFTEST_LITE_PONG_TOPIC_NAME, args->reliable, &reader_listener);
    (void)writer_listener;

    if (!ping_w || !pong_r) {
        mw->shutdown(mw);
        return PERFTEST_LITE_ERR_MIDDLEWARE;
    }

#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    /* The endpoint-ready checkpoint intentionally precedes publisher-only
     * statistics and semaphore allocations, so middleware pool capacity is
     * not reported as application memory (or vice versa). */
    perftest_lite_memory_emit_checkpoint(
        "endpoints_ready", "middleware", process_start_heap_bytes);
#endif

    ctx.latency_sem = os->semaphore_new();
    if (!ctx.latency_sem) {
        mw->shutdown(mw);
        return PERFTEST_LITE_ERR_OS;
    }
#if PERFTEST_LITE_LATENCY_HISTORY_SIZE > 0
    ctx.lat_history = (uint32_t *)OSAPI_Heap_allocate(
        PERFTEST_LITE_LATENCY_HISTORY_SIZE, sizeof(*ctx.lat_history));
    /* NULL is tolerated — percentiles will be unavailable but the test runs. */
#endif

    PerftestSample *sample = T->create();
    if (!sample) {
        mw->shutdown(mw);
        os->semaphore_delete(ctx.latency_sem);
#if PERFTEST_LITE_LATENCY_HISTORY_SIZE > 0
        if (ctx.lat_history) OSAPI_Heap_free(ctx.lat_history);
#endif
        return PERFTEST_LITE_ERR_TYPE;
    }
    T->initialize(sample);
#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    perftest_lite_memory_emit_checkpoint(
        "application_ready", "application", process_start_heap_bytes);
#endif

    PERFTEST_LITE_PRINT("[pub] waiting for a perftest subscriber...\n");
    while (ctx.matched_pubs < 1) {
        os->sleep_us(50000);
    }
    PERFTEST_LITE_PRINT("[pub] matched.\n");

    ctx.latency_stream = args->latency_stream;
    if (ctx.latency_stream) {
        PERFTEST_LITE_PRINT("[LAT] elapsed_us,latency_us\n");
    }
    if (args->latency_log_file) {
        ctx.latency_log_fp = fopen(args->latency_log_file, "w");
        if (!ctx.latency_log_fp) {
            mw->shutdown(mw);
            os->semaphore_delete(ctx.latency_sem);
            return PERFTEST_LITE_ERR_OS;
        }
        fprintf(ctx.latency_log_fp, "elapsed_us,latency_us\n");
    }

    PerftestLiteSizeResult tmp;
    memset(&tmp, 0, sizeof(tmp));
    run_one_size(&ctx, args, ping_w, sample, args->datalen, &tmp);
    if (results && results->sizes
        && results->size_count < results->size_capacity) {
        results->sizes[results->size_count++] = tmp;
    }
    PERFTEST_LITE_PRINT(
        "[pub] size=%d done: total=%llu avgLat=%llu us "
        "min=%llu p50=%llu p90=%llu p99=%llu max=%llu\n",
        tmp.size_bytes,
        (unsigned long long)tmp.total_samples,
        (unsigned long long)tmp.avg_latency_us,
        (unsigned long long)tmp.min_latency_us,
        (unsigned long long)tmp.p50_latency_us,
        (unsigned long long)tmp.p90_latency_us,
        (unsigned long long)tmp.p99_latency_us,
        (unsigned long long)tmp.max_latency_us);

    /* Send finalization. Carry the last data seq so the subscriber can
     * account for trailing loss in the final size. */
    T->set_kind(sample, PERFTEST_TYPE_KIND_FINALIZATION);
    T->set_sequence_number(sample, ctx.last_seq);
    for (int k = 0; k < 10 && !ctx.finished_remote; ++k) {
        ping_w->write(ping_w, sample);
        os->sleep_us(50000);
    }

    T->destroy(sample);
    mw->shutdown(mw);
    if (ctx.latency_log_fp) {
        fclose(ctx.latency_log_fp);
    }
    os->semaphore_delete(ctx.latency_sem);
#if PERFTEST_LITE_LATENCY_HISTORY_SIZE > 0
    if (ctx.lat_history) {
        OSAPI_Heap_free(ctx.lat_history);
    }
#endif
    return PERFTEST_LITE_OK;
}
