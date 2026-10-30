# TAGE-SC-L (64 KiB)

This module ports Andre Seznec's TAGE-SC-L implementation from
[Daniel A. Jimenez's ChampSim-bp fork](https://github.com/djimeneth/ChampSim-bp),
commit `c29a7ad8ac4611dbbba1afc4afd47d8c6c4c00c3`, file
[`branch/tage_sc_l/tage_sc_l.cc`](https://github.com/djimeneth/ChampSim-bp/blob/c29a7ad8ac4611dbbba1afc4afd47d8c6c4c00c3/branch/tage_sc_l/tage_sc_l.cc).
The fork credits Alberto Ros for the original ChampSim adaptation. Its repository
is licensed under Apache-2.0; see the root `LICENSE`.

Select it with `"branch_predictor": "tage_sc_l"` in an `ooo_cpu` entry. It is also
the default when that field is omitted. All experiment JSON files under
`IDUN_CONFIGS`, including both FDIP size sweeps, select it explicitly.
Reconfigure and rebuild existing binaries to use the new predictor.

The upstream 64 KiB parameters are retained: 36 logical tagged banks, 10 low and
20 high physical banks of 1,024 entries, histories from 6 to 3,000 bits, the
statistical corrector (including local and IMLI components), and the loop
predictor. Upstream's `predictorsize()` reports 523,355 bits (63.886 KiB). This is
modeled hardware storage; the simulator uses larger C++ objects to represent it.
BTB selection and branch-misprediction latency remain separate configuration
options.

Local adaptations:

- The old `O3_CPU` callbacks are replaced by the current module interface.
- Former globals and dynamically allocated tables belong to each module instance,
  with explicit initialization and cleanup. Moving a module transfers its state.
- As in Jimenez's adapter, conditional branches train direction tables; other
  branches update history through `TrackOtherInst`.
- Lookups change prediction scratch only. Conditional updates recompute scratch
  before training, so intervening lookups cannot change the training context.
  This follows ChampSim's existing update-at-fetch model. Wrong-path lookups do
  not train tables or advance history; speculative history and rollback are not
  modeled.
- History registers use unsigned arithmetic, long-history masks use 64-bit
  literals, and cursor/PRNG wrapping is explicit. These remove undefined signed
  shifts, overflow, and the upstream 32-bit shifts used for 40-bit history masks.
  The history-buffer clearing loop now clears each entry rather than entry zero.
  Results can therefore differ from builds relying on upstream's undefined
  arithmetic behavior.

The algorithm is kept in the private `tage_sc_l.inc` implementation file so its
upstream macros and helper classes do not leak into generated configuration
headers. Regression tests cover learning, extra lookups, independent cores,
nonconditional history updates, and module moves.
