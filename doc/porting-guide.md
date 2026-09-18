# Porting Guide

## Purpose and scope

Perftest Lite is intended to be straightforward to adapt to a new platform.
This guide describes the public integration path for users who need to build
Perftest Lite for a specific embedded or hosted platform.

The repository already provides CMake build paths and OS adapters for Linux and
QNX. Start with [Building](building.md) when either platform matches the target.
For another platform, use the steps below to add the required OS, toolchain,
and RTI Connext DDS Micro integration. A new port is not a supported platform
claim until it has passed the validation checklist in this document.

## Choose an integration path

Use the path that matches the target:

| Target | Starting point | Work required |
| --- | --- | --- |
| Linux | Existing CMake configuration | Set the RTI installation and target-library variables. |
| QNX | Existing CMake configuration with `PERFTEST_LITE_OS=qnx` | Provide the QNX compiler environment and matching RTI target libraries. |
| Other hosted OS | Existing CMake configuration as a reference | Add an OS adapter, platform compile definitions, and link libraries. |
| Embedded or RTOS application | Public source and C API | Integrate one role into the application, provide platform services, generate the DDS type, and link the selected RTI libraries. |

For all paths, the publisher and subscriber must use compatible generated type,
transport, discovery, and middleware settings.

Before changing code, record these target facts. They determine the port and
make build failures reproducible:

| Input | Example | Why it matters |
| --- | --- | --- |
| CPU, endianness, and ABI | Cortex-M7, little-endian EABI | Selects the compiler and Micro archives. |
| OS and version | FreeRTOS 10 | Selects the Micro OS definition and platform services. |
| Network stack | lwIP 2 | Determines NIC lookup and platform libraries. |
| Build system | CMake, Make, or vendor IDE | Determines how generated files and source lists are integrated. |
| Micro target directories | `<target>` and `<target>-<psl>` | Selects compatible core and PSL archives. |
| Discovery | DPDE or DPSE | Selects both a definition and a discovery archive. |
| Role | Publisher or subscriber | Avoids linking unused role code. |
| Transport and type | UDPv4 and sequence | Determines generated IDL and optional archives. |
| Maximum payload | 1472 bytes | Controls generated type bounds and DDS memory. |
| Timer source | Hardware counter at 1 MHz or better | Determines whether latency measurements are meaningful. |

## Integrate into an application

Perftest Lite can run without a command line. The application supplies its own
entry point, constructs `PerftestLiteInputArgs`, and calls
`perftest_lite_main()`.

### Link one role

Link one role-specific library into each application:

- Publisher application: `perftest_lite_pub`.
- Subscriber application: `perftest_lite_sub`.

Use the generated IDL sources, generated include directory, compile
definitions, middleware archives, and platform libraries from the equivalent
CMake executable target as the integration reference. There is currently no
installed package or CMake export target.

### Minimal invocation

```c
#include "perftest_lite.h"

int application_main(void)
{
    PerftestLiteInputArgs args;
    PerftestLiteSizeResult storage[1];
    PerftestLiteResults results;

    perftest_lite_input_args_init(&args);
    args.role = PERFTEST_LITE_ROLE_PUBLISHER;
    args.nic = "eth0";
    args.peer = "192.0.2.20";
    args.datalen = 256;
    args.exec_seconds = 10;

    perftest_lite_results_init(&results, storage, 1);
    return perftest_lite_main(&args, &results);
}
```

The caller owns the input arguments, result storage, and the lifetime of
strings assigned to input fields. If `perftest_lite_parse_arguments()` was
used, call `perftest_lite_free_args()` to release its allocated size list. The
engine writes only up to the supplied result capacity; capacity exhaustion is
not currently reported as an execution error.

Repeated invocation in a single process has not yet been established as a
supported lifecycle. Run one benchmark per application process until lifecycle
tests and documentation define otherwise.

### FreeRTOS and lwIP reference

The [FreeRTOS/lwIP integration example](../examples/embedded/freertos_lwip/README.md)
is the reference for a user-owned, self-contained embedded build. It provides a
complete OS adapter, direct application entry function,
platform configuration header, IDL generation, source selection, definitions,
and Micro archive order. The board support package supplies only a monotonic
microsecond clock and formatted-output hook.

Use the example as a source integration, not as a prebuilt library. Build the
publisher and subscriber as separate firmware images so each image contains
only its role-specific engine.

Run the example's [Linux contract simulation](../examples/embedded/freertos_lwip/README.md#linux-contract-simulation)
before moving to hardware. It exercises the customer build path and both roles
without claiming to simulate the FreeRTOS scheduler, lwIP driver, target ABI,
hardware timer, or embedded memory constraints.

### AI-assisted porting

The repository includes an optional
[Perftest Lite porting skill](../.github/skills/port-perftest-lite/SKILL.md) for
AI coding assistants that support repository skills. Ask the assistant to port
or integrate Perftest Lite into the target RTOS, BSP, vendor project, or custom
build. The skill guides the assistant through target discovery, OS adapter
implementation, type generation, source and library selection, low-memory
tuning, and staged validation.

The skill does not replace this guide or target expertise. It is instructed not
to guess compiler, RTI target, linker, network-stack, byte-order, or hardware
timer details. Review every generated change and complete the hardware checks
in this guide before treating a port as working. The porting guide is the
authoritative technical contract when it and an AI response differ.

## Reproduce the build contract

A non-CMake build must reproduce the following inputs. Do not compile every
file under `src`: doing so selects multiple OS implementations and both roles.

| Category | Required inputs for sequence UDPv4 |
| --- | --- |
| Common core | `perftest_lite_args.c`, `perftest_lite_main.c` |
| One role | `perftest_lite_pub.c` or `perftest_lite_sub.c` |
| Middleware | `middleware_micro.c` |
| Type | `type_default.c` |
| OS | Exactly one `os_<platform>.c` implementation |
| Renderer | `render_dispatch.c` and exactly one renderer implementation |
| Generated type | `perftest.c`, `perftestPlugin.c`, `perftestSupport.c` |
| Includes | Micro `include` and `include/rti_me`, generated directory, and each selected Perftest Lite source directory |
| Role definition | `PERFTEST_LITE_BUILD_PUB=1` or `PERFTEST_LITE_BUILD_SUB=1` |
| OS definitions | The matching RTI OS definition and `PERFTEST_LITE_OS_CUSTOM=1` for a customer adapter |
| Renderer definition | `PERFTEST_LITE_HAS_RENDER_TABLE=1` or `PERFTEST_LITE_HAS_RENDER_CSV=1` |
| Discovery definition | `RTI_DPDE=1` or `RTI_DPSE=1` |

For a release UDPv4/DPDE build, link the matching `z` archives in this order:

```text
librti_me_discdpdez.a
librti_me_whsmz.a
librti_me_rhsmz.a
librti_me_netiopslz.a
librti_me_ospslz.a
librti_mez.a
```

Use `discdpse` for DPSE and the target's debug suffix when building against
debug Micro libraries. Some static linkers require a library group or rescan
around this list. Optional SHMEM, Zero Copy, and interpreted-type builds add
archives and definitions; reproduce the corresponding selection from the root
`CMakeLists.txt` rather than adding every available archive.

## Tune a low-memory target

Memory-constrained ports should start with one role, UDPv4, the sequence type,
one renderer, and no optional SHMEM, Zero Copy, interpreted-type, or latency
history features. Then tune the generated type, DDS pools, and socket buffers
for the largest workload the target must run.

These definitions have the largest direct effect:

| Definition | Memory or behavior controlled | Tuning rule |
| --- | --- | --- |
| `PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES` | Generated sequence capacity used by each allocated sample | Set it to the largest required reported `datalen` minus the 28-byte type overhead. Regenerate the IDL after changing it. |
| `PERFTEST_LITE_ZCOPY_DATALEN` | Fixed generated array size for a Zero Copy image | Build one image per required reported size. Prefer the sequence type when variable sizes are required. |
| `PERFTEST_LITE_DW_MAX_SAMPLES_*` | Maximum samples reserved for each writer | Reduce together with the corresponding history depth. Reliable throughput generally needs more than latency mode. |
| `PERFTEST_LITE_DR_MAX_SAMPLES_*` | Maximum samples reserved for each reader | Keep enough receive samples to absorb scheduler and network bursts; exhaustion can reduce throughput or lose data. |
| `PERFTEST_LITE_DW_HISTORY_DEPTH_*` | Writer `KEEP_LAST` depth; reliable non-Zero-Copy writers use `max_samples` with `KEEP_ALL` | Must be at least 1 and no greater than writer maximum samples. |
| `PERFTEST_LITE_DR_HISTORY_DEPTH_*` | Reader `KEEP_LAST` depth | Must be at least 1 and no greater than reader maximum samples. |
| `PERFTEST_LITE_DW_MAX_INSTANCES` | Writer instance capacity and multiplier for total writer samples | The current unkeyed, one-peer topology normally needs 1. |
| `PERFTEST_LITE_DR_MAX_INSTANCES` | Reader instance capacity and multiplier for total reader samples | The current unkeyed, one-peer topology normally needs 1. |
| `PERFTEST_LITE_DR_MAX_REMOTE_WRITERS` | Remote writers tracked by each reader | Use 1 for the documented one-publisher topology. Increase only for a changed topology. |
| `PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE` | Requested UDP receive socket buffer | Reduce to a value supported by the network stack, but retain room for bursts and reliable repair traffic. |
| `PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE` | Requested UDP send socket buffer | Reduce to a value supported by the network stack; too small can cause send failures under load. |
| `PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE` | Maximum UDP transport message accepted by Micro | Keep it compatible with the network MTU and the largest serialized sample. This is not the same as `datalen`. |
| `PERFTEST_LITE_LATENCY_HISTORY_SIZE` | Publisher percentile history | Each entry is 4 bytes, excluding allocator overhead. Use 0 to disable percentiles. |

The following values affect operation but are not primary DDS reservations:

- `PERFTEST_LITE_DEFAULT_DATALEN` selects the reported runtime size. For the
   sequence type it must be between 28 and
   `28 + PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES`.
- `PERFTEST_LITE_INIT_SAMPLE_SIZE` should fit the generated type. A small value
   such as 32 avoids requesting the desktop-oriented 1234-byte warm-up sample.
- `PERFTEST_LITE_INIT_BURST` controls warm-up traffic and transient queue
   pressure. Lower it only after verifying discovery and first-sample behavior.
- `PERFTEST_LITE_PUB_MIN_SLEEP_US` does not reserve memory, but a nonzero value
   can prevent an unlimited publisher from starving lwIP or another cooperative
   network task.
- `PERFTEST_LITE_DEFAULT_EXEC_SECONDS`, domain, peer, and print defaults do not
   materially reduce DDS pool memory.

### Constrained starting profile

This profile is a starting point for a 256-byte reported sample, not a support
claim or a minimum. It uses a 228-byte generated payload because the type adds
28 bytes of fixed fields:

```cmake
set(PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES 228)

target_compile_definitions(firmware PRIVATE
   PERFTEST_LITE_DEFAULT_DATALEN=256
   PERFTEST_LITE_UDP_MAX_MESSAGE_SIZE=1500
   PERFTEST_LITE_UDP_MAX_RECV_BUFFER_SIZE=16384
   PERFTEST_LITE_UDP_MAX_SEND_BUFFER_SIZE=8192
   PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY=2
   PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY=4
   PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY=2
   PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY=4
   PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT=8
   PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT=8
   PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT=16
   PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT=16
   PERFTEST_LITE_DW_MAX_INSTANCES=1
   PERFTEST_LITE_DR_MAX_INSTANCES=1
   PERFTEST_LITE_DR_MAX_REMOTE_WRITERS=1
   PERFTEST_LITE_LATENCY_HISTORY_SIZE=0
   PERFTEST_LITE_INIT_SAMPLE_SIZE=32
   PERFTEST_LITE_PUB_MIN_SLEEP_US=1)
```

Apply the payload setting before calling `perftest_lite_add_embedded_role()` so
the same value reaches `rtiddsgen`. Compiler-defining
`PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES` does not resize an already generated
type.

Build and run latency and throughput separately because they select different
pools. For each mode, record firmware sections, peak heap, task-stack high-water
marks, allocation failures, write failures, loss, and achieved throughput.
Reduce one dimension at a time and repeat reliable and best-effort tests at the
maximum payload and publication rate. A configuration that starts successfully
but loses samples or cannot sustain the required rate is undersized.

## Add a new OS adapter

The core benchmark depends on the platform only through
`PerftestLiteOsInterface` in `src/os/os_interface.h`. Create
`src/os/os_<platform>.c` and implement the following services:

- A monotonic accumulated clock in microseconds. Its epoch is arbitrary, but
   it must remain correct across counter wrap for the longest run.
- Sleep for at least the requested microseconds, rounded according to the
   scheduler's actual resolution.
- Semaphore creation, timed and infinite wait, signal, and deletion. Take and
   give return a positive value on success and a non-positive value otherwise;
   a negative timeout means wait indefinitely.
- IPv4 address and netmask lookup for a named network interface, returned in
   network byte order. Return `UINT32_MAX` when either value cannot be resolved.

`PERFTEST_LITE_OS_INTERFACE_VERSION` identifies the source-level adapter
contract. A customer adapter should fail its build if it requires a different
version.

When the selected DDS middleware provides RTI OSAPI, prefer its facilities for
timing, sleep, semaphores, heap operations, and other operating-system
services whenever an equivalent facility is available. Use target APIs where
the middleware does not provide the required operation, such as a
platform-specific network-interface lookup.

Add the adapter to the target build system, define
`PERFTEST_LITE_OS_CUSTOM=1` and the corresponding RTI platform macro, and link
only the platform libraries required by that target. Define
`PERFTEST_LITE_PLATFORM_CONFIG_HEADER` as a quoted header name when the port
needs to provide output hooks or defaults before `perftest_lite_config.h`
applies its fallbacks. The Linux, QNX, and FreeRTOS/lwIP adapters are working
examples of this boundary; they are not a requirement to use their specific
APIs or toolchains.

## Generate and link the DDS type

Each build generates exactly one selected IDL type using `rtiddsgen`. Keep the
generated sources, the selected type implementation, and their compile-time
payload bound consistent:

- `sequence` supports variable logical payload sizes up to its configured
  maximum.
- `zcopy` uses a fixed payload array and requires
   `PERFTEST_LITE_ZCOPY_DATALEN` to be set to the reported sample size, including
   the fixed type overhead.

### Generate the IDL sources

The CMake build performs this generation automatically. When integrating with
another build system, invoke `rtiddsgen` once for the selected type and compile
the generated C sources with the application. Create a generation directory
outside the source IDL directory, then run the command that matches the selected
type.

For the default bounded-sequence type:

```bash
mkdir -p gen
"$RTIMEHOME/rtiddsgen/scripts/rtiddsgen" \
   -language C -micro -interpreted 0 \
   -D PERFTEST_TYPE_CONFIGURED_MAX_PAYLOAD=64000 \
   -d gen idl/sequence/perftest.idl
```

For the fixed-array type used by a Zero Copy build:

```bash
mkdir -p gen
"$RTIMEHOME/rtiddsgen/scripts/rtiddsgen" \
   -language C -micro -interpreted 0 \
   -D PERFTEST_TYPE_CONFIGURED_DATALEN=64028 \
   -d gen idl/zcopy/perftest.idl
```

The generator input is the single source for type sizing. Each IDL emits public
constants into `perftest.h`; its type adapter consumes those constants for
validation, defaults, allocation, and reported-sample-size conversion. Do not
duplicate the configured bound as a C compiler definition.

Add `gen` to the include path and compile these generated sources with the
selected role and platform sources:

```text
gen/perftest.c
gen/perftestPlugin.c
gen/perftestSupport.c
```

The selected IDL file is always named `perftest.idl`, so the generated API and
file names remain `perftest.*`, regardless of the type variant. Regenerate the
sources whenever the selected IDL file, its payload bound, the target Micro
version, or generator options change. Do not mix generated sources from two
type variants in the same build directory.

Use the selected CMake target as the source of truth for the required
`rtiddsgen` invocation, generated files, include paths, RTI libraries, and
compile definitions. If the target uses another build system, reproduce those
inputs there rather than copying a host binary. See [Building](building.md) and
[Transports and discovery](transports.md) for compatible build selections.

## Bring up a new platform

Work through these stages in order:

1. **Compile:** Build the generated type, one adapter, one renderer, and one
   role. Treat duplicate `perftest_lite_os_get()` or role symbols as a source
   selection error, not something to suppress in the linker.
2. **Link:** Resolve the selected Micro and PSL archives. Preserve archive
   order and apply the linker-specific group/rescan mechanism when required.
3. **Verify services:** Test the clock across hardware-counter wrap, measure
   sleep granularity, exercise timed and infinite semaphore waits, and confirm
   NIC lookup returns the expected address bytes.
4. **Start the subscriber:** Confirm it reaches its discovery wait without an
   allocation, registry, transport, or interface error.
5. **Interoperate with Linux:** Use a Linux peer with the same domain, peer,
   generated type bound, discovery, transport, and reliability selection.
6. **Reverse roles:** Test an embedded publisher against a Linux subscriber so
   both endpoint and callback paths are covered.
7. **Exercise transport:** Test UDPv4 best-effort throughput, reliable
   throughput, and strict latency before enabling optional transports.
8. **Validate limits:** Test invalid sizes and unavailable transports, then
   measure static memory, heap lifecycle, task-stack high-water marks, and
   endpoint-pool requirements on the target.

Do not copy desktop queue depths, socket buffers, payload bounds, or task-stack
sizes blindly to a constrained target. Select and validate them for the target
workload, maximum payload, transport, and reliability mode.

## Extend the type or middleware

The advanced [type-variant guide](adding_a_type_variant.md) describes the
generated-type boundary. A new middleware integration must implement the
middleware interfaces and be validated independently for discovery, transport,
resource limits, callback or thread behavior, errors, and cleanup.

Do not claim a new OS, middleware, transport, or type as supported solely
because it compiles. Record the exact build configuration and test both roles
with compatible peers before relying on the port for benchmark results.
