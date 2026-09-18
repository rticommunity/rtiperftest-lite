/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Perftest Lite - public API.
 *
 * Two entry points exist (per the design):
 *   - perftest_lite_main_cli(argc, argv): for platforms with a CLI.
 *   - perftest_lite_main(InputArgs*, Results*): for embedded platforms
 *     where InputArgs is built explicitly from main().
 *
 * Plus two role-specific entry points:
 *   - perftest_lite_publisher(InputArgs*, Results*)
 *   - perftest_lite_subscriber(InputArgs*, Results*)
 *
 * No globals are required by the engine (CLI wrappers are separate).
 */
#ifndef PERFTEST_LITE_H
#define PERFTEST_LITE_H

#include <stdint.h>
#include "perftest_lite_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PERFTEST_LITE_ROLE_PUBLISHER  = 0,
    PERFTEST_LITE_ROLE_SUBSCRIBER = 1
} PerftestLiteRole;

typedef enum {
    PERFTEST_LITE_OK             =  0,
    PERFTEST_LITE_ERR_BAD_ARGS   = -1,
    PERFTEST_LITE_ERR_OS         = -2,
    PERFTEST_LITE_ERR_MIDDLEWARE = -3,
    PERFTEST_LITE_ERR_TYPE       = -4,
    PERFTEST_LITE_ERR_TIMEOUT    = -5,
    PERFTEST_LITE_ERR_INTERNAL   = -6
} PerftestLiteRetcode;

/* Standardized parse return values for perftest_lite_parse_arguments(). */
#define PERFTEST_LITE_PARSE_OK      0
#define PERFTEST_LITE_PARSE_HELP    1
#define PERFTEST_LITE_PARSE_ERROR  -1

/* Standardized process exit codes for CLI entry points. */
#define PERFTEST_LITE_EXIT_SUCCESS  0
#define PERFTEST_LITE_EXIT_FAILURE  1

/*
 * InputArgs - immutable test settings, built either by the CLI parser or
 * directly by main() on embedded platforms.
 */
typedef struct PerftestLiteInputArgs {
    PerftestLiteRole role;
    int32_t          domain_id;
    const char      *nic;            /* NULL -> loopback */
    const char      *peer;           /* NULL -> 127.0.0.1 */
    const char      *transport_id;   /* "UDPv4" (default), "ZeroCopy"... */
    const char      *latency_log_file;
    int              latency_stream;     /* 0/1: emit [LAT] lines via PERFTEST_LITE_PRINT */

    int32_t          datalen;        /* default size in bytes */
    int32_t          exec_seconds;   /* execution time                     */
    int32_t          pubrate;        /* samples/sec, -1 unlimited          */
    int32_t          latency_count;  /* ping every N samples               */
    int32_t          sub_sleep_us;   /* subscriber loop sleep              */

    int              reliable;       /* 0/1                                */
    int              latency_test;   /* 0/1                                */
    int              print_every_second;
    int              print_config;
    int              print_resource_limits;
} PerftestLiteInputArgs;

void perftest_lite_input_args_init(PerftestLiteInputArgs *args);
void perftest_lite_free_args(PerftestLiteInputArgs *args);

/* The measurement engine writes results here so the application (CLI,
 * telemetry sink, etc.) decides how to render them. */
typedef struct {
    int32_t  size_bytes;
    uint64_t total_samples;
    uint64_t latency_samples_received;
    uint64_t avg_latency_us;
    uint64_t min_latency_us;
    uint64_t max_latency_us;
    uint64_t p50_latency_us;
    uint64_t p90_latency_us;
    uint64_t p99_latency_us;
    double   avg_throughput_mbps;
    uint64_t lost_samples;
    uint64_t elapsed_us;
} PerftestLiteSizeResult;

typedef struct {
    int32_t                size_count;
    int32_t                size_capacity;
    PerftestLiteSizeResult *sizes; /* may be NULL: engine still works */
} PerftestLiteResults;

void perftest_lite_results_init(PerftestLiteResults *results,
                                PerftestLiteSizeResult *storage,
                                int32_t capacity);

/* ---- Entry points ----------------------------------------------------- */

/* Build an InputArgs from CLI tokens. Returns 0 on success, <0 on error,
 * or >0 if the user requested help (caller should exit 0). */
int perftest_lite_parse_arguments(int argc, char **argv,
                                  PerftestLiteInputArgs *args);

void perftest_lite_print_help(const char *appname);

void perftest_lite_print_input_args(const PerftestLiteInputArgs *args);

PerftestLiteRetcode perftest_lite_main(const PerftestLiteInputArgs *args,
                                       PerftestLiteResults *results);

PerftestLiteRetcode perftest_lite_publisher(const PerftestLiteInputArgs *args,
                                            PerftestLiteResults *results);

PerftestLiteRetcode perftest_lite_subscriber(const PerftestLiteInputArgs *args,
                                             PerftestLiteResults *results);

/* CLI convenience: parses argv, calls perftest_lite_main, prints results. */
int perftest_lite_main_cli(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* PERFTEST_LITE_H */
