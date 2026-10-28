# Front-end storage design-space exploration

This experiment contains **80 configurations**: four designs × nominal L1I
sizes **8, 16, 32, 64 KiB** × BTB capacities **2K, 4K, 8K, 16K, 32K entries**
(K = 1024). The sweep varies capacities independently.

| Design | Instruction cache | BTB |
| --- | --- | --- |
| Baseline | Conventional, 8 ways | basic_btb, 8 ways |
| UBS | UBS | basic_btb, 8 ways |
| LiteBTB | Conventional, 8 ways | xbtbx, 8 heterogeneous ways |
| Combined | UBS | xbtbx, 8 heterogeneous ways |

All designs use FDIP, 64-byte lines, and LRU replacement. Other core/memory
settings use the same ChampSim defaults. Baseline BTB allocation now honors
`btb_sets` and `btb_ways`; its indirect table and return predictor remain fixed.
LiteBTB uses target widths `[0,4,5,7,9,11,19,64]` and 13-bit clipped tags
(except its full-target bypass way). The artifact's 25-bit way is omitted
so eight ways give exact power-of-two entry counts. This is the current
inline-tag LiteBTB port, with no separate UTB or tag-pointer table.

UBS uses the existing 16-way heterogeneous profile and four candidate ways.
Its set counts match the conventional cache at each nominal L1I size: 16,
32, 64 and 128 sets. Thus size labels are comparison points, not identical
physical capacities: UBS stores 444 data bytes per set plus one 64-byte
predictor line, versus 512 conventional data bytes per set. **Actual storage**
on the plot includes tags, status, replacement, UBS offsets and predictor,
and the entire BTB including fixed indirect/RAS/call-tracker state. It is
read from the corresponding run's startup report, never inferred from filenames.

## Generate, build, run

From the repository root:

```bash
python3 IDUN_CONFIGS/storage_dse/generate.py
./idun_build.sh storage_dse
```

The build command submits a Slurm job. After that job completes successfully:

```bash
WARMUP_INSTRUCTIONS=50000000 SIMULATION_INSTRUCTIONS=50000000 \
  ARRAY_MAX_CONCURRENT=32 ./idun_runner.sh storage_dse
```

The runner submits one Slurm array using the existing IPC1 spec/server/client
trace layout. `TRACE_ROOT` and `RESULT_ROOT` can override the defaults in
`idun_runner.sh`. With N traces this runs 80 × N simulations. For a short
experiment, set warmup to 100,000 and simulation to 1,000,000; compare identical
trace sets and instruction counts across all points. No cluster jobs are
submitted by the generator or plotter.

For a local build of the entire sweep:

```bash
./config.sh --join chain IDUN_CONFIGS/storage_dse/*.json
make -j4
```

## Plot after the run completes

```bash
python3 aggregation_scripts/plot_storage_dse.py \
  /cluster/work/romankb/results/ipc1/storage_dse \
  --output storage_dse
```

This produces `storage_dse.png` and `.pdf` with two panels: ROI IPC versus
L1I+BTB KiB, and IPC per KiB versus that same storage. Baseline, UBS, LiteBTB
and Combined use separate colors **and marker shapes**, with legends. Points
are independent sweep configurations; no interpolation is implied.

`storage_dse.csv` contains one row per configuration, globally sorted by total
storage, and `storage_dse_per_trace.csv` contains similarly sorted individual
trace results. The summary uses the harmonic mean of per-trace ROI IPC,
with equal weight per trace; IPC/KiB divides this mean by measured storage.
The single-core ROI loader sums instructions/cycles across phases for each
trace. Metadata columns retain nominal size, BTB entries, and exact cache/BTB
bit totals, so every point can be identified.

Missing configurations fail by default. `--allow-incomplete` permits a partial
sweep, but every included configuration must still have exactly the same trace
set. Missing logs, unfinished runs, duplicate traces, changing storage across
traces, partial storage reports and invalid IPC are rejected. Keep results from
different instruction lengths or binary revisions in separate result roots.
Storage estimates assume packed hardware fields and 64-bit addresses; they
exclude host containers, simulator bookkeeping, prefetcher state and queues.

Replacement models follow the selected built-in policy: packed LRU ranks,
SRRIP RRPVs, DRRIP RRPVs/selector/BRRIP counter, SHiP RRPVs/sampler/SHCT,
or a modeled 64-bit random source. UBS uses its own subblock LRU state.
Custom replacement modules may provide `storage_bits()` returning an optional
bit count; otherwise their conventional-cache report is marked partial and
is rejected by this experiment's plotter.

A five-point, single-trace plot-pipeline check is available in
[validation/](validation/README.md). It uses only 1K warmup + 10K ROI instructions
and must not be treated as full-experiment performance evidence.

The summary `.csv` is also the exact data source for both plot panels:
`total_storage_kib` is the x coordinate, `ipc` is the left-panel y coordinate,
and `ipc_per_kib` is the right-panel y coordinate. All values retain full float
precision; PNG and PDF use the same data.
