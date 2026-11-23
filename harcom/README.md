# HARCOM gshare

The `harcom_gshare` branch direction predictor implements ChampSim's gshare
algorithm with a HARCOM SRAM of 16,384 two-bit counters, a 14-bit global history
register, and combinational logic for indexing, prediction, and updates.

From the ChampSim root, use GCC 12+ or Clang 19+ to build and run the example.

```sh
./config.sh --no-compile-all-modules harcom/gshare.json
make -j
bin/champsim-harcom-gshare -w 1000000 -i 10000000 --json harcom-results.json /path/to/trace.champsimtrace.xz
```

For another configuration, set the core's `branch_predictor` to
`harcom/branch/harcom_gshare`. Other predictor configurations continue to use
the existing ChampSim models.

The text report and the top-level `energy` object in each measured phase give
per-instance dynamic energy and a modeled subtotal in joules. Average power
uses the common scheduler interval from the end of warmup to the end of the
measured phase. A component's clock period is available in the JSON, and
power is reported in watts. Energy per instruction uses the sum of retired
instructions across all cores over that same interval. Zero-duration
intervals have no power value.

The estimate covers gshare operations requested by ChampSim, including
speculative lookups when enabled. The model uses the reference HARCOM
technology at nominal 0.75 V and serializes SRAM accesses for energy
accounting. ChampSim controls pipeline timing. Coverage is partial and
includes dynamic energy for each selected HARCOM gshare instance. Run each
simulation in its own process because HARCOM's floorplan and storage share
a process-wide lifetime.

The vendored HARCOM header is from
[CBP-NG commit `5ceea8358ddbab94e5621e2536530b860cc1e5b7`](https://github.com/AmpereComputing/cbp-ng/tree/5ceea8358ddbab94e5621e2536530b860cc1e5b7).
