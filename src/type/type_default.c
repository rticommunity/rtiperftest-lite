/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Default Type implementation: wraps the IDL-generated PerftestType.
 *
 * Per the design (Type Interface), the engine NEVER touches the generated
 * type directly. All interactions go through this interface.
 */
#ifndef PERFTEST_LITE_TYPE_ZCOPY

#include "type_interface.h"
#include "../core/perftest_lite.h"

#include "rti_me_c.h"
#include "perftest.h"
#include "perftestSupport.h"

#include <string.h>

/* PerftestSample is opaque to the engine; here it's just an alias. */
struct PerftestSample {
    PerftestType impl;
};

static int type_set_payload_length(PerftestSample *s, int32_t length);

static int type_validate_input(const PerftestLiteInputArgs *args)
{
    return args->datalen >= PERFTEST_TYPE_OVERHEAD
        && args->datalen - PERFTEST_TYPE_OVERHEAD
            <= PERFTEST_TYPE_MAX_PAYLOAD ? 0 : -1;
}

static int32_t type_default_sample_size(void)
{
    if (PERFTEST_LITE_DEFAULT_DATALEN < PERFTEST_TYPE_OVERHEAD) {
        return PERFTEST_TYPE_OVERHEAD;
    }
    if (PERFTEST_LITE_DEFAULT_DATALEN - PERFTEST_TYPE_OVERHEAD
            > PERFTEST_TYPE_MAX_PAYLOAD) {
        return PERFTEST_TYPE_OVERHEAD + PERFTEST_TYPE_MAX_PAYLOAD;
    }
    return PERFTEST_LITE_DEFAULT_DATALEN;
}

static void type_print_input_error(void)
{
    PERFTEST_LITE_PRINT(
        "[err] input is not supported by the selected type "
        "(-datalen must be %d..%d bytes; compiled payload maximum is %d)\n",
        (int) PERFTEST_TYPE_OVERHEAD,
        (int) (PERFTEST_TYPE_OVERHEAD + PERFTEST_TYPE_MAX_PAYLOAD),
        (int) PERFTEST_TYPE_MAX_PAYLOAD);
}

    static PerftestSample *type_create(void)
{
    PerftestSample *s = (PerftestSample *)PerftestType_create();
    if (!s) return NULL;
        if (!DDS_OctetSeq_set_maximum(
            &s->impl.bin_data, PERFTEST_TYPE_MAX_PAYLOAD)) {
        PerftestType_delete(&s->impl);
        return NULL;
    }
        if (!DDS_OctetSeq_set_length(
            &s->impl.bin_data, PERFTEST_TYPE_MAX_PAYLOAD)) {
        PerftestType_delete(&s->impl);
        return NULL;
    }
    return s;
}

static void type_destroy(PerftestSample *s)
{
    if (s) PerftestType_delete(&s->impl);
}

static int type_initialize(PerftestSample *s)
{
    s->impl.sequence_number = 0;
    s->impl.kind            = PERFTEST_KIND_DATA;
    s->impl.timestamp_us    = 0;
    return 0;
}

static int type_copy_metadata(PerftestSample *dst, const PerftestSample *src)
{
    /* Per design: copy every field except bin_data. */
    dst->impl.sequence_number = src->impl.sequence_number;
    dst->impl.kind            = src->impl.kind;
    dst->impl.timestamp_us    = src->impl.timestamp_us;
    return 0;
}

static int type_copy_for_write(PerftestSample *dst, const PerftestSample *src)
{
    int32_t length = DDS_OctetSeq_get_length(&src->impl.bin_data);
    DDS_Octet *src_data;
    DDS_Octet *dst_data;

    if (type_copy_metadata(dst, src) != 0) return -1;
    if (type_set_payload_length(dst, length) != 0) return -1;
    if (length == 0) return 0;

    src_data = DDS_OctetSeq_get_contiguous_buffer(&src->impl.bin_data);
    dst_data = DDS_OctetSeq_get_contiguous_buffer(&dst->impl.bin_data);
    if (!src_data || !dst_data) return -1;
    memcpy(dst_data, src_data, (size_t)length);
    return 0;
}

static void type_set_kind(PerftestSample *s, PerftestTypeKind k)
{
    s->impl.kind = (PerftestKind)k;
}

static PerftestTypeKind type_get_kind(const PerftestSample *s)
{
    return (PerftestTypeKind)s->impl.kind;
}

static void type_set_seq(PerftestSample *s, int32_t n)        { s->impl.sequence_number = n; }
static int32_t type_get_seq(const PerftestSample *s)          { return s->impl.sequence_number; }

static void type_set_ts(PerftestSample *s, uint64_t t)        { s->impl.timestamp_us = t; }
static uint64_t type_get_ts(const PerftestSample *s)          { return s->impl.timestamp_us; }

static int type_set_payload_length(PerftestSample *s, int32_t length)
{
    if (length > DDS_OctetSeq_get_maximum(&s->impl.bin_data)) {
        if (!DDS_OctetSeq_set_maximum(&s->impl.bin_data, length)) return -1;
    }
    if (!DDS_OctetSeq_set_length(&s->impl.bin_data, length)) return -1;
    return 0;
}

static int32_t type_get_payload_length(const PerftestSample *s)
{
    return DDS_OctetSeq_get_length(&s->impl.bin_data);
}

static int type_set_sample_size(PerftestSample *s, int32_t size)
{
    if (size < PERFTEST_TYPE_OVERHEAD
            || size - PERFTEST_TYPE_OVERHEAD
                > PERFTEST_TYPE_MAX_PAYLOAD) {
        return -1;
    }
    return type_set_payload_length(s, size - PERFTEST_TYPE_OVERHEAD);
}

static int32_t type_get_sample_size(const PerftestSample *s)
{
    return type_get_payload_length(s) + PERFTEST_TYPE_OVERHEAD;
}

static void *type_raw(PerftestSample *s) { return &s->impl; }

static const char *type_name(void) { return PerftestTypeTYPENAME; }

static const PerftestLiteTypeInterface g_type_iface = {
    .validate_input       = type_validate_input,
    .default_sample_size  = type_default_sample_size,
    .print_input_error    = type_print_input_error,
    .create               = type_create,
    .destroy              = type_destroy,
    .initialize           = type_initialize,
    .copy_metadata        = type_copy_metadata,
    .copy_for_write       = type_copy_for_write,
    .set_kind             = type_set_kind,
    .get_kind             = type_get_kind,
    .set_sequence_number  = type_set_seq,
    .get_sequence_number  = type_get_seq,
    .set_timestamp_us     = type_set_ts,
    .get_timestamp_us     = type_get_ts,
    .set_sample_size      = type_set_sample_size,
    .get_sample_size      = type_get_sample_size,
    .raw                  = type_raw,
    .type_name            = type_name
};

const PerftestLiteTypeInterface *perftest_lite_type_get(void)
{
    return &g_type_iface;
}

#endif /* !PERFTEST_LITE_TYPE_ZCOPY */
