# Results

## Ownership by role

Publisher rows contain latency measurements. Subscriber rows contain received samples, detected sequence gaps, and average throughput. A row is not a combined report from both processes.

## Size accounting

`-datalen` specifies the reported/sample size for the single run. It includes
`PERFTEST_LITE_OVERHEAD_BYTES`, currently 28 bytes. The type build options
specify payload bounds. For example, a sequence build with a 64,000-byte
payload bound accepts a reported/sample size up to 64,028 bytes.

## Latency

The publisher timestamps pings and computes one-way latency as half of the observed round-trip time: $latency = RTT / 2$. This assumes symmetric timing and path behavior; it is not synchronized-clock one-way latency.

Latency fields are in microseconds. `total_samples` is sent sample count and `latency_samples_received` is available through the C API but not emitted by the current CSV schema.

Percentiles require a nonzero `PERFTEST_LITE_LATENCY_HISTORY_SIZE`. Its default is `0`, so `p50_latency_us`, `p90_latency_us`, and `p99_latency_us` are zero placeholders rather than measured percentiles. Do not interpret these zeros as a measurement.

## Throughput and loss

The subscriber reports `avg_throughput_mbps` and `lost_samples`. Loss is sequence-gap detection and does not prove all possible middleware or transport loss mechanisms. The exact throughput calculation is an implementation metric; compare it only across runs with the same build, topology, duration, and methodology.

## Renderers

The build contains exactly one final renderer.

Publisher CSV columns:

```text
size_bytes,total_samples,avg_latency_us,min_latency_us,max_latency_us,p50_latency_us,p90_latency_us,p99_latency_us,elapsed_us
```

Subscriber CSV columns:

```text
size_bytes,total_samples,lost_samples,avg_throughput_mbps,elapsed_us
```

Table output is intended for people. CSV is intended for automation, but the
current output sink also carries the initial configuration block, progress,
diagnostic, and optional latency-stream messages. The initial configuration
block is enabled by default and is suppressed with `-noPrintConfig`. Periodic
statistics and latency streaming are disabled by default and appear only when
`-printPeriodic` or `-latencyStream` is explicitly used. For a CSV capture,
use `-noPrintConfig`, do not enable those two options, and validate the
complete captured output before treating it as parse-safe CSV.
Output-stream separation is pending release work.

### Implementing a custom renderer

Renderers are deliberately small callbacks over the public
`PerftestLiteResults` data. A custom renderer can write to a file, UART,
syslog, a ring buffer, or an application-specific telemetry interface without
changing the measurement engine.

The renderer interface is declared in
[`src/render/perftest_lite_render.h`](../src/render/perftest_lite_render.h):

```c
typedef struct {
	void (*begin)(void *user, PerftestLiteRole role,
				  const PerftestLiteInputArgs *args);
	void (*row)(void *user, const PerftestLiteSizeResult *result);
	void (*end)(void *user);
} PerftestLiteResultRenderer;
```

Implement the callbacks in a C source file. `begin()` is called once, then
`row()` is called once for every entry in `results->sizes`, in input order, and
`end()` is called once. Any callback may be `NULL`. The `user` pointer is
passed unchanged to every callback and is the recommended way to provide
output state; do not use renderer globals when more than one test can run in
the same process.

For example, this renderer emits one compact line per result and sends it to
an application-provided output function:

```c
#include "perftest_lite_render.h"
#include <stdio.h>

typedef void (*MyWriteFn)(void *context, const char *text);

typedef struct {
	MyWriteFn write;
	void *context;
} MyRenderContext;

static void my_begin(void *user, PerftestLiteRole role,
					 const PerftestLiteInputArgs *args)
{
	MyRenderContext *output = (MyRenderContext *)user;
	(void)args;
	output->write(output->context,
				  role == PERFTEST_LITE_ROLE_PUBLISHER
					  ? "publisher\n" : "subscriber\n");
}

static void my_row(void *user, const PerftestLiteSizeResult *result)
{
	MyRenderContext *output = (MyRenderContext *)user;
	char line[128];

	/* Use snprintf or an equivalent bounded formatter for the target. */
	snprintf(line, sizeof(line), "%d %llu %llu\n",
			 result->size_bytes,
			 (unsigned long long)result->total_samples,
			 (unsigned long long)result->elapsed_us);
	output->write(output->context, line);
}

static void my_end(void *user)
{
	(void)user;
}

static const PerftestLiteResultRenderer my_renderer = {
	my_begin, my_row, my_end
};
```

On a restricted target, replace `snprintf` with the target's bounded
formatting routine.

#### Embedded/application integration

For an application that calls the C API, pass the renderer directly after the
measurement call:

```c
PerftestLiteInputArgs args;
PerftestLiteResults results;
MyRenderContext output = { my_write, my_context };

perftest_lite_main(&args, &results);
perftest_lite_render_results(&my_renderer, &output, args.role,
							 &args, &results);
```

The application owns `results` storage and the lifetime of `output` until
rendering completes. Check the return code from `perftest_lite_main()` before
rendering if partial or failed results should not be emitted. The engine may
also return successfully with `results->sizes == NULL`; a renderer should
handle that case and treat `size_count == 0` as an empty result set.

All fields available to a renderer are documented by
`PerftestLiteSizeResult`: publisher results primarily use the latency fields,
while subscriber results primarily use `lost_samples` and
`avg_throughput_mbps`. Unused fields remain present so one renderer interface
can support both roles.
