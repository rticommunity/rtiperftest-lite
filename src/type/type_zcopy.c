/*
 * Copyright (c) 2026 Real-Time Innovations, Inc.
 * Subject to the Eclipse Public License v1.0; see LICENSE.md for details.
 */

/*
 * Zero Copy Type implementation: wraps the fixed-array IDL-generated PerftestType.
 *
 * Selected when PERFTEST_LITE_TYPE=zcopy.  All generated symbols come from
 * the fixed-array IDL variant (idl/zcopy/perftest.idl).
 *
 * Key differences from type_default.c (sequence variant):
 * - create() has no runtime capacity; the struct is fixed size.
 * - set_sample_size() accepts only the generated fixed sample size.
 * - copy_for_write() copies metadata but NOT bin_data, because (a) the
 *   benchmark core does not inspect payload bytes, and (b) copying 64 KB per
 *   sample would negate the Zero Copy benefit.
 */
#ifdef PERFTEST_LITE_TYPE_ZCOPY

#include "type_interface.h"
#include "../core/perftest_lite_config.h"
#include "../core/perftest_lite.h"

#include "rti_me_c.h"
#include "perftest.h"
#include "perftestSupport.h"

#include <stdlib.h>
#include <string.h>

/* PerftestSample is opaque to the engine; here it is just an alias. */
struct PerftestSample {
    PerftestType impl;
};

static PerftestSample *type_create(void)
{
    PerftestSample *s = (PerftestSample *)PerftestType_create();
    return s;
}

static int type_validate_input(const PerftestLiteInputArgs *args)
{
    return args->datalen == PERFTEST_TYPE_DATALEN ? 0 : -1;
}

static int32_t type_default_sample_size(void)
{
    return PERFTEST_TYPE_DATALEN;
}

static void type_print_input_error(void)
{
    PERFTEST_LITE_PRINT(
        "[err] input is not supported by the selected type "
        "(-datalen must be %d bytes for the fixed-size type)\n",
        (int) PERFTEST_TYPE_DATALEN);
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
    /* Subscriber echo path: copy identity fields only. */
    dst->impl.sequence_number = src->impl.sequence_number;
    dst->impl.kind            = src->impl.kind;
    dst->impl.timestamp_us    = src->impl.timestamp_us;
    return 0;
}

static int type_copy_for_write(PerftestSample *dst, const PerftestSample *src)
{
    /* Writer-facing copy used by the Zero Copy loan path. Skip bin_data to
     * avoid a PERFTEST_TYPE_MAX_DATA_LEN-byte memcpy on every write. */
    dst->impl.sequence_number = src->impl.sequence_number;
    dst->impl.kind            = src->impl.kind;
    dst->impl.timestamp_us    = src->impl.timestamp_us;
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

static void type_set_seq(PerftestSample *s, int32_t n)
{
    s->impl.sequence_number = n;
}
static int32_t type_get_seq(const PerftestSample *s)
{
    return s->impl.sequence_number;
}

static void type_set_ts(PerftestSample *s, uint64_t t)
{
    s->impl.timestamp_us = t;
}
static uint64_t type_get_ts(const PerftestSample *s)
{
    return s->impl.timestamp_us;
}

static int type_set_sample_size(PerftestSample *s, int32_t size)
{
    (void)s;
    return size == PERFTEST_TYPE_DATALEN ? 0 : -1;
}

static int32_t type_get_sample_size(const PerftestSample *s)
{
    (void)s;
    return PERFTEST_TYPE_DATALEN;
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

#endif /* PERFTEST_LITE_TYPE_ZCOPY */
