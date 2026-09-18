# FreeRTOS and lwIP integration example

This directory is a customer-owned build example for integrating one Perftest
Lite role into an existing FreeRTOS/lwIP firmware. It does not require
proprietary or internal build tooling.
It uses RTI OSAPI for sleep and semaphores, lwIP for IPv4 interface lookup, and
two hooks supplied by the board support package.

## Prerequisites

The firmware project must already provide:

- A working FreeRTOS and lwIP port.
- RTI Connext DDS Micro libraries built for that port.
- A C99 compiler and an existing firmware CMake target.
- A hardware timer suitable for monotonic microsecond measurements.

The example is intentionally board-neutral. It cannot initialize clocks,
Ethernet, the scheduler, or the hardware timer because those operations belong
to the board support package.

## 1. Implement the board hooks

Implement the declarations from `platform_config.h` in the board support
package:

```c
#include <stdarg.h>
#include <stdint.h>
#include "platform_config.h"

uint64_t perftest_lite_platform_time_now_us(void)
{
    return board_monotonic_time_now_us();
}

int perftest_lite_platform_printf(const char *format, ...)
{
    int result;
    va_list arguments;

    va_start(arguments, format);
    result = board_vprintf(format, arguments);
    va_end(arguments);
    return result;
}
```

The timer must be monotonic, have microsecond resolution, and remain correct
across hardware-counter wraparound for the longest test. Initialize it before
calling `perftest_lite_example_run()`.

Edit the NIC, peer, domain, payload, and duration defaults in
`platform_config.h`, or override them with compiler definitions.

## 2. Add one role to the firmware target

After creating the firmware target, set the integration variables and call the
helper. This example builds a DPDE publisher with a 1472-byte maximum payload:

```cmake
add_executable(firmware
  board_startup.c
  board_perftest_hooks.c)

set(PERFTEST_LITE_ROOT "/path/to/rti-perftest-lite")
set(PERFTEST_LITE_RTIMEHOME "/path/to/rti_connext_dds_micro-4.3.0")
set(PERFTEST_LITE_RTIME_TARGET "<micro-target-library-directory>")
set(PERFTEST_LITE_RTIME_TARGET_PSL "<psl-target-library-directory>")
set(PERFTEST_LITE_RTI_OS_DEFINE RTI_FREERTOS)
set(PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES 1472)
set(PERFTEST_LITE_DISCOVERY DPDE)

include("${PERFTEST_LITE_ROOT}/examples/embedded/freertos_lwip/perftest_lite_sources.cmake")
perftest_lite_add_embedded_role(firmware PUB)
```

Use `SUB` instead of `PUB` for subscriber firmware. Build separate firmware
images for the two roles. The helper generates the DDS type, adds the exact
role-specific source set, defines the custom OS and platform configuration,
and links the UDP Micro libraries in dependency order.

The firmware target must separately link its FreeRTOS, lwIP, board, C runtime,
and toolchain support libraries. For a non-GNU linker, preserve the listed
Micro archive order and apply the toolchain's equivalent static-library group
or rescan mechanism when required.

## 3. Call Perftest Lite from an application task

After network initialization and after the selected lwIP interface is up, call:

```c
extern int perftest_lite_example_run(void);

void perftest_task(void *argument)
{
    (void)argument;
    (void)perftest_lite_example_run();
    vTaskDelete(NULL);
}
```

Allocate enough task stack for the selected Micro configuration and measure its
high-water mark on the target. Do not call the benchmark from an lwIP callback
or middleware receive thread.

## 4. Bring up and validate

1. Start the embedded subscriber and confirm that it waits for a publisher.
2. Start a Linux publisher with the same domain, peer, type bound, discovery,
   reliability, and transport selections.
3. Repeat with the embedded publisher and Linux subscriber.
4. Test best-effort throughput, reliable throughput, and strict latency.
5. Verify timer monotonicity, semaphore wakeup, payload rejection, heap
   lifecycle, task-stack high-water marks, and DDS pool limits.

If discovery does not complete, first verify the peer address, interface name,
IPv4 byte order, DPDE/DPSE selection, and that both generated types use the same
payload bound.

## Linux contract simulation

The repository includes a Linux simulation of the customer-owned integration
path under `test/custom_port_sim`. It uses a separate custom OS adapter and
platform hooks, builds through `perftest_lite_add_embedded_role()`, and launches
the role-specific direct API executables over loopback:

```bash
cmake -S test/custom_port_sim -B build/custom-port-sim \
    -DRTIMEHOME="$RTIMEHOME" \
    -DRTIME_TARGET_NAME="$RTIME_TARGET_NAME" \
    -DRTIME_TARGET_PSL="$RTIME_TARGET_PSL"
cmake --build build/custom-port-sim --parallel
ctest --test-dir build/custom-port-sim --output-on-failure
```

This verifies platform configuration injection, custom OS selection, IDL
generation and regeneration, role-specific source selection, Micro archive
link order, discovery, and direct `perftest_lite_main()` execution. It does not
verify the FreeRTOS compiler or ABI, lwIP scheduling and driver behavior,
hardware timer accuracy or wraparound, firmware memory limits, or target task
stack usage. Those checks must still run on the target.
