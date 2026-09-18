# Perftest Lite

Perftest Lite is a C99 performance analysis tool for measuring throughput and latency across different communication backends. It is designed for small systems and can run as two host executables or be integrated into an application through its C API.

Copyright (c) 2025-2026 Real-Time Innovations, Inc.

## License and dependencies

Perftest Lite is licensed under the Eclipse Public License 1.0. RTI Connext DDS Micro and other platform dependencies are not included in this repository and remain subject to their respective license terms. Users must obtain those dependencies separately and comply with their applicable licenses. The Perftest Lite license does not grant rights to those dependencies.

## What it measures

A test requires one publisher and one subscriber. The publisher sends data samples on the ping topic; the subscriber counts throughput and echoes pings on the pong topic. Publisher output contains latency metrics; subscriber output contains throughput and detected loss.

The project currently provides:

- Backend-neutral benchmark core with a current RTI Connext DDS integration.
- Architecture intended to support raw UDP and other middleware backends in future versions.
- `sequence` (default) and fixed-array `zcopy` type variants.
- UDPv4 in every build; optional serialized SHMEM for sequence builds and Zero Copy v2 for zcopy builds.
- DPSE discovery by default and an opt-in DPDE build.
- Separate publisher and subscriber executables and static libraries.
- Linux and QNX OS adapters.

It is not the same product as RTI Perftest. Perftest Lite has a deliberately narrower topology and build-time feature set.

## Quick start: Linux UDPv4

This Linux UDPv4 example has been tested with a licensed RTI Connext DDS Micro 4.3.0 installation, which is not included in this repository. Set `RTIMEHOME` to that installation root. The target values below are for a Linux x86_64 GNU target; use the target names from your installation if they differ.

```bash
cmake -S . -B build \
    -DRTIMEHOME=/path/to/rti_connext_dds_micro-4.3.0 \
    -DRTIME_TARGET_NAME=x86_64leElfgcc12.3.0 \
    -DRTIME_TARGET_PSL=x86_64leElfgcc12.3.0-Linux5 \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

In one terminal, start the subscriber:

```bash
./build/perftest_lite_subscriber -sub
```

In another terminal, start a short throughput test:

```bash
./build/perftest_lite_publisher -pub 
```

For strict ping-pong latency, add `-latencyTest` to both commands. The processes wait for matching endpoints; verify domain, transport, NIC, peer, and type build if either process does not start a test.

## Documentation

- [Building](doc/building.md)
- [CLI reference](doc/cli-reference.md)
- [Results and methodology](doc/results.md)
- [Transports and discovery](doc/transports.md)
- [Architecture](doc/architecture.md)
- [Porting guide](doc/porting-guide.md)
- [FreeRTOS/lwIP integration example](examples/embedded/freertos_lwip/README.md)
- [Troubleshooting](doc/troubleshooting.md)


## Run — between two machines

On the **subscriber** host (e.g. 192.0.2.10, NIC `wlp0s20f3`):

```bash
./perftest_lite_subscriber -domain 0 -nic wlp0s20f3 -peer 192.0.2.20
```

On the **publisher** host (e.g. 192.0.2.20, NIC `enp0s31f6`):

```bash
./perftest_lite_publisher -domain 0 -nic enp0s31f6 -peer 192.0.2.10 \
    -datalen 1024 -exec 10 -pubrate 10000
```

## Run — latency mode

Forces ping-pong every sample.

```bash
./perftest_lite_subscriber -domain 0 -nic lo -peer 127.0.0.1
./perftest_lite_publisher  -domain 0 -nic lo -peer 127.0.0.1 \
    -datalen 256 -exec 10 -latencyTest -rel
```

## CLI options

The following is a summary. The generated `--help` output and the
[CLI reference](doc/cli-reference.md) are the authoritative command-line
documentation.

```
-pub | -sub                     Role (defaults from binary name)
-domain <id>                    Domain ID (default 0)
-nic <name>                     Network interface (default lo)
-peer <addr>                    Peer initial address (default 127.0.0.1)
-transport <id>                 Transport id: UDPv4, SHMEM (sequence build), or ZeroCopy (zcopy build)
-datalen <bytes>                Sample size (default 100)
-exec <seconds>                 Execution time (default 10)
-pubrate <samples/s>            Publisher rate (default unlimited)
-latencyTest                    Latency mode (ping-pong every sample)
-latencyCount <n>               Ping every N samples (default 10000)
-rel                            Reliable QoS
-subSleep <us>                  Subscriber loop sleep
-printPeriodic                  Print statistics every second (default off)
-latencyLog <file>              Log per-pong latency samples to a file
-latencyStream                  Stream per-pong latency samples as [LAT] lines
-noPrintConfig                  Suppress configuration banner
-h | --help                     Help
```

## Embedded usage (no CLI)

Build a tiny `main()` that fills `PerftestLiteInputArgs` directly:

```c
#include "perftest_lite.h"

int main(void) {
    PerftestLiteInputArgs args;
    perftest_lite_input_args_init(&args);
    args.role         = PERFTEST_LITE_ROLE_PUBLISHER;
    args.domain_id    = 0;
    args.nic          = "eth0";
    args.peer         = "192.0.2.10";
    args.datalen      = 256;
    args.exec_seconds = 10;

    PerftestLiteSizeResult storage[4];
    PerftestLiteResults results;
    perftest_lite_results_init(&results, storage, 4);

    return perftest_lite_main(&args, &results);
}
```

CMake produces two role-specific static libraries:
`libperftest_lite_pub.a` and `libperftest_lite_sub.a`. Link the archive
corresponding to the embedded application's role. Keeping publisher and
subscriber code in separate archives reduces the footprint of embedded
deployments that only need one role. See the [build artifacts](doc/building.md#artifacts)
for the complete target and linking details.

## Porting

See the [porting guide](doc/porting-guide.md) for the complete platform,
middleware, and type-variant integration details.

To port to a new OS:

1. Add `src/os/os_<platform>.c` implementing the
   `PerftestLiteOsInterface` from `src/os/os_interface.h`.
2. Wire the new file in `CMakeLists.txt` based on `PERFTEST_LITE_OS`.

To port to a new middleware:

1. Add `src/middleware/middleware_<name>.c` implementing the
   `PerftestLiteMiddleware` interface in
   `src/middleware/middleware_interface.h`.
2. Wire it in `CMakeLists.txt` based on `PERFTEST_LITE_MIDDLEWARE`.

To use a different IDL variant (e.g., ZeroCopy arrays):

1. Add a new IDL file using the same `PerftestType` name.
2. Add a `src/type/type_<variant>.c` implementing
   `PerftestLiteTypeInterface`.


