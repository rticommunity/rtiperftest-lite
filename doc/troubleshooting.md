# Troubleshooting

## CMake cannot find Micro

Set `RTIMEHOME` to the installation root containing `rtiddsgen/scripts/rtiddsgen`, `include`, and target libraries. Set `RTIME_TARGET_NAME` and `RTIME_TARGET_PSL` to the directory names in that installation. The Linux helpers intentionally do not provide a personal-path fallback.

## Generated type or link failures

Remove the build directory after changing type variant, payload bound, interpreted-type setting, compiler, target, or Micro version. Verify that all selected static archives belong to the same Micro installation and target.

## Waiting for peer

Compare both configuration banners. Check domain, `-transport`, type build, discovery selection, NIC, peer address, firewall, and same-host constraints. DPSE uses fixed endpoint configuration and only supports the current one-publisher/one-subscriber topology.

## Rejected sample size

The reported/sample size includes 28 fixed bytes. It must fit the selected type. For zcopy, the logical size must fit the fixed array bound configured at build time.

## CSV contains non-CSV text

The current print sink is shared by configuration, progress, diagnostics, latency streaming, and final output. Use `-noPrintConfig`, leave `-printPeriodic` and `-latencyStream` disabled, and inspect output before parsing. A separate result-output contract is pending.

## Latency percentiles are zero

`PERFTEST_LITE_LATENCY_HISTORY_SIZE` defaults to zero. This disables percentile history to save memory; zero percentile fields do not mean a measured zero latency.

## Strict latency does not complete

Confirm subscriber health and transport matching. In the current implementation, reliable strict latency can wait without a finite timeout for a pong.
