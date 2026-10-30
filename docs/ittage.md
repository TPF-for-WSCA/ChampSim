# Shared ITTAGE indirect target prediction

Every production BTB module uses the same `ittage` implementation in
[`inc/ittage.h`](../inc/ittage.h) and [`src/ittage.cc`](../src/ittage.cc):
`basic_btb`, `xbtbx`, all hashed variants, and all two-level variants.
No JSON setting is required. Reconfigure and rebuild existing binaries.

For a BTB hit classified as an indirect jump or indirect call, ITTAGE supplies
the predicted target. Direct branches keep their BTB targets and returns keep
their return-stack predictions. Two-level BTBs use ITTAGE on both L1 and lower
BTB hits. A BTB miss retains the existing unknown-target behavior.

This is a local implementation of the ITTAGE principles described by Andre
Seznec in [A 64-Kbytes ITTAGE indirect branch predictor](https://jilp.org/jwac-2/program/cbp3_07_seznec.pdf)
(JWAC-2, 2011), rather than a port of his competition configuration. It uses:

- A 512-entry tagless base target table.
- Six tagged tables of 1,024 entries, with history lengths 4, 12, 36, 108, 324,
  and 972 bits. Entries hold full 64-bit targets, 11-bit tags, 2-bit confidence,
  a usefulness bit, and a valid bit.
- Longest-history matching, an alternate prediction, and a 4-bit signed chooser
  for zero-confidence providers.
- Target replacement after confidence falls to zero, allocation in a longer
  history table on a miss, and usefulness resets under allocation pressure.
- Incrementally folded history for indexing and tagging. Indirect branches and
  calls contribute mixed PC/target path bits; other branches contribute direction.

The modeled cost per BTB instance is 519,836 bits (63.457 KiB): 33,280 base-table
bits, 485,376 tagged-table bits, and 1,180 history/control bits. Storage reports
include these components in each BTB's total, including LiteBTB variants. The
host C++ objects are larger than this modeled hardware budget.

Each instance owns its state. Updates train indirect jumps and indirect calls
with known, taken targets using the preceding history, then advance history for
the branch. Conditional branches, direct branches, and returns advance history
without training indirect targets. Unknown targets do not train target tables.
Lookups are const and do not modify tables, history, or prediction scratch, so
wrong-path lookups cannot pollute ITTAGE. This follows the simulator's existing
update-at-fetch convention; speculative history and rollback are not modeled.

`test/cpp/src/173-ittage.cc` exercises the predictor directly. The tests in
`test/cpp/src/174-ittage-all-btbs.cc` exercise all 13 production BTB modules with
alternating full-width targets selected by branch history. A last-target table
cannot pass that workload. Existing direct-target and return-stack tests cover
the remaining dispatch paths.
