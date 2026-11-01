# Ideal instruction-front-end study

These 32 configurations cover every subset of five factors: I-cache, CBP, IBP,
BTB, and instruction TLB (ITLB). They include perfect BPU (`ideal_cbp_ibp_btb`),
I-cache plus perfect BPU (`ideal_icache_cbp_ibp_btb`), ITLB alone (`ideal_itlb`),
and all five together (`ideal_icache_cbp_ibp_btb_itlb`). They let you measure standalone gains
and interactions without assuming individual gains add up.

The baseline derives from `storage_dse/baseline_i32k_b8k.json`, with a smaller
BTB and explicit revised recovery settings: 32 KiB L1I
(64 sets × 8 ways × 64 bytes), 4,096-entry basic BTB (512 sets × 8 ways),
TAGE-SC-L conditional prediction, ITTAGE indirect prediction, FDIP instruction
prefetching, SRRIP L1I replacement, Berti L1D, Pythia L2, and DRRIP LLC.
UBS is disabled. Every ideal-study configuration explicitly sets a 2-cycle
post-resolution redirect delay and a 24-entry FTQ. The global default recovery
delay is also 2 cycles; explicit configuration overrides remain supported.
This delay is additional to time spent fetching/decoding/executing the branch,
not a claim that total branch recovery takes 2 cycles. It is a study parameter,
not a calibrated model of a particular processor. All unspecified settings
inherit the same simulator defaults.
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
  perfect demand BPU. For independent lookahead, an unlimited directory of
  previously observed branch entries removes capacity/conflict misses; cold
  branches and dynamic return/indirect outcomes are not available ahead of time.
  It is not merely an unlimited-capacity warmed BTB on the demand path.
- **IBP:** use the exact indirect jump/call target, but only when a BTB entry
  identifies it as indirect. BTB misses remain unless BTB is also ideal.
  Returns belong to the BTB factor above.

The BPU oracle is active only for the instruction currently consumed from the
correct-path trace. Wrong-path replay receives ordinary predictions and never
looks ahead in the trace. Real predictors continue training on correct-path
branches at demand trace consumption. Predictor histories are not speculatively
advanced/checkpointed, so this remains an approximation; lookahead scratch state
is refreshed before outcome training. `ideal_btb` and `ideal_ibp` are implemented by `basic_btb`; use that
module for this study. The core's normal correctly predicted taken-branch fetch
stop remains even with perfect demand BPU. Backend, memory, data translation, and
frontend width/latency constraints remain in the fully ideal configuration.
Ideal instruction translation retains the shared VM mapping policy; bypassing
page-table allocations may change subsequent physical-page allocation order.

## Predicted-flow FDIP and wrong-path NOPs

FDIP is automatically driven by the core's independent address generator when
the L1I selects the `fdip` prefetcher. It predicts one 32-byte fetch block per
cycle, ending at the first predicted taken branch, and buffers up to 24 blocks
(`fdip_ftq_size`). Demand fetch consumes the saved predictions; FDIP does not
receive targets from demand branch callbacks. Sequential blocks are prefetched
as well as predicted taken targets. The walker continues along the predicted
path during a misprediction until the FTQ fills or recovery resolves the branch.
The basic BTB keeps a separate speculative RAS for predicted calls/returns,
restored from the demand RAS when lookahead restarts after a redirect. This
prevents outstanding speculative calls from changing architectural RAS state.
Recovery flushes queued predictions and unissued prefetches; requests already
in the memory hierarchy remain and can pollute caches. Prediction and fetch
restart after the configured redirect delay.

The address generator assumes four-byte instruction slots, matching the
translated IPC1 traces used in this study. Unaligned dependency-only records bypass both prediction and the FTQ, and
are excluded from replay fallthrough history. Other trace encodings require an
instruction-boundary model. The initial PC and resolved redirects seed the
walker; it never reads future trace instructions/outcomes. An unexpected trace
discontinuity forces resynchronization and is counted separately. These counters
should be checked before interpreting results.

Previously observed wrong-path instructions are replayed without data execution.
Unseen PCs become four-byte NOPs, still issuing instruction fetch/translation
traffic and following saved BTB/direction predictions. Unknown-NOP counts are
reported. Replay is not full speculative backend execution. The ideal CBP/IBP
oracles apply only to the current demand instruction; they do not supply future
wrong-path outcomes. Consequently the fully ideal variant is not a bound on
perfect future FDIP control-flow prediction.

New JSON counters include demand BTB accesses/misses by branch type (exact for
`basic_btb`), physical BTB lookups/hits, direction/target mistakes (overlapping),
FDIP blocks/predictions/FTQ-full cycles/discontinuities, recovery stall cycles,
and aggregate detection-to-redirect cycles. Empty frontend and ROB cycles are
occupancy diagnostics, not an exclusive stall-cause decomposition.

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
PNG/PDF horizontal bar charts (sized for all 32 configurations), a summary CSV,
`_per_trace.csv`, `_by_group.csv`, and `_diagnostics_by_group.csv`. Separate `_client`, `_server`, and `_spec`
PNG/PDF charts normalize and average within each group. It divides each
configuration's ROI IPC by the baseline IPC **for the same trace**, then takes
the geometric mean of those speedups, with equal weight per trace. Baseline is
1×. ROI IPC combines instructions/cycles across phases. This is not a ratio of
arithmetic mean IPCs, and trace groups are not weighted equally unless they have
equal numbers of traces. CSVs retain full precision.

The `_wrong_path_unknown.png`/`.pdf` chart shows the percentage of wrong-path
instructions missing from replay history for each configuration and workload
group: `100 * sum(wrong_path_unknown) / sum(wrong_path_fetched)` over ROI phases
and traces. The diagnostics CSV includes both counts and
`wrong_path_unknown_percent`. Missing counters or zero fetched wrong-path
instructions produce N/A, rather than a zero percentage.

Missing configurations, duplicate traces, unequal trace coverage, invalid IPC,
and missing/unfinished simulation logs cause errors. `--allow-incomplete` skips
entirely absent variants while still requiring a baseline and identical trace
coverage for every included variant. Gains are measured relative to the same
baseline; differences between combination results expose interactions.

The existing root `ideal_frontend.*` artifacts describe the old 8K/one-cycle,
trace-triggered experiment. Do not compare new variants to that old baseline or
overwrite its raw results. Use a fresh result root for this revision.
