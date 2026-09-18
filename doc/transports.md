# Transports and Discovery

## UDPv4

UDPv4 is compiled into every current build. Supply a valid local IPv4 interface through `-nic` and peer address through `-peer` for two-host use.

## Serialized SHMEM

 Serialized SHMEM is selected at runtime with `-transport SHMEM`. It is distinct from Zero Copy: data remains serialized and a variable-length sequence type is used. It is intended for same-host testing; `-peer` is ignored by the SHMEM setup.

## Zero Copy v2

Zero Copy requires a `zcopy` type build and `-DPERFTEST_LITE_ENABLE_ZEROCOPY=ON`. Select it with `-transport ZeroCopy`.

Zero Copy is a same-host mode. The fixed type length must also be set at build
time with `PERFTEST_LITE_ZCOPY_DATALEN`, for example:

```bash
-DPERFTEST_LITE_TYPE=zcopy \
-DPERFTEST_LITE_ENABLE_ZEROCOPY=ON \
-DPERFTEST_LITE_ZCOPY_DATALEN=64028
```

The default is `64028`. This is the reported sample size, including 28 bytes of
fixed type overhead; the payload array is derived by subtracting that overhead.
It is also the runtime default when `-datalen` is omitted. An explicitly supplied
`-datalen` must equal the compiled size, and publisher and subscriber must use
compatible builds. Zero Copy cannot be combined with serialized SHMEM in one
executable.

## Discovery

DPSE is the default selection. Choose DPDE only with:

```bash
-DRTI_DPDE=ON -DRTI_DPSE=OFF
```

Both sides need compatible discovery settings and must use the same domain. Fixed participant names, participant IDs, topic names, and DPSE endpoint keys currently constrain the project to one publisher and one subscriber.

## Mismatch symptoms

A process waiting for a peer commonly indicates a mismatch in domain, selected transport, type compatibility, discovery mode, NIC/peer address, or network reachability. Confirm the configuration banner on both processes before debugging the middleware.


