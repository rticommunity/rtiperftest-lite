# Adding a New Type Variant to Perftest Lite

This guide explains how to add a new IDL type variant to Perftest Lite for
latency and throughput testing. The architecture is designed for this — no
modifications to the publisher or subscriber core logic are required.

---

## Overview

Perftest Lite uses a **type interface** (`src/type/type_interface.h`) to
decouple the test engine from the concrete DDS type. The engine operates on
opaque `PerftestSample*` pointers and accesses fields exclusively through
function pointers.

Adding a new type variant requires:

1. A new `idl/<variant>/perftest.idl` file declaring `struct PerftestType`.
2. A new type implementation (`.c` file) implementing `PerftestLiteTypeInterface`.
3. A CMake entry to select the variant at build time.

The publisher, subscriber, renderer, OS layer, and middleware remain unchanged.

---

## Architecture at a Glance

```
┌─────────────────────────────────────────────────────┐
│  Engine (perftest_lite_pub.c / perftest_lite_sub.c)  │
│  Uses: T->set_kind(), T->get_timestamp_us(), etc.   │
└────────────────────────┬────────────────────────────┘
                         │ PerftestLiteTypeInterface
┌────────────────────────▼────────────────────────────┐
│  Type Implementation (type_default.c / type_zcopy.c) │
│  Wraps the generated PerftestType struct             │
└────────────────────────┬────────────────────────────┘
                         │ #include "perftest.h"
┌────────────────────────▼────────────────────────────┐
│  Generated Code (from rtiddsgen)                     │
│  perftest.h / perftest.c / perftestPlugin.c/h        │
└─────────────────────────────────────────────────────┘
```

---

## Step-by-Step Guide

### Step 1 — Create the IDL file

Create `idl/<variant>/perftest.idl`. Every variant uses the basename
`perftest` so direct `rtiddsgen` generation always produces the stable public
headers `perftest.h`, `perftestPlugin.h`, and `perftestSupport.h`.

**Mandatory rules:**

- The struct **must** be named `PerftestType`.
- The enum **must** be named `PerftestKind` with the same five values:
  ```idl
  enum PerftestKind {
      PERFTEST_KIND_DATA,
      PERFTEST_KIND_DATA_PING,
      PERFTEST_KIND_INITIALIZATION,
      PERFTEST_KIND_FINALIZATION,
      PERFTEST_KIND_SIZE_CHANGE
  };
  ```
- The struct **must** contain these fields (names and types must match exactly):
  ```idl
  long               sequence_number;
  PerftestKind       kind;
  unsigned long long timestamp_us;
  ```
- The struct **must** provide a way to carry a logical payload size. This can be:
  - A `sequence<octet>` whose length represents the size (like the default variant).
  - An explicit `long data_length` field (like the Zero Copy variant).
    - Any other mechanism, as long as the adapter implements the reported sample
        size and payload operations.

**Optional:**

- Additional fields (e.g., keys, extra metadata) are allowed.
- Annotations (e.g., `@transfer_mode(SHMEM_REF)`) can be added as needed.
- A generator-input macro and public IDL constants can define type sizing.

**Example — a hypothetical flat fixed-header type:**

```idl
#ifndef PERFTEST_TYPE_CONFIGURED_MAX_PAYLOAD
#define PERFTEST_TYPE_CONFIGURED_MAX_PAYLOAD 1024
#endif
const long PERFTEST_TYPE_OVERHEAD = 28;
const long PERFTEST_TYPE_MAX_PAYLOAD =
    PERFTEST_TYPE_CONFIGURED_MAX_PAYLOAD;

enum PerftestKind {
    PERFTEST_KIND_DATA,
    PERFTEST_KIND_DATA_PING,
    PERFTEST_KIND_INITIALIZATION,
    PERFTEST_KIND_FINALIZATION,
    PERFTEST_KIND_SIZE_CHANGE
};

struct PerftestType {
    long               sequence_number;
    PerftestKind       kind;
    unsigned long long timestamp_us;
    long               data_length;
    octet              payload[PERFTEST_TYPE_MAX_PAYLOAD];
};
```

`rtiddsgen` emits IDL constants into the generated header. Consume those
constants inside the type adapter instead of defining a matching C-side bound.

### Step 2 — Create the type implementation

Create `src/type/type_<variant>.c`.

This file must:

1. `#include "type_interface.h"`
2. `#include "perftest.h"` and `"perftestSupport.h"` (generated headers).
3. Define `struct PerftestSample` wrapping the generated `PerftestType`.
4. Implement every function in `PerftestLiteTypeInterface`.
5. Export the interface via `perftest_lite_type_get()`.

**Required functions:**

| Function | Purpose |
|----------|---------|
| `validate_input(args)` / `print_input_error()` | Enforce and describe the generated type's accepted sample sizes. |
| `default_sample_size()` | Return the type-appropriate default reported sample size. |
| `create()` | Allocate a sample using the generated type's capacity. |
| `destroy(s)` | Free the sample. Use `PerftestType_delete()`. |
| `initialize(s)` | Zero all fields (sequence_number=0, kind=DATA, timestamp_us=0). |
| `copy_metadata(dst, src)` | Copy `sequence_number`, `kind`, `timestamp_us` from src to dst. Do NOT copy payload data. Used by subscriber echo. |
| `copy_for_write(dst, src)` | Copy metadata + logical data length. Used by loan-based writers (Zero Copy). For non-loan types, can be identical to `copy_metadata`. |
| `set_kind(s, k)` / `get_kind(s)` | Write/read the `kind` field. Cast between `PerftestTypeKind` and `PerftestKind`. |
| `set_sequence_number(s, n)` / `get_sequence_number(s)` | Write/read `sequence_number`. |
| `set_timestamp_us(s, t)` / `get_timestamp_us(s)` | Write/read `timestamp_us`. |
| `set_sample_size(s, size)` / `get_sample_size(s)` | Convert between the reported sample size and the type's payload representation. |
| `raw(s)` | Return `&s->impl` as `void*` (the generated type pointer for DDS writes). |
| `type_name()` | Return `PerftestTypeTYPENAME` (generated constant). |

**Template:**

```c
#include "type_interface.h"
#include "../core/perftest_lite.h"
#include "rti_me_c.h"
#include "perftest.h"
#include "perftestSupport.h"
#include <stdlib.h>

struct PerftestSample {
    PerftestType impl;
};

static int type_validate_input(const PerftestLiteInputArgs *args)
{
    return args->datalen >= PERFTEST_TYPE_OVERHEAD
        && args->datalen <= PERFTEST_TYPE_OVERHEAD
            + PERFTEST_TYPE_MAX_PAYLOAD ? 0 : -1;
}

static int32_t type_default_sample_size(void)
{
    return PERFTEST_TYPE_OVERHEAD + PERFTEST_TYPE_MAX_PAYLOAD;
}

static void type_print_input_error(void)
{
    /* Print the accepted range using the generated constants. */
}

static PerftestSample *type_create(void)
{
    return (PerftestSample *)PerftestType_create();
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
    /* Initialize your payload-size mechanism here */
    return 0;
}

static int type_copy_metadata(PerftestSample *dst, const PerftestSample *src)
{
    dst->impl.sequence_number = src->impl.sequence_number;
    dst->impl.kind            = src->impl.kind;
    dst->impl.timestamp_us    = src->impl.timestamp_us;
    return 0;
}

static int type_copy_for_write(PerftestSample *dst, const PerftestSample *src)
{
    type_copy_metadata(dst, src);
    /* Also copy data_length or equivalent */
    return 0;
}

static void type_set_kind(PerftestSample *s, PerftestTypeKind k)
{ s->impl.kind = (PerftestKind)k; }

static PerftestTypeKind type_get_kind(const PerftestSample *s)
{ return (PerftestTypeKind)s->impl.kind; }

static void type_set_seq(PerftestSample *s, int32_t n)
{ s->impl.sequence_number = n; }

static int32_t type_get_seq(const PerftestSample *s)
{ return s->impl.sequence_number; }

static void type_set_ts(PerftestSample *s, uint64_t t)
{ s->impl.timestamp_us = t; }

static uint64_t type_get_ts(const PerftestSample *s)
{ return s->impl.timestamp_us; }

static int type_set_sample_size(PerftestSample *s, int32_t size)
{
    int32_t payload_size = size - PERFTEST_TYPE_OVERHEAD;
    /* Validate and apply payload_size using the generated representation. */
    (void)s; (void)payload_size;
    return 0;
}

static int32_t type_get_sample_size(const PerftestSample *s)
{ return s->impl.data_length + PERFTEST_TYPE_OVERHEAD; }

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
```

### Step 3 — Register the variant in CMake

Edit `CMakeLists.txt` to add your variant to the type selection logic:

```cmake
if(PERFTEST_LITE_TYPE STREQUAL "myvariant")
    set(IDL_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/idl/myvariant/perftest.idl)
  set(TYPE_SRC_FILE src/type/type_myvariant.c)
elseif(PERFTEST_LITE_TYPE STREQUAL "zcopy")
    set(IDL_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/idl/zcopy/perftest.idl)
  set(TYPE_SRC_FILE src/type/type_zcopy.c)
else()
    set(IDL_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/idl/sequence/perftest.idl)
  set(TYPE_SRC_FILE src/type/type_default.c)
endif()
```

The build system passes the selected IDL directly to `rtiddsgen`. Generated
file names remain `perftest.h`, `perftest.c`, etc. because every selected
source file is named `perftest.idl`.

### Step 4 — Build and test

```bash
rm -rf build && mkdir build && cd build
cmake .. \
    -DRTIMEHOME=/path/to/micro \
    -DRTIME_TARGET_NAME=x86_64leElfgcc13.3.0 \
    -DRTIME_TARGET_PSL=x86_64leElfgcc13.3.0-Linux6 \
    -DPERFTEST_LITE_TYPE=myvariant
cmake --build .
```

Run a smoke test:

```bash
# Terminal 1
./perftest_lite_subscriber -sub -domain 200 -latencyTest

# Terminal 2
./perftest_lite_publisher -pub -domain 200 -latencyTest -exec 5
```

Verify:
- Publisher prints latency (avgLat > 0).
- Subscriber prints throughput (Mbps > 0).
- No crashes, no DDS errors in logs.

---

## What You Do NOT Need to Change

| Component | Why it's unchanged |
|-----------|-------------------|
| `src/core/perftest_lite_pub.c` | Uses only `PerftestLiteTypeInterface` |
| `src/core/perftest_lite_sub.c` | Uses only `PerftestLiteTypeInterface` |
| `src/core/perftest_lite_args.c` | CLI is type-agnostic |
| `src/core/perftest_lite_main.c` | Dispatches to pub/sub without type knowledge |
| `src/os/os_*.c` | OS layer has no type dependency |
| `src/render/render_*.c` | Renders results, not samples |
| `src/middleware/middleware_micro.c` | Uses `type_iface->raw()` and generated `PerftestTypeDataWriter_write()` |

The middleware **may** need changes only if your type requires special write
semantics (e.g., loaned samples for Zero Copy). For standard copy-on-write
types, the middleware works as-is.

---

## When the Middleware Needs Changes

If your type variant requires a non-standard write path, you need to extend
`middleware_micro.c`. Examples:

| Scenario | What to add |
|----------|-------------|
| DDS loans (Zero Copy) | Loan-aware writer that calls `DDS_DataWriter_get_loan()` + `copy_for_write()` |
| Special transport | Transport registration and participant QoS configuration |
| Custom serialization | Reader/writer hooks |

Gate new middleware code behind a compile-time define (e.g.,
`PERFTEST_LITE_HAS_MICRO_ZEROCOPY`) so it doesn't affect other variants.

---

## Existing Type Variants as Reference

| Variant | IDL | Type source | Key characteristic |
|---------|-----|-------------|-------------------|
| `sequence` (default) | `idl/sequence/perftest.idl` | `src/type/type_default.c` | Variable-length payload via bounded `sequence<octet>`. Compatible with UDPv4/SHMEM. |
| `zcopy` | `idl/zcopy/perftest.idl` | `src/type/type_zcopy.c` | Fixed-size array + `@transfer_mode(SHMEM_REF)`. Compatible with Micro Zero Copy v2. Requires loan-aware writer. |

---

## Checklist

Before considering your type variant complete:

- [ ] IDL declares `struct PerftestType` and `enum PerftestKind` with correct names.
- [ ] Type source implements all `PerftestLiteTypeInterface` functions.
- [ ] `perftest_lite_type_get()` returns your interface.
- [ ] CMake selects your IDL and type source via `PERFTEST_LITE_TYPE=<name>`.
- [ ] Generated code produces `perftest.h` with `PerftestType` and `PerftestTypeTYPENAME`.
- [ ] Project compiles cleanly (`-Wall -Wextra`).
- [ ] Latency test works (publisher receives pongs, reports latency > 0).
- [ ] Throughput test works (subscriber receives samples, reports Mbps > 0).
- [ ] Other type variants still build and pass their tests (no regressions).
- [ ] No core source file includes your type-specific headers directly.

---

## Common Pitfalls

1. **Wrong type name**: If your IDL uses a name other than `PerftestType`, the
   middleware won't compile (`PerftestTypeDataWriter_write` won't exist).

2. **Wrong enum name/values**: The engine uses `PERFTEST_KIND_DATA_PING` etc.
   to determine behavior. If your enum doesn't map correctly, pings won't be
   echoed and latency measurement breaks.

3. **Forgetting `copy_for_write`**: If your variant is used with a loan-based
   writer and `copy_for_write` doesn't copy the payload size, the subscriber
   will see `data_length=0` and report 0 throughput.

4. **Stale generated code**: After switching between type variants, use a
    clean or variant-specific build directory. The generated `perftest.h` from
    a previous variant will cause hard-to-debug struct layout mismatches.

5. **Binary payload copy in `copy_metadata`**: Never copy `bin_data` (array or
   sequence contents) in `copy_metadata` — the subscriber calls this on every
   echoed ping, and copying 64KB per echo destroys latency performance.
