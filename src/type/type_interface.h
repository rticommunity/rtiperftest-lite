/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Type Interface (per design: Type Interface section).
 *
 * Encapsulates all interactions with the IDL-generated Perftest type, so
 * different IDL variants (sequence, ZeroCopy array, etc.) can be plugged
 * in without touching the engine.
 *
 * The opaque PerftestSample* points to whatever the underlying type is
 * (e.g., a generated `PerftestType*`). The engine never dereferences it
 * directly: every read/write uses this interface.
 */
#ifndef PERFTEST_LITE_TYPE_INTERFACE_H
#define PERFTEST_LITE_TYPE_INTERFACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PERFTEST_TYPE_KIND_DATA           = 0,
    PERFTEST_TYPE_KIND_DATA_PING      = 1,
    PERFTEST_TYPE_KIND_INITIALIZATION = 2,
    PERFTEST_TYPE_KIND_FINALIZATION   = 3
} PerftestTypeKind;

typedef struct PerftestSample PerftestSample;
typedef struct PerftestLiteInputArgs PerftestLiteInputArgs;

typedef struct PerftestLiteTypeInterface {
    /* Validates benchmark inputs for this type and returns 0 on success.
     * Invoked by perftest_lite_main() for both CLI and embedded callers. */
    int (*validate_input)(const PerftestLiteInputArgs *args);
    int32_t (*default_sample_size)(void);
    void (*print_input_error)(void);

    /* Lifecycle */
    PerftestSample *(*create)(void);
    void            (*destroy)(PerftestSample *s);

    /* Initialize the contents (sets all fields to zero / empty) */
    int (*initialize)(PerftestSample *s);

    /* Lightweight "copy back" used by the subscriber: copies all fields
     * from src to dst EXCEPT the binary data (which is the heavy field),
     * keeping dst's binary buffer untouched. This is the trick described
     * in "Other design details" (avoid copies of bin_data). */
    int (*copy_metadata)(PerftestSample *dst, const PerftestSample *src);

    /* Writer-side copy. For Zero Copy this populates a DDS-loaned sample
     * without copying the fixed payload array. */
    int (*copy_for_write)(PerftestSample *dst, const PerftestSample *src);

    /* Field setters/getters (primitive fields are exposed directly). */
    void (*set_kind)(PerftestSample *s, PerftestTypeKind k);
    PerftestTypeKind (*get_kind)(const PerftestSample *s);

    void (*set_sequence_number)(PerftestSample *s, int32_t n);
    int32_t (*get_sequence_number)(const PerftestSample *s);

    void (*set_timestamp_us)(PerftestSample *s, uint64_t t);
    uint64_t (*get_timestamp_us)(const PerftestSample *s);

    /* Reported sample size and binary payload. */
    int     (*set_sample_size)(PerftestSample *s, int32_t size);
    int32_t (*get_sample_size)(const PerftestSample *s);

    /* Underlying generated type pointer (passed to middleware writes/reads). */
    void *(*raw)(PerftestSample *s);

    /* Type name (for type registration with the middleware). */
    const char *(*type_name)(void);
} PerftestLiteTypeInterface;

/* Default type implementation (sequence-based). Selected at compile time. */
const PerftestLiteTypeInterface *perftest_lite_type_get(void);

#ifdef __cplusplus
}
#endif

#endif /* PERFTEST_LITE_TYPE_INTERFACE_H */
