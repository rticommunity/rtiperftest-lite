---
name: port-perftest-lite
description: "Use when porting or integrating Perftest Lite into an embedded OS, RTOS, BSP, vendor IDE, or custom CMake project, including FreeRTOS and lwIP targets."
---

# Port Perftest Lite

Integrate one Perftest Lite role into a user-owned target. Treat
`doc/porting-guide.md` as authoritative and use
`examples/embedded/freertos_lwip/` as the reference implementation when its
assumptions match the target.

## Guardrails

- Use the documented platform interface and application entry point; do not
   depend on internal build infrastructure or generated platform headers.
- Prefer RTI OSAPI for sleep, semaphores, heap, and other OS services when it
  provides an equivalent API.
- Do not guess compiler names, RTI target directory names, RTI OS definitions,
  network-stack APIs, linker group syntax, byte order, or hardware timer APIs.
- Do not claim a port works because it compiles. Require runtime evidence for
  clock behavior, discovery, both roles, throughput, and latency.
- Build exactly one OS adapter, one role, the supplied sequence type adapter,
   one middleware, and one renderer into each firmware image.
- Keep IDL generation flags, generated sources, payload limits, type adapter,
  and peer builds consistent.
- Preserve user changes and existing BSP initialization. Do not replace the
   user's scheduler, network startup, linker script, or startup code.

## 1. Establish target facts

Before editing, locate the firmware target and collect or ask for any missing
facts that cannot be derived from the build:

1. CPU architecture, endianness, ABI, compiler, and linker.
2. OS/RTOS and version.
3. Network stack and how it resolves an interface name to an IPv4 address.
4. Build system and the existing firmware target to extend.
5. Connext DDS Micro installation and exact core and PSL target directories;
   the selected PSL supplies the RTI OS configuration.
6. Publisher or subscriber role; create separate targets if both are needed.
7. DPDE or DPSE discovery.
8. Transport and type variant. Use UDPv4 and `sequence` by default; select
   another option only for a documented requirement that this path cannot meet.
9. Maximum payload, default NIC, peer address, domain, reliability, duration,
    and output destination.
10. Monotonic hardware timer API, frequency, resolution, initialization point,
    and wrap behavior.

If an exact SDK or RTI value is unknown, stop at that dependency and identify
how the user can obtain it. Never substitute a plausible value.

## 2. Choose the integration path

For FreeRTOS with lwIP, start from
`examples/embedded/freertos_lwip/perftest_lite_sources.cmake` and its adjacent
adapter and configuration. For another RTOS or network stack, copy the shape of
that example but replace only the platform operations that differ.

Use `idl/sequence/perftest.idl` with `src/type/type_default.c` by default. This
supports variable payload sizes and avoids implementing a new
`PerftestLiteTypeInterface`. Create or port a type adapter only when the user
has a requirement that the supplied sequence representation cannot meet.

When the user has Connext DDS Micro 4.3.0, reuse
`src/middleware/middleware_micro.c` as the default middleware implementation.
It still requires target-specific Micro and PSL archives plus the selected
feature and discovery definitions. Compile and validate it with the target
toolchain; port the middleware only when API compatibility or a required
feature cannot be verified. For another Micro version, verify the used Micro C
APIs before reusing it and do not assume compatibility from the version number
alone.

For CMake, add sources to the user's existing firmware target. For another
build system, reproduce the source, include, definition, generated-file,
library, and link-order manifest in `doc/porting-guide.md`; do not add CMake as a
runtime dependency.

## 3. Implement the platform boundary

1. Define `PERFTEST_LITE_OS_CUSTOM=1`; use the selected PSL's RTI OS
   configuration rather than adding a guessed OS macro.
2. Implement `PerftestLiteOsInterface` version
   `PERFTEST_LITE_OS_INTERFACE_VERSION`.
3. Return accumulated monotonic microseconds from `time_now_us`; account for
   hardware-counter wrap for the longest run.
4. Implement sleep and semaphores with RTI OSAPI where available.
5. Return the selected NIC IPv4 address and netmask in network byte order, or
   `UINT32_MAX` when either value cannot be resolved.
6. Supply a platform configuration header through
   `PERFTEST_LITE_PLATFORM_CONFIG_HEADER` for output and embedded defaults.
7. In the application entry function, initialize `PerftestLiteInputArgs`,
   initialize `PerftestLiteResults` with caller-owned result storage, and set
   the role and required embedded inputs. Keep assigned strings alive for the
   duration of the call.
8. Call `perftest_lite_main()` from an application task only after the timer,
   scheduler, and network interface are ready. Do not synthesize an empty CLI.

## 4. Integrate type generation and linking

For the default path, generate `idl/sequence/perftest.idl` into a build
directory with the target installation's `rtiddsgen` and these inputs:

```text
-language C -micro -interpreted 0
-D PERFTEST_TYPE_CONFIGURED_MAX_PAYLOAD=<payload-bytes>
```

Compile `perftest.c`, `perftestPlugin.c`, and `perftestSupport.c`; add the
generated directory to includes; and compile `src/type/type_default.c`. Confirm
that generated `perftest.h` reports the intended `PERFTEST_TYPE_OVERHEAD` and
`PERFTEST_TYPE_MAX_PAYLOAD`. Do not duplicate the generated payload bound as a
C compiler definition.

Select one role source and matching `PERFTEST_LITE_BUILD_PUB=1` or
`PERFTEST_LITE_BUILD_SUB=1` definition.

For UDPv4, link the matching `disc<dpde|dpse>`, writer-history, reader-history,
network PSL, OS PSL, and Micro core archives in the exact order documented by
`doc/porting-guide.md`. Apply the target linker's group/rescan feature when
needed. Add optional transport or XTypes archives only when the selected
feature requires them.

## 5. Validate incrementally

Before target validation, create an explicit low-memory profile when the target
is constrained:

1. Build only one role, UDPv4, the sequence type, and one renderer; omit
   optional transports and interpreted type support unless required.
2. Set the generated sequence payload bound to the largest required reported
   sample size minus the 28-byte fixed type overhead. Pass this value to
   `rtiddsgen`; a compiler definition alone does not resize the type.
3. Keep `PERFTEST_LITE_DEFAULT_DATALEN` within the generated bound and set
   `PERFTEST_LITE_INIT_SAMPLE_SIZE` to a valid small sample when the generated
   type cannot hold the default 1234-byte warm-up sample.
4. Set `PERFTEST_LITE_LATENCY_HISTORY_SIZE=0` unless percentiles are required;
   each enabled entry consumes 4 bytes plus allocator overhead.
5. Tune latency and throughput writer/reader history depths and maximum samples
   independently. Keep every depth between 1 and its corresponding maximum.
6. Use one writer instance, one reader instance, and one remote writer only
   when the application retains the documented unkeyed, one-peer topology.
7. Size UDP send/receive buffers for the target stack and bursts. Do not infer
   a safe value from desktop defaults or equate maximum UDP message size with
   Perftest `datalen`.
8. Use a nonzero `PERFTEST_LITE_PUB_MIN_SLEEP_US` when an unlimited publisher
   can starve a cooperative network task; this addresses scheduling, not pool
   memory.

Use the constrained starting profile in `doc/porting-guide.md` as an initial
experiment, not as a universal minimum. Reduce one dimension at a time and
measure firmware sections, peak heap, stack high-water marks, allocation/write
failures, loss, and achieved rate in both latency and throughput modes.

Run the cheapest available functional check after each step:

1. Preprocess or compile the platform configuration and OS adapter.
2. Generate the DDS type and confirm all expected generated files exist.
3. Compile one role without compiling another OS adapter or role.
4. Link and inspect unresolved symbols before adding libraries.
5. On target, verify monotonic clock progression, wrap handling, sleep
   granularity, semaphore timeout/signal, and NIC address bytes.
6. Start the embedded subscriber against a compatible Linux publisher.
7. Reverse the roles.
8. Run UDPv4 best-effort throughput, reliable throughput, and strict latency.
9. Test payloads below the generated overhead and above the generated bound,
   and test unavailable transports.
10. Record firmware size, heap lifecycle, task-stack high-water marks, DDS pool
    limits, exact build definitions, and generated-type bound.
11. When percentile history is disabled, verify that zero percentile fields are
   treated as unavailable placeholders rather than measured zero latency.

When hardware is unavailable, clearly distinguish completed compile/link checks
from pending target validation. Do not report the port as complete.

## 6. Report the result

Summarize:

- Files added or changed.
- Exact target, role, discovery, transport, type, and payload selections.
- Commands or IDE actions used to generate, compile, and link.
- Validation that passed and its evidence.
- Target-only validation still required.
- Any user-provided values or proprietary SDK components that remain
   unresolved.
