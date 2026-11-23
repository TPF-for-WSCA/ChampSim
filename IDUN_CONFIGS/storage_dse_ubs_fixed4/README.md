# UBS-only fixed-latency DSE rerun

These 40 configurations copy the UBS and Combined configurations from
`../storage_dse`, explicitly setting L1I hit latency to 4 cycles and fill
latency to 4 cycles. Both designs use UBS. Baseline and LiteBTB need no rerun.
The source configs already specify these timings; this separate set supports
rebuilding and rerunning older binaries/results without changing other numbers.
All other configuration fields match the source, except the executable directory.

From the repository root, submit the build:

```bash
./submit_storage_dse_ubs_fixed4.sh build
```

After that job succeeds, submit the 40-config simulation array:

```bash
ORIGINAL_RESULT_ROOT=/path/to/existing/storage_dse \
RESULT_ROOT=/path/to/storage_dse_ubs_fixed4 \
  ./submit_storage_dse_ubs_fixed4.sh run
```

Defaults are `/cluster/work/romankb/results/ipc1/storage_dse` for existing
results and `/cluster/work/romankb/results/ipc1/storage_dse_ubs_fixed4` for
the new results. Use the same traces and instruction counts as the original
experiment. The runner supports `TRACE_ROOT`, `WARMUP_INSTRUCTIONS`,
`SIMULATION_INSTRUCTIONS`, and `ARRAY_MAX_CONCURRENT`; its defaults are
50M warmup, 50M simulation, and 32 concurrent tasks.

The run command links the 40 existing Baseline/LiteBTB result directories into
the new root, then submits only binaries from `bin/storage_dse_ubs_fixed4`.
Original results remain intact, including the old UBS/Combined results.
Rerunning the run command resubmits the UBS/Combined simulations.

Once the array completes, plot all 80 configurations using the original config
directory (the default), which also includes the reused designs:

```bash
python3 aggregation_scripts/plot_storage_dse.py \
  /path/to/storage_dse_ubs_fixed4 --output storage_dse_fixed4
```
