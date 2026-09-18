# CLI Reference

The generated `--help` output from the selected executable is the authoritative command-line reference. Options are parsed by the shared CLI wrapper; each role-specific executable is compiled with only its own engine.

| Option | Default | Meaning |
| --- | --- | --- |
| `-pub`, `-sub` | Built artifact's role | Select the role. An artifact built with both roles defaults to publisher. |
| `-domain <id>` | `0` | DDS domain ID. |
| `-nic <name>` | `lo` | IPv4 network-interface name. |
| `-peer <address>` | `127.0.0.1` | Peer initial address for applicable transport/discovery setup. |
| `-transport <id>` | `UDPv4` | A compiled-in transport: `UDPv4`, optional `SHMEM`, or optional `ZeroCopy`. |
| `-datalen <bytes>` | `100`; compiled size for zcopy | One reported/sample size. Includes the 28-byte fixed type overhead. |
| `-exec <seconds>` | `10` | Run duration. |
| `-pubrate <samples/s>` | `-1` | Publisher rate; `-1` is unlimited. |
| `-latencyTest` | off | Strict ping-pong mode: every data sample is a ping. |
| `-latencyCount <n>` | `10000` | Send a latency ping every `n` samples in non-strict mode. |
| `-rel` | Best Effort | Enable reliable forward-path QoS. |
| `-subSleep <microseconds>` | `1000000` | Subscriber main-loop sleep. |
| `-printPeriodic` | off | Print periodic statistics. |
| `-noPrintConfig` | off | Hide the configuration banner. |
| `-latencyLog <file>` | none | Write per-pong latency samples to a file. |
| `-latencyStream` | off | Emit per-pong `[LAT] elapsed_us,latency_us` lines. |
| `-h`, `--help` | — | Print help and exit successfully. |

## Validation cautions

The parser rejects malformed and out-of-range numeric values, including
zero or negative values where the option requires a positive value. The type
layer additionally rejects unsupported sample sizes when execution begins.
Treat values outside the documented ranges as invalid input.

## Examples

### Local UDPv4 throughput

Start the subscriber first:

```bash
./build/perftest_lite_subscriber -sub
```

Then start a publisher using a 256-byte sample for five seconds at 5,000
samples per second:

```bash
./build/perftest_lite_publisher -pub \
	-datalen 256 -exec 5 -pubrate 5000
```

### Two-host throughput with periodic latency

On the subscriber host, use its local interface and the publisher address:

```bash
./build/perftest_lite_subscriber -sub \
	-domain 10 -nic eth0 -peer 192.0.2.10
```

On the publisher host, use its local interface and the subscriber address:

```bash
./build/perftest_lite_publisher -pub \
	-domain 10 -nic eth0 -peer 192.0.2.20 \
	-datalen 1024 -exec 10 -pubrate 10000 -latencyCount 10000
```

### Strict latency with a log

Use the same domain, NIC, peer, and transport on both processes. Strict
latency makes every data sample a ping and waits for its pong:

```bash
./build/perftest_lite_subscriber -sub -latencyTest
```

```bash
./build/perftest_lite_publisher -pub \
	-datalen 64 -exec 10 -latencyTest -rel \
	-latencyLog latency.csv -latencyStream
```

The publisher writes per-pong samples to `latency.csv` and also emits `[LAT]`
records through the normal output stream.

### Selecting a shared-memory transport

For a sequence build with serialized SHMEM available, select it explicitly on
both processes:

```bash
./build/perftest_lite_subscriber -sub -transport SHMEM
./build/perftest_lite_publisher -pub -transport SHMEM -datalen 1024 -exec 10
```

For a Zero Copy build, use `ZeroCopy` instead:

```bash
./build/perftest_lite_subscriber -sub -transport ZeroCopy
./build/perftest_lite_publisher -pub -transport ZeroCopy -exec 10
```

For a Zero Copy build, `PERFTEST_LITE_ZCOPY_DATALEN` determines the fixed
reported sample size. It is used as the default when `-datalen` is omitted, and
an explicitly supplied `-datalen` must equal the same compiled size.

The selected transport must be compiled into both role executables, and both
processes must use compatible type and transport builds.

