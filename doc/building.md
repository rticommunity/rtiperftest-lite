# Building Perftest Lite

CMake is used to configure and build Perftest Lite for the supported
general-purpose operating systems: Linux and QNX. This section shows direct
CMake configurations for common transport and memory choices. If you want to
build for another operating system, see the [porting guide](porting-guide.md)
for the required OS adapter, toolchain, and platform-library integration steps.

## CMake configuration

Users can define their own CMake configuration for Linux, QNX, or another
supported platform. The build configuration is selected at compile time through
the variables below. `RTIMEHOME`, `RTIME_TARGET_NAME`, and `RTIME_TARGET_PSL`
are required.

| Variable | Purpose |
| --- | --- |
| `RTIMEHOME` | RTI Connext DDS installation root. |
| `RTIME_TARGET_NAME` | RTI Connext DDS target-library directory name. |
| `RTIME_TARGET_PSL` | Platform-support-library target directory name. |

The examples below use these shell variables. Replace them with the RTI
installation and target names for your platform:

```bash
RTIMEHOME=/path/to/rti_connext_dds_micro-4.3.0
RTIME_TARGET_NAME=x86_64leElfgcc13.3.0
RTIME_TARGET_PSL=x86_64leElfgcc13.3.0-Linux6
```

For QNX, use the same configuration and set `PERFTEST_LITE_OS=qnx`, together with
the appropriate QNX target names and compiler environment:

```bash
cmake -S . -B build-qnx \
  -DRTIMEHOME=/path/to/rti_connext_dds_micro-4.3.0 \
  -DRTIME_TARGET_NAME=armv8leElfqnx_qcc8.3.0 \
  -DRTIME_TARGET_PSL=armv8leElfqnx_qcc8.3.0-QNX7.1 \
  -DPERFTEST_LITE_OS=qnx
cmake --build build-qnx
```

## Build examples

### 1. Build UDPv4

The default sequence type with serialized SHMEM disabled provides a UDPv4
build:

Definition added: `PERFTEST_LITE_ENABLE_SHMEM=OFF` restricts the default
sequence build to UDPv4.

```bash
cmake -S . -B build-udp \
  -DRTIMEHOME="$RTIMEHOME" \
  -DRTIME_TARGET_NAME="$RTIME_TARGET_NAME" \
  -DRTIME_TARGET_PSL="$RTIME_TARGET_PSL" \
  -DPERFTEST_LITE_ENABLE_SHMEM=OFF
cmake --build build-udp
```

### 2. Build SHMEM

Serialized SHMEM is enabled by default for the default sequence-type build
when the Micro SHMEM header and library are present:

No feature definitions are added; this example relies on the default
`PERFTEST_LITE_TYPE=sequence` and `PERFTEST_LITE_ENABLE_SHMEM=ON` settings.

```bash
cmake -S . -B build-shmem \
  -DRTIMEHOME="$RTIMEHOME" \
  -DRTIME_TARGET_NAME="$RTIME_TARGET_NAME" \
  -DRTIME_TARGET_PSL="$RTIME_TARGET_PSL"
cmake --build build-shmem
```

Run both role executables with `-transport SHMEM`. SHMEM is a serialized
transport and is distinct from Zero Copy.

### 3. Build Zero Copy

Zero Copy uses the fixed-array type and requires the Micro Zero Copy, SHMEM,
and SDM libraries and headers:

Definitions added: `PERFTEST_LITE_TYPE=zcopy` selects the fixed-array type,
`PERFTEST_LITE_ENABLE_ZEROCOPY=ON` enables Zero Copy, and
`PERFTEST_LITE_ZCOPY_DATALEN=1024` sets the fixed reported sample size. The
generated payload array is 996 bytes because the sample includes 28 bytes of
fixed type overhead.

```bash
cmake -S . -B build-zerocopy \
  -DRTIMEHOME="$RTIMEHOME" \
  -DRTIME_TARGET_NAME="$RTIME_TARGET_NAME" \
  -DRTIME_TARGET_PSL="$RTIME_TARGET_PSL" \
  -DPERFTEST_LITE_TYPE=zcopy \
  -DPERFTEST_LITE_ENABLE_ZEROCOPY=ON \
  -DPERFTEST_LITE_ZCOPY_DATALEN=1024
cmake --build build-zerocopy
```

Run both role executables with `-transport ZeroCopy`. The compiled sample size
is the runtime default, so `-datalen` may be omitted. Create a separate build
for each required sample size.

### 4. Reduce memory

For a memory-constrained deployment, reduce the generated sequence bound and
the endpoint pools. The values below are an example for a 32-byte workload:

Definitions added: `PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES=32` reduces the
generated sequence bound; the `*_HISTORY_DEPTH_*` and `*_MAX_SAMPLES_*`
definitions set the latency and throughput endpoint pools to one sample;
`PERFTEST_LITE_ENABLE_SHMEM=OFF` removes the optional SHMEM transport; and
`CMAKE_BUILD_TYPE=MinSizeRel` selects a smaller release build. The default
`PERFTEST_LITE_TYPE=sequence` is omitted.

```bash
cmake -S . -B build-small \
  -DRTIMEHOME="$RTIMEHOME" \
  -DRTIME_TARGET_NAME="$RTIME_TARGET_NAME" \
  -DRTIME_TARGET_PSL="$RTIME_TARGET_PSL" \
  -DPERFTEST_LITE_ENABLE_SHMEM=OFF \
  -DPERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES=32 \
  -DPERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY=1 \
  -DPERFTEST_LITE_DW_MAX_SAMPLES_LATENCY=1 \
  -DPERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY=1 \
  -DPERFTEST_LITE_DR_MAX_SAMPLES_LATENCY=1 \
  -DPERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT=1 \
  -DPERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT=1 \
  -DPERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT=1 \
  -DPERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT=1 \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-small
```

The sequence bound and endpoint pool settings reduce DDS memory reservations;
`MinSizeRel` also reduces the executable footprint. The pool values must be
validated against the intended throughput and latency workload because smaller
pools can reduce buffering capacity.

## Other build systems

CMake is the build description used by this repository, but it is not a runtime
requirement. Another build system can invoke CMake with its preferred generator,
for example `cmake -G Ninja`, or reproduce the same steps directly: run
`$RTIMEHOME/rtiddsgen/scripts/rtiddsgen` for the selected IDL and generation
flags, compile the C99 sources and generated sources with the RTI include paths
and CMake definitions, and link the selected role with the matching Micro
archives and platform libraries. The [porting guide](porting-guide.md) lists
the source, generated-type, and platform integration points that a non-CMake
build must provide.

Define `PERFTEST_LITE_BUILD_PUB` or `PERFTEST_LITE_BUILD_SUB` to compile one
role. Define both and compile both role sources to produce one binary where
`-pub` or `-sub` selects the role at runtime; publisher is the default.

## Build selections

| CMake setting | Default | Meaning |
| --- | --- | --- |
| `PERFTEST_LITE_OS` | `linux` | `linux` or `qnx`. |
| `PERFTEST_LITE_MIDDLEWARE` | `micro` | The current RTI Connext DDS backend. Future backends can use this selection. |
| `PERFTEST_LITE_TYPE` | `sequence` | `sequence` or `zcopy`. |
| `PERFTEST_LITE_SEQUENCE_MAX_PAYLOAD_BYTES` | `64000` | Maximum sequence payload bytes. |
| `PERFTEST_LITE_ZCOPY_DATALEN` | `64028` | Fixed zcopy reported sample bytes, including 28 bytes of overhead. |
| `PERFTEST_LITE_ENABLE_SHMEM` | `ON` | Enables serialized SHMEM only when a sequence build finds the required Micro header and archive. |
| `PERFTEST_LITE_ENABLE_ZEROCOPY` | `OFF` | Requires `PERFTEST_LITE_TYPE=zcopy` and the required Micro Zero Copy archives and header. |
| `PERFTEST_LITE_INTERPRETED_TYPE` | `OFF` | Selects interpreted/XTypes generated support. |
| `PERFTEST_LITE_ENABLE_MEMORY_CHECKPOINTS` | `OFF` | Builds OSAPI heap memory checkpoint instrumentation. |
| `RTI_DPSE` / `RTI_DPDE` | `ON` / `OFF` | DPSE is selected unless DPDE is on and DPSE is off. |
| `PERFTEST_LITE_RENDER_TABLE` / `PERFTEST_LITE_RENDER_CSV` | `ON` / `OFF` | Exactly one renderer must be enabled. |

The following optional variables override the generated DDS resource limits. Leave
them empty to use the defaults in `perftest_lite_config.h`:

| Variable | Applies to |
| --- | --- |
| `PERFTEST_LITE_DW_HISTORY_DEPTH_LATENCY` | Latency writer history depth. |
| `PERFTEST_LITE_DW_MAX_SAMPLES_LATENCY` | Latency writer maximum samples. |
| `PERFTEST_LITE_DR_HISTORY_DEPTH_LATENCY` | Latency reader history depth. |
| `PERFTEST_LITE_DR_MAX_SAMPLES_LATENCY` | Latency reader maximum samples. |
| `PERFTEST_LITE_DW_HISTORY_DEPTH_THROUGHPUT` | Throughput writer history depth. |
| `PERFTEST_LITE_DW_MAX_SAMPLES_THROUGHPUT` | Throughput writer maximum samples. |
| `PERFTEST_LITE_DR_HISTORY_DEPTH_THROUGHPUT` | Throughput reader history depth. |
| `PERFTEST_LITE_DR_MAX_SAMPLES_THROUGHPUT` | Throughput reader maximum samples. |

For example, a custom fixed-array build with CSV output can be configured as:

```bash
cmake -S . -B build-custom \
  -DRTIMEHOME=/path/to/rti_connext_dds_micro-4.3.0 \
  -DRTIME_TARGET_NAME=x86_64leElfgcc13.3.0 \
  -DRTIME_TARGET_PSL=x86_64leElfgcc13.3.0-Linux6 \
  -DPERFTEST_LITE_TYPE=zcopy \
  -DPERFTEST_LITE_ENABLE_ZEROCOPY=ON \
  -DPERFTEST_LITE_ZCOPY_DATALEN=64028 \
  -DPERFTEST_LITE_RENDER_TABLE=OFF \
  -DPERFTEST_LITE_RENDER_CSV=ON
cmake --build build-custom
```

The type, generated IDL, and transport capabilities are build-time selections. Both peers must be built with compatible type and transport choices.


## Artifacts

CMake creates these targets:

- `perftest_lite_pub`: publisher static library.
- `perftest_lite_sub`: subscriber static library.
- `perftest_lite_publisher` and `perftest_lite_subscriber`: executables.
- `perftest_lite_gen`: generated-IDL object library.

A zcopy executable has the `_zcopy` suffix. There is no single `libperftest_lite.a` target.

No install or CMake export package is currently defined. Use source integration or link the role-specific build artifact with the same generated sources, compile definitions, middleware archives, and platform libraries as the executable build.
