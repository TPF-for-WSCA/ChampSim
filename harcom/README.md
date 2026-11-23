# ChampSim HARCOM energy models

## Gshare

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

## Basic BTB and frontend

```sh
./config.sh --no-compile-all-modules harcom/frontend.json
make -j
bin/champsim-harcom-frontend -w 1000000 -i 10000000 --json frontend-results.json /path/to/trace.champsimtrace.xz
```

The sample uses the existing TAGE-SC-L direction predictor, the HARCOM-instrumented
basic BTB, FDIP and wrong-path replay. To use another configuration, set its
`btb` field to `harcom/btb/harcom_basic_btb`. The instrumentation prices activity
with HARCOM SRAM, register and logic models. The existing ChampSim structures
continue to determine the simulated predictions, fetch requests and timing.

| Component | Modeled design |
| --- | --- |
| DIB | 32 sets, eight ways and 16-byte windows. Each lookup reads and compares the eight tags. Promotions and fills update a per-set LRU row. |
| FDIP FTQ and next-PC logic | A 24-block FTQ with eight prediction-slot banks and 131 bits per slot. The model charges populated slot writes, nonempty queue reads, pointer changes and redirects. Next-PC logic includes the sequential increment, branch selection and PC register. |
| Basic BTB direct table | 512 or 1024 sets, eight ways and full tags. The model reads the ways in parallel and charges one data-row write per fill. The lookup for a training update can supply the same row-selection result to the fill. |
| Basic BTB ITTAGE | Parallel base and tagged SRAM banks, provider and alternate selection, resettable useful bits, and a shift register with taps for folded history. |
| Basic BTB return-address stack | A 64-entry stack per context and four-bit call-size trackers. FDIP checkpointing copies live stack entries and clears a shadow-valid bitmap. The first committed write to a call-size index after a checkpoint saves the older value in a shadow SRAM. The speculative read selects the shadow entry when marked valid. |

The report lists unsupported geometry and idealized target configurations as
coverage gaps. TAGE-SC-L direction prediction, leakage, and the rest of the chip
are outside this subtotal. Gshare energy can be enabled independently by selecting
the HARCOM gshare direction predictor in a configuration that uses the frontend model.

The vendored HARCOM header is from
[CBP-NG commit `5ceea8358ddbab94e5621e2536530b860cc1e5b7`](https://github.com/AmpereComputing/cbp-ng/tree/5ceea8358ddbab94e5621e2536530b860cc1e5b7).
