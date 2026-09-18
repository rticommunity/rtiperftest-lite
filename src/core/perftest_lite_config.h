/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Perftest Lite - configuration header.
 *
 * Per the design (Parameters Config Header File), all resource limits and
 * tunable settings are centralized here as preprocessor defines. Each port
 * can override these individually by passing `-D` flags during compilation
 * or by setting YAML defines.
 */
#ifndef PERFTEST_LITE_CONFIG_H
#define PERFTEST_LITE_CONFIG_H

/* A port may inject defaults and hooks before the built-in fallbacks below.
 * Define this as a quoted header name, for example:
 * -DPERFTEST_LITE_PLATFORM_CONFIG_HEADER=\"my_platform_config.h\" */
#ifdef PERFTEST_LITE_PLATFORM_CONFIG_HEADER
#  include PERFTEST_LITE_PLATFORM_CONFIG_HEADER
#endif

/*
 * Default network interface fallback. Platform/profile/toolchain defines can
 * set DEFAULT_NETWORK_INTERFACE before this header is parsed.
 *
 * Keep it as a bare token (lo/st0/eth0) and stringify only at use-site so
 * both -DDEFAULT_NETWORK_INTERFACE=st0 and quoted forms work.
 */
#ifndef PERFTEST_LITE_STRINGIFY
#  define PERFTEST_LITE_STRINGIFY(x) #x
#  define PERFTEST_LITE_STRINGIFY_VALUE(x) PERFTEST_LITE_STRINGIFY(x)
#endif

#ifndef PERFTEST_LITE_ZCOPY_MAX_SAMPLES_PER_NOTIF_LATENCY
#  define PERFTEST_LITE_ZCOPY_MAX_SAMPLES_PER_NOTIF_LATENCY 1
#endif

/* Start conservatively. Platforms may increase this after measuring the
 * notification and scheduling behavior of their target. */
#ifndef PERFTEST_LITE_ZCOPY_MAX_SAMPLES_PER_NOTIF_THROUGHPUT
#  define PERFTEST_LITE_ZCOPY_MAX_SAMPLES_PER_NOTIF_THROUGHPUT 500
#endif

/* ---- Defaults for runtime parameters (used when no CLI is available) --- */
#ifndef PERFTEST_LITE_DEFAULT_DOMAIN_ID
#  define PERFTEST_LITE_DEFAULT_DOMAIN_ID 0
#endif

#ifndef PERFTEST_LITE_DEFAULT_PEER
#  ifdef DEVICE_PEER_STR
#    ifndef PERFTEST_LITE_STRINGIFY
#      define PERFTEST_LITE_STRINGIFY(x) #x
#      define PERFTEST_LITE_STRINGIFY_VALUE(x) PERFTEST_LITE_STRINGIFY(x)
#    endif
#    define PERFTEST_LITE_DEFAULT_PEER PERFTEST_LITE_STRINGIFY_VALUE(DEVICE_PEER_STR)
#  else
#    define PERFTEST_LITE_DEFAULT_PEER "127.0.0.1"
#  endif
#endif

#ifndef PERFTEST_LITE_DEFAULT_NIC
#  if defined(PERFTEST_LITE_OS_FORGE)
#    include "netconfig.h"  /* defines DEFAULT_NETWORK_INTERFACE as a string literal */
#    define PERFTEST_LITE_DEFAULT_NIC DEFAULT_NETWORK_INTERFACE
#  elif defined(DEFAULT_NETWORK_INTERFACE)
#    define PERFTEST_LITE_DEFAULT_NIC PERFTEST_LITE_STRINGIFY_VALUE(DEFAULT_NETWORK_INTERFACE)
#  else
#    define PERFTEST_LITE_DEFAULT_NIC "lo"
#  endif
#endif

#ifndef PERFTEST_LITE_DEFAULT_DATALEN
#  define PERFTEST_LITE_DEFAULT_DATALEN 100
#endif

#ifndef PERFTEST_LITE_DEFAULT_EXEC_SECONDS
#  define PERFTEST_LITE_DEFAULT_EXEC_SECONDS 10
#endif

#ifndef PERFTEST_LITE_DEFAULT_LATENCY_COUNT
#  define PERFTEST_LITE_DEFAULT_LATENCY_COUNT 10000
#endif

/* Maximum number of latency samples stored for percentile computation.
 * Each entry is a uint32_t (4 bytes); 10000000 entries = 40 MB.
 * Matches perftest's default numIter for latency tests (10 000 000 / latencyCount=1).
 * Set to 0 to disable percentiles (saves memory on very constrained targets). */
#ifndef PERFTEST_LITE_LATENCY_HISTORY_SIZE
#  define PERFTEST_LITE_LATENCY_HISTORY_SIZE 0
#endif

#ifndef PERFTEST_LITE_DEFAULT_PUBRATE
#  define PERFTEST_LITE_DEFAULT_PUBRATE -1  /* unlimited */
#endif

#ifndef PERFTEST_LITE_DEFAULT_LATENCY_TEST
#  define PERFTEST_LITE_DEFAULT_LATENCY_TEST 0
#endif

/* When set to 1, stream per-sample latency via PERFTEST_LITE_PRINT as
 * "[LAT] elapsed_us,latency_us" lines.  Zero memory overhead; works on any
 * platform with PERFTEST_LITE_PRINT (UART on FreeRTOS, stdout on Linux).
 * Independent of -latencyLog: both can be active simultaneously. */
#ifndef PERFTEST_LITE_DEFAULT_LATENCY_STREAM
#  define PERFTEST_LITE_DEFAULT_LATENCY_STREAM 0
#endif

#ifndef PERFTEST_LITE_DEFAULT_RELIABLE
#  define PERFTEST_LITE_DEFAULT_RELIABLE 0
#endif

/* Subscriber default loop sleep in microseconds */
#ifndef PERFTEST_LITE_DEFAULT_SUB_SLEEP_US
#  define PERFTEST_LITE_DEFAULT_SUB_SLEEP_US 1000000
#endif

/* ---- Discovery / topic identifiers ------------------------------------- */
#define PERFTEST_LITE_PARTICIPANT_NAME_PUB  "PerftestLite_publisher"
#define PERFTEST_LITE_PARTICIPANT_NAME_SUB  "PerftestLite_subscriber"
#define PERFTEST_LITE_PARTICIPANT_ID_PUB    1
#define PERFTEST_LITE_PARTICIPANT_ID_SUB    2

#define PERFTEST_LITE_PING_TOPIC_NAME       "Perftest_ping"
#define PERFTEST_LITE_PONG_TOPIC_NAME       "Perftest_pong"
#define PERFTEST_LITE_TYPE_NAME             "PerftestType"

/* DPSE keys (only used when DPSE is the active discovery plugin) */
#define PERFTEST_LITE_PING_DW_KEY 101
#define PERFTEST_LITE_PING_DR_KEY 111
#define PERFTEST_LITE_PONG_DW_KEY 211
#define PERFTEST_LITE_PONG_DR_KEY 201

/* ---- DDS resource limits (tweak per platform) -------------------------- */
#ifndef PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE
#  define PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE      65507
#endif

#ifndef PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE
#  define PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE  4194304
#endif

#ifndef PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE
#  define PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE  2097152
#endif

/* Endpoint pools are selected at runtime from -latencyTest before the DDS
 * endpoints are created. Define either mode-specific value to tune one pool
 * without duplicating a complete build profile. */
#ifndef PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY
#  define PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY 2
#endif

#ifndef PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY
#  define PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY 2
#endif

#ifndef PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY
#  define PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY 2
#endif

#ifndef PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY
#  define PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY 2
#endif

#ifndef PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT
#  define PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT 8
#endif

#ifndef PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT
#  define PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT 256
#endif

#ifndef PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT
#  define PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT 16
#endif

#ifndef PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT
#  define PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT 256
#endif

#if PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY < 1 \
     || PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY > PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY
#  error "Latency writer history depth must be between 1 and its maximum samples."
#endif

#if PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY < 1 \
     || PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY > PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY
#  error "Latency reader history depth must be between 1 and its maximum samples."
#endif

#if PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT < 1 \
     || PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT > PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT
#  error "Throughput writer history depth must be between 1 and its maximum samples."
#endif

#if PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT < 1 \
     || PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT > PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT
#  error "Throughput reader history depth must be between 1 and its maximum samples."
#endif

#ifndef PERFTEST_LITE_DW_MAX_INSTANCES
#  define PERFTEST_LITE_DW_MAX_INSTANCES          1
#endif

#ifndef PERFTEST_LITE_DR_MAX_INSTANCES
#  define PERFTEST_LITE_DR_MAX_INSTANCES          1
#endif

#ifndef PERFTEST_LITE_DR_MAX_REMOTE_WRITERS
#  define PERFTEST_LITE_DR_MAX_REMOTE_WRITERS     1
#endif

/* ---- Reliable DataWriter heartbeat period --------------------------------
 * Micro's default is 3 s, which stalls the first latency sample.
 * rti-perftest uses 10 ms; we match that.
 * Override with -DPERFTEST_LITE_DW_HEARTBEAT_PERIOD_NS=N */
#ifndef PERFTEST_LITE_DW_HEARTBEAT_PERIOD_NS
#  define PERFTEST_LITE_DW_HEARTBEAT_PERIOD_NS 10000000  /* 10 ms */
#endif

/* ---- Initialization burst --------------------------------------------- */
#ifndef PERFTEST_LITE_INIT_BURST
#  define PERFTEST_LITE_INIT_BURST 50
#endif

/* ---- Init burst sample size -------------------------------------------
 * Matches rti-perftest's INITIALIZE_SIZE = 1234.  A fixed size independent
 * of the test data size ensures that the init burst always occupies the
 * same WHSM/RHSM slots regardless of what follows.  The pong callback
 * discards echoed INITIALIZATION samples without signalling the semaphore,
 * matching rti-perftest's LatencyListener (case INITIALIZE_SIZE: return). */
#ifndef PERFTEST_LITE_INIT_SAMPLE_SIZE
#  define PERFTEST_LITE_INIT_SAMPLE_SIZE 1234
#endif

/* ---- Publisher minimum inter-sample sleep (microseconds) --------------
 * When -pubrate is unlimited (<= 0) the loop would otherwise hot-spin and
 * starve cooperative network stacks (e.g. LwIP TCPIP thread on embedded
 * targets), leading to udp_sendto returning ERR_MEM/ERR_BUF and dropped
 * samples. Override per port with -DPERFTEST_LITE_PUB_MIN_SLEEP_US=N (a
 * small non-zero value forces the loop to yield every iteration). */
#ifndef PERFTEST_LITE_PUB_MIN_SLEEP_US
#  define PERFTEST_LITE_PUB_MIN_SLEEP_US 0
#endif

/* ---- Print macro: redefine for embedded targets without printf -------- */
#ifndef PERFTEST_LITE_PRINT
#  if defined(PERFTEST_LITE_USE_FORGE)
     /* Substrate / Forge: route through hammer's UART-prefixed printer. */
#    include "FORGE_Stdio.h"
#    define PERFTEST_LITE_PRINT(...) FORGE_Stdio_printf_stderr(__VA_ARGS__)
#  else
#    include <stdio.h>
#    define PERFTEST_LITE_PRINT(...) printf(__VA_ARGS__)
#  endif
#endif

#endif /* PERFTEST_LITE_CONFIG_H */
