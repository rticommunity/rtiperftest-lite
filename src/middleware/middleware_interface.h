/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Middleware Interface (per design: Middleware Interface section).
 *
 * Top-level interface used by the engine to talk to a DDS (or raw UDP)
 * middleware. It returns reader / writer / listener interface
 * implementations that the engine then uses.
 *
 * The transport identifier is purely declarative ("UDPv4", "ZeroCopy").
 * Each middleware implementation interprets it.
 */
#ifndef PERFTEST_LITE_MIDDLEWARE_INTERFACE_H
#define PERFTEST_LITE_MIDDLEWARE_INTERFACE_H

#include <stdint.h>
#include "../type/type_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PerftestLiteWriter PerftestLiteWriter;
typedef struct PerftestLiteReader PerftestLiteReader;
typedef struct PerftestLiteMiddleware PerftestLiteMiddleware;

/* ---- Listener interface: callbacks the engine wants to receive -------- */
typedef struct PerftestLiteListener {
    void *user_data;
    /* Lifecycle: called before the first sample is written/taken.
     * Use to finalize QoS, enable logging, arm timers, etc. */
    void (*on_start)(void *user_data);
    /* Lifecycle: called after the last sample is written/taken.
     * Use to flush logs, tear down QoS overrides, etc. */
    void (*on_end)(void *user_data);
    void (*on_publication_matched)(void *user_data, int32_t change);
    void (*on_subscription_matched)(void *user_data, int32_t change);
    void (*on_data_available)(void *user_data, PerftestLiteReader *reader);
} PerftestLiteListener;

/* ---- Configuration passed when creating a middleware ------------------ */
typedef struct PerftestLiteMiddlewareConfig {
    const PerftestLiteTypeInterface *type_iface;

    int32_t     domain_id;
    const char *participant_name;
    int32_t     participant_id;
    const char *peer;          /* may be NULL */
    const char *nic;           /* may be NULL */
    const char *transport_id;  /* "UDPv4" by default */

    int reliable;
    int is_publisher;
    /* Select middleware resources that trade latency for batching. */
    int latency_test;

#if defined(PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS)
    /* OSAPI heap allocation count captured before engine-owned setup. It is
     * used only for lifecycle memory checkpoint deltas. */
    uint64_t process_start_heap_bytes;
#endif

    /* For DPSE-like middlewares that need to know the remote endpoint
     * names ahead of time. NULL for middlewares that auto-discover. */
    const char *remote_participant_name;
} PerftestLiteMiddlewareConfig;

/* ---- Writer interface ------------------------------------------------- */
struct PerftestLiteWriter {
    int (*write)(PerftestLiteWriter *w, PerftestSample *sample);
    void *impl;
};

/* ---- Reader interface ------------------------------------------------- */
struct PerftestLiteReader {
    /* Take samples and feed them one by one to `cb`. The middleware owns
     * the storage; the engine must not keep references after the callback
     * returns. Returns the number of samples taken (0 if nothing). */
    int (*take)(PerftestLiteReader *r,
                void (*cb)(void *user, PerftestSample *sample),
                void *user);
    void *impl;
};

/* ---- Middleware interface (top level) --------------------------------- */
struct PerftestLiteMiddleware {
    int (*init)(PerftestLiteMiddleware *m,
                const PerftestLiteMiddlewareConfig *cfg);

    PerftestLiteWriter *(*create_writer)(PerftestLiteMiddleware *m,
                                         const char *topic_name,
                                         int reliable);
    PerftestLiteReader *(*create_reader)(PerftestLiteMiddleware *m,
                                         const char *topic_name,
                                         int reliable,
                                         const PerftestLiteListener *l);

    /* Wait until at least one matching endpoint of each kind is found. */
    int (*wait_for_match)(PerftestLiteMiddleware *m, int32_t timeout_ms);

    void (*shutdown)(PerftestLiteMiddleware *m);

    void *impl;
};

/* Returns the middleware implementation selected at compile time
 * (PERFTEST_LITE_MW_MICRO is the default). */
PerftestLiteMiddleware *perftest_lite_middleware_get(void);

#ifdef __cplusplus
}
#endif

#endif /* PERFTEST_LITE_MIDDLEWARE_INTERFACE_H */
