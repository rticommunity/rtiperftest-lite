/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * InputArgs / Results lifecycle helpers + CLI parser + help text.
 */
#include "perftest_lite.h"
#include "../type/type_interface.h"
#if defined(PERFTEST_LITE_HAS_MICRO_ZEROCOPY) && defined(PERFTEST_LITE_TYPE_ZCOPY)
#  define PERFTEST_LITE_TRANSPORT_LIST "UDPv4, ZeroCopy"
#elif defined(PERFTEST_LITE_HAS_MICRO_SHMEM) && !defined(PERFTEST_LITE_TYPE_ZCOPY)
#  define PERFTEST_LITE_TRANSPORT_LIST "UDPv4, SHMEM"
#else
#  define PERFTEST_LITE_TRANSPORT_LIST "UDPv4"
#endif

#if defined(PERFTEST_LITE_BUILD_PUB) && PERFTEST_LITE_BUILD_PUB
static const PerftestLiteRole perftest_lite_default_role =
    PERFTEST_LITE_ROLE_PUBLISHER;
#elif defined(PERFTEST_LITE_BUILD_SUB) && PERFTEST_LITE_BUILD_SUB
static const PerftestLiteRole perftest_lite_default_role =
    PERFTEST_LITE_ROLE_SUBSCRIBER;
#else
#  error "A Perftest Lite publisher or subscriber role must be built."
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

static int parse_int32_option(const char *opt, const char *text,
                              int32_t min_value, int32_t max_value,
                              int32_t *result)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0'
        || value < min_value || value > max_value) {
        PERFTEST_LITE_PRINT(
            "invalid value for %s: '%s' (expected %d..%d)\n",
            opt, text, min_value, max_value);
        return PERFTEST_LITE_PARSE_ERROR;
    }

    *result = (int32_t)value;
    return PERFTEST_LITE_PARSE_OK;
}

void perftest_lite_input_args_init(PerftestLiteInputArgs *a)
{
    a->role           = perftest_lite_default_role;
    a->domain_id      = PERFTEST_LITE_DEFAULT_DOMAIN_ID;
    a->nic            = PERFTEST_LITE_DEFAULT_NIC;
    a->peer           = PERFTEST_LITE_DEFAULT_PEER;
#ifdef PERFTEST_LITE_HAS_MICRO_ZEROCOPY
    a->transport_id   = "ZeroCopy";
#else
    a->transport_id   = "UDPv4";
#endif
    a->latency_log_file = NULL;
    a->latency_stream   = PERFTEST_LITE_DEFAULT_LATENCY_STREAM;
    a->datalen        = perftest_lite_type_get()->default_sample_size();
    a->exec_seconds   = PERFTEST_LITE_DEFAULT_EXEC_SECONDS;
    a->pubrate        = PERFTEST_LITE_DEFAULT_PUBRATE;
    a->latency_count  = PERFTEST_LITE_DEFAULT_LATENCY_COUNT;
    a->sub_sleep_us   = PERFTEST_LITE_DEFAULT_SUB_SLEEP_US;
    a->reliable             = PERFTEST_LITE_DEFAULT_RELIABLE;
    a->latency_test         = PERFTEST_LITE_DEFAULT_LATENCY_TEST;
    a->print_every_second   = 0;
    a->print_config         = 1;
    a->print_resource_limits = 1;
}

void perftest_lite_free_args(PerftestLiteInputArgs *a)
{
    if (!a) return;
}

void perftest_lite_results_init(PerftestLiteResults *r,
                                PerftestLiteSizeResult *storage,
                                int32_t capacity)
{
    r->size_count    = 0;
    r->size_capacity = capacity;
    r->sizes         = storage;
}

void perftest_lite_print_help(const char *appname)
{
    PerftestLiteInputArgs defaults;
    perftest_lite_input_args_init(&defaults);

    PERFTEST_LITE_PRINT(
        "%s [options]\n"
        "  -pub | -sub                     Role (default: %s)\n"
        "  -domain <id>                    Domain ID (default: %d)\n"
        "  -nic <name>                     Network interface (default: lo)\n"
        "  -peer <addr>                    Peer initial address (default: 127.0.0.1)\n"
        "  -transport <id>                 Transport id (default: %s; compiled-in: " PERFTEST_LITE_TRANSPORT_LIST ")\n"
        "  -latencyLog <file>              Log per-pong latency samples to file\n"
        "  -latencyStream                  Stream per-pong latency via PERFTEST_LITE_PRINT ([LAT] lines)\n"
        "  -datalen <bytes>                Sample size (default: %d)\n"
        "  -exec <seconds>                 Execution time (default: %d)\n"
        "  -pubrate <samples/s>            Pub rate (default: unlimited)\n"
        "  -latencyTest                    Latency mode (ping-pong every sample)\n"
        "  -latencyCount <n>               Ping every N samples (default: %d)\n"
        "  -rel                            Reliable QoS\n"
        "  -subSleep <us>                  Subscriber loop sleep (default %d us)\n"
        "  -printPeriodic                  Print stats every second (default: off)\n"
        "  -noPrintConfig                  Suppress configuration banner\n"
        "  -h | --help                     This text\n",
        appname,
        perftest_lite_default_role == PERFTEST_LITE_ROLE_SUBSCRIBER
            ? "sub" : "pub",
        PERFTEST_LITE_DEFAULT_DOMAIN_ID,
        defaults.transport_id,
        perftest_lite_type_get()->default_sample_size(),
        PERFTEST_LITE_DEFAULT_EXEC_SECONDS,
        PERFTEST_LITE_DEFAULT_LATENCY_COUNT,
        PERFTEST_LITE_DEFAULT_SUB_SLEEP_US);
}

#define NEED_ARG(i, argc, opt) \
    do { if ((i)+1 >= (argc)) { \
        PERFTEST_LITE_PRINT("missing value for %s\n", (opt)); return -1; \
    } } while (0)

int perftest_lite_parse_arguments(int argc, char **argv,
                                  PerftestLiteInputArgs *args)
{
    int32_t value;

    perftest_lite_input_args_init(args);
    for (int i = 1; i < argc; ++i) {
        const char *opt = argv[i];
        if (!strcmp(opt, "-h") || !strcmp(opt, "--help")) {
            perftest_lite_print_help(argv[0]);
            return PERFTEST_LITE_PARSE_HELP;
        } else if (!strcmp(opt, "-pub")) { args->role = PERFTEST_LITE_ROLE_PUBLISHER; }
        else if (!strcmp(opt, "-sub"))   { args->role = PERFTEST_LITE_ROLE_SUBSCRIBER; }
        else if (!strcmp(opt, "-domain")) {
            NEED_ARG(i, argc, opt);
            if (parse_int32_option(opt, argv[++i], 0, 232, &value) != 0) {
                return PERFTEST_LITE_PARSE_ERROR;
            }
            args->domain_id = value;
        }
        else if (!strcmp(opt, "-nic"))       { NEED_ARG(i, argc, opt); args->nic          = argv[++i]; }
        else if (!strcmp(opt, "-peer"))      { NEED_ARG(i, argc, opt); args->peer         = argv[++i]; }
        else if (!strcmp(opt, "-transport")) { NEED_ARG(i, argc, opt); args->transport_id = argv[++i]; }
        else if (!strcmp(opt, "-latencyLog")){ NEED_ARG(i, argc, opt); args->latency_log_file = argv[++i]; }
        else if (!strcmp(opt, "-datalen")) {
            NEED_ARG(i, argc, opt);
            if (parse_int32_option(
                    opt, argv[++i], 1, INT32_MAX, &value) != 0) {
                return PERFTEST_LITE_PARSE_ERROR;
            }
            args->datalen = value;
        }
        else if (!strcmp(opt, "-exec")) {
            NEED_ARG(i, argc, opt);
            if (parse_int32_option(
                    opt, argv[++i], 1, INT32_MAX, &value) != 0) {
                return PERFTEST_LITE_PARSE_ERROR;
            }
            args->exec_seconds = value;
        }
        else if (!strcmp(opt, "-pubrate")) {
            NEED_ARG(i, argc, opt);
            if (parse_int32_option(opt, argv[++i], -1, INT32_MAX, &value) != 0) {
                return PERFTEST_LITE_PARSE_ERROR;
            }
            if (value == 0) {
                PERFTEST_LITE_PRINT(
                    "invalid value for %s: '0' (expected -1 or 1..%d)\n",
                    opt, INT32_MAX);
                return PERFTEST_LITE_PARSE_ERROR;
            }
            args->pubrate = value;
        }
        else if (!strcmp(opt, "-latencyCount")) {
            NEED_ARG(i, argc, opt);
            if (parse_int32_option(
                    opt, argv[++i], 1, INT32_MAX, &value) != 0) {
                return PERFTEST_LITE_PARSE_ERROR;
            }
            args->latency_count = value;
        }
        else if (!strcmp(opt, "-subSleep")) {
            NEED_ARG(i, argc, opt);
            if (parse_int32_option(
                    opt, argv[++i], 0, INT32_MAX, &value) != 0) {
                return PERFTEST_LITE_PARSE_ERROR;
            }
            args->sub_sleep_us = value;
        }
        else if (!strcmp(opt, "-rel"))          { args->reliable = 1; }
        else if (!strcmp(opt, "-latencyTest"))  { args->latency_test = 1; }
        else if (!strcmp(opt, "-printPeriodic")) { args->print_every_second = 1; }
        else if (!strcmp(opt, "-noPrintConfig")){ args->print_config = 0; }
        else if (!strcmp(opt, "-latencyStream")) { args->latency_stream = 1; }
        else {
            PERFTEST_LITE_PRINT("unknown option: %s\n", opt);
            return PERFTEST_LITE_PARSE_ERROR;
        }
    }
    return PERFTEST_LITE_PARSE_OK;
}

void perftest_lite_print_input_args(const PerftestLiteInputArgs *a)
{
    PERFTEST_LITE_PRINT(
        "\n=== Perftest Lite Configuration ===\n"
        "  Role          : %s\n"
        "  Domain        : %d\n"
        "  NIC           : %s\n"
        "  Peer          : %s\n"
        "  Transport     : %s\n"
        "  Latency log   : %s\n"
        "  Latency stream: %s\n"
        "  Reliability   : %s\n"
        "  Datalen       : %d\n"
        "  Exec time     : %d s\n"
        "  Pubrate       : %d\n"
        "  Latency test  : %s (latencyCount=%d)\n",
        a->role == PERFTEST_LITE_ROLE_PUBLISHER ? "Publisher" : "Subscriber",
        a->domain_id, a->nic,
        a->peer,
        a->transport_id,
        a->latency_log_file ? a->latency_log_file : "(disabled)",
        a->latency_stream ? "yes" : "no",
        a->reliable ? "Reliable" : "Best Effort",
        a->datalen, a->exec_seconds, a->pubrate,
        a->latency_test ? "yes" : "no", a->latency_count);

    if (a->print_resource_limits) {
        PERFTEST_LITE_PRINT(
            "--- Resource limits (effective %s mode) ---\n"
            "  UDP max msg : %d\n"
            "  UDP recv buf: %d\n"
            "  UDP send buf: %d\n"
            "  DW history  : %d (max samples %d)\n"
            "  DR history  : %d (max samples %d, max remote writers %d)\n"
            "  Init burst  : %d\n",
            a->latency_test ? "latency" : "throughput",
            PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE,
            PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE,
            PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE,
            a->latency_test ? PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY
                            : PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT,
            a->latency_test ? PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY
                            : PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT,
            a->latency_test ? PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY
                            : PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT,
            a->latency_test ? PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY
                            : PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT,
            PERFTEST_LITE_DR_MAX_REMOTE_WRITERS,
            PERFTEST_LITE_INIT_BURST);
    }
    PERFTEST_LITE_PRINT("===================================\n\n");
}
