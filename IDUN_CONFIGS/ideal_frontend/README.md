# Ideal instruction-front-end study

These 32 configurations cover every subset of five factors: I-cache, CBP, IBP,
BTB, and instruction TLB (ITLB). They include perfect BPU (`ideal_cbp_ibp_btb`),
I-cache plus perfect BPU (`ideal_icache_cbp_ibp_btb`), ITLB alone (`ideal_itlb`),
and all five together (`ideal_icache_cbp_ibp_btb_itlb`). They let you measure standalone gains
and interactions without assuming individual gains add up.

The baseline is copied from `storage_dse/baseline_i32k_b8k.json`: 32 KiB L1I
(64 sets × 8 ways × 64 bytes), 8,192-entry basic BTB (1,024 sets × 8 ways),
TAGE-SC-L conditional prediction, ITTAGE indirect prediction, FDIP instruction
prefetching, SRRIP L1I replacement, Berti L1D, Pythia L2, and DRRIP LLC.
UBS is disabled. All unspecified settings inherit the same simulator defaults.
Wrong-path replay is explicitly enabled for every configuration, including
the baseline, as requested for this study. The generator preserves this policy
by default. `python3 IDUN_CONFIGS/ideal_frontend/generate.py --no-wrong-path`
disables it uniformly for an alternative study.

## Meaning of ideal

- **I-cache:** every translated L1I request hits, including cold/conflicting
  lines, without accessing the lower cache. Normal translation, hit latency,
  tag bandwidth, fetch bandwidth, and queues remain. FDIP sees all instruction
  addresses as present. This is an always-hit I-cache, not zero-cycle fetch or
  an ideal TLB. The shortcut does not model instruction data and is therefore
  restricted to instruction caches by configuration validation.
- **ITLB:** every instruction translation hits, including cold pages, using
  the same per-core virtual-to-physical mapping as the page walker. Page walks
  and minor-fault latency are bypassed; normal ITLB hit latency, lookup bandwidth,
  and queues remain. Applies to correct-path and replayed wrong-path fetches.
  DTLB and the shared STLB retain baseline behavior for data translations. This
  factor is independent of the always-hit I-cache. Configure it with
  `"ITLB": {"ideal_translation": true}`; it is restricted to instruction TLBs.
- **CBP:** use the trace direction for conditional branches (and the trace's
  fallback `BRANCH_OTHER` class). BTB misses/target errors remain. The real
  TAGE-SC-L still receives its normal updates.
- **BTB:** supply branch presence, type, and direct targets from the current
  trace instruction, including first encounters; return targets are also exact.
  ITTAGE still predicts indirect jumps/calls unless IBP is ideal. Thus this
  factor includes RAS errors, making the three BPU factors sufficient for
  perfect BPU. It is not merely an unlimited-capacity warmed BTB.
- **IBP:** use the exact indirect jump/call target, but only when a BTB entry
  identifies it as indirect. BTB misses remain unless BTB is also ideal.
  Returns belong to the BTB factor above.

The BPU oracle is active only for the instruction currently consumed from the
correct-path trace. Wrong-path replay receives ordinary predictions and never
looks ahead in the trace. Real predictors continue training on correct-path
branches. `ideal_btb` and `ideal_ibp` are implemented by `basic_btb`; use that
module for this study. The core's normal correctly predicted taken-branch fetch
stop remains even with perfect BPU. Backend, memory, data translation, and
frontend width/latency constraints remain in the fully ideal configuration.
Ideal instruction translation retains the shared VM mapping policy; bypassing
page-table allocations may change subsequent physical-page allocation order.

## Build and run

```bash
./config.sh --join chain IDUN_CONFIGS/ideal_frontend/*.json
make -j8

# Alternatively, submit a cluster build followed by dependent runs:
./idun_experiment.sh ideal_frontend
```

Use the same trace set, warmup length, and ROI length for every configuration.
The study requires 32 × number-of-traces simulations. Keep results from different
binary revisions or wrong-path policies in separate directories.

## Plot overall averages

```bash
python3 aggregation_scripts/plot_ideal_frontend.py \
  /cluster/work/romankb/results/ipc1/ideal_frontend \
  --output ideal_frontend
```

The input layout is `<results>/<configuration>/<trace-group>/<trace>.json`
with a completed sibling `.log`, as produced by the runner. The script writes
PNG/PDF horizontal bar charts (sized for all 32 configurations), a summary CSV, and `_per_trace.csv`. It divides each
configuration's ROI IPC by the baseline IPC **for the same trace**, then takes
the geometric mean of those speedups, with equal weight per trace. Baseline is
1×. ROI IPC combines instructions/cycles across phases. This is not a ratio of
arithmetic mean IPCs, and trace groups are not weighted equally unless they have
equal numbers of traces. CSVs retain full precision.

Missing configurations, duplicate traces, unequal trace coverage, invalid IPC,
and missing/unfinished simulation logs cause errors. `--allow-incomplete` skips
entirely absent variants while still requiring a baseline and identical trace
coverage for every included variant. Gains are measured relative to the same
baseline; differences between combination results expose interactions.
