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
`harcom/branch/harcom_gshare` or use the predictor accounting option below.

The estimate covers gshare operations requested by ChampSim, including
speculative lookups when enabled. The model uses the reference HARCOM
technology at nominal 0.75 V and serializes SRAM accesses for energy
accounting. ChampSim controls pipeline timing. Coverage is partial and
includes dynamic and leakage energy for each selected HARCOM gshare instance. Run each
simulation in its own process because HARCOM's floorplan and storage share
a process-wide lifetime.

## Energy reporting and leakage

The text report and the top-level `energy` object in each measured phase give
dynamic energy, leakage power and leakage energy by component. On-chip
leakage energy is the component's modeled leakage power multiplied by the
phase's elapsed simulated seconds. Modeled components stay powered for that
interval, including idle cycles. Energy per instruction uses the phase's
retired instruction count across all cores. The JSON preserves the dynamic
subtotal, adds on-chip leakage and on-chip total energy, and leaves average
power unset for a zero-duration phase. With DRAMPower enabled, the combined
joule total adds replay energy once, including its device background.

HARCOM separates SRAM cell and logic fin off-current at nominal 0.75 V with
25 °C reference off-currents. The estimate uses configured array capacities,
bank and way replication, register bits and representative logic circuits.
An array contributes while idle, including banks that a particular access
leaves inactive. HARCOM SRAM estimates include local peripheral fins. The
reference model omits temperature scaling, transistor stacking and power
gating. Physical multiport layouts, full clock trees, global wiring and
unsupported structures remain coverage gaps.

| Component group | Powered inventory |
| --- | --- |
| Branch predictors and BTBs | Counter, weight, tag and target SRAM banks, rank rows, history and useful registers, hashes and selection logic. LiteBTB decode and owner accesses reuse metadata in the corresponding way. Rabin uses the lookup PC and omits saved upper-PC bits in inline ways. |
| DIB, FDIP core frontend and RAS | DIB tag and rank arrays, banked FTQ and cursors, next-PC datapath, stack and call-size arrays, and the FDIP checkpoint stack plus copy-on-write call-size storage. |
| Core backend | Pipeline queues, full ROB rows, rename maps, free list, wrong-path checkpoint when enabled, scheduler and scoreboard, physical registers, representative execution and address-generation lanes, LSQ entries and comparators, and reference clock trunk and link drivers. ROB destination accesses and queue flag activity reuse the corresponding physical storage. |
| Caches, TLBs and PTW | Tag and payload banks per physical way, replacement budgets, request queues, MSHRs, separate refill staging, PTW and PSCL state, prefetch buffer, UBS arrays, and FDIP target queues and directories when configured. |
| DRAM controller | On-chip request and write-data SRAMs, arbitration and bank/refresh state, digital bus control and shared staging per channel. DRAMPower accounts for the external device and interface. |

Set `refill_queue_size` in a cache, TLB, or PTW JSON object to model a finite
refill queue. HARCOM uses that depth for staging registers, cache fill-search
comparators, and the refill portion of FDIP's pending directory. When the
setting is omitted, the simulator keeps an unbounded queue and HARCOM uses
configured MSHR depth for its leakage proxy. The independent PTW walk-tracking
estimate also uses configured MSHR depth. The inventories omit access counters,
simulator timestamps and dependency vectors.

## Basic BTB and frontend

```sh
./config.sh --no-compile-all-modules harcom/frontend.json
make -j
bin/champsim-harcom-frontend -w 1000000 -i 10000000 --json frontend-results.json /path/to/trace.champsimtrace.xz
```

The sample uses HARCOM-instrumented TAGE-SC-L direction prediction, the
basic BTB, FDIP and wrong-path replay. To use another configuration, set its
`btb` field to `harcom/btb/harcom_basic_btb`. Select the TAGE-SC-L energy model
with `branch_predictor: "harcom/branch/harcom_tage_sc_l"`. The instrumentation prices activity
with HARCOM SRAM, register and logic models. The existing ChampSim structures
continue to determine the simulated predictions, fetch requests and timing.

| Component | Modeled design |
| --- | --- |
| TAGE-SC-L | Parallel SRAMs cover bimodal, tagged, loop and statistical-corrector tables and local histories. Fresh predictions charge hash, comparison and selection logic. History updates use a 3,000-bit shift register and fixed taps for the folded histories. Useful bits live in separately resettable planes. |
| DIB | 32 sets, eight ways and 16-byte windows. Each lookup reads and compares the eight tags. Promotions and fills update a per-set LRU row. |
| FDIP FTQ and next-PC logic | A 24-block FTQ with eight prediction-slot banks and 131 bits per slot. The model charges populated slot writes, nonempty queue reads, pointer changes and redirects. Next-PC logic includes the sequential increment, branch selection and PC register. |
| Basic BTB direct table | Eight ways with full tags at 512 or 1024 sets, or clipped tags at power-of-two set counts from 256 to 4096 with 15 bits or the corresponding LiteBTB sweep width. The model reads the ways in parallel and charges one data-row write per fill. The lookup for a training update can supply the same row-selection result to the fill. |
| Basic BTB ITTAGE | Parallel base and tagged SRAM banks, provider and alternate selection, resettable useful bits, and a shift register with taps for folded history. |
| Basic BTB return-address stack | A 64-entry stack per context and four-bit call-size trackers. FDIP checkpointing copies live stack entries and clears a shadow-valid bitmap. The first committed write to a call-size index after a checkpoint saves the older value in a shadow SRAM. The speculative read selects the shadow entry when marked valid. |

The TAGE-SC-L datapath estimate uses HARCOM logic costs for indexing,
arbitration, training, and history updates. It assumes one history-register
write per branch update and parallel tag lookups for fresh predictions.
Cached same-PC predictions charge the reuse check and skip the table and
prediction datapath costs. The report lists unsupported geometries and idealized
target configurations. Global wiring and the frontend clock tree remain outside these estimates. The frontend model can also run with HARCOM gshare.

## Alternate BTBs and direction predictors

Set the top-level `"harcom_predictor_energy": true` and keep the BTB and
direction predictor names in the core configuration. For example:

```json
{
  "executable_name": "champsim-litebtb-energy",
  "harcom_predictor_energy": true,
  "ooo_cpu": [
    {
      "btb": "two_level_full_hashed_xbtbx",
      "branch_predictor": "hashed_perceptron",
      "btb_sets": 512,
      "btb_ways": 8,
      "btb_target_sizes": [8, 8, 12, 12, 16, 20, 32, 64],
      "btb_utb_entries": 128,
      "btb_utb_ways": 4
    }
  ]
}
```

Save the configuration and run `./config.sh --no-compile-all-modules <config>`
and `make -j` with the C++20-capable toolchain. The simulator reports component
energy and average dynamic power in the usual text and JSON outputs.

- All 12 LiteBTB variants use their configured per-way target and tag widths.
  A two-level prediction reads L2 only on an L1 miss. L1, the main target
  table, UTB, ITTAGE, and the return stack have separate energy entries where
  present. UTB costs include generation checks, reference updates, and LRU or
  reference-count replacement. The model assumes parallel tags, a gated
  target payload read, and SRAM banks with local wiring and decode costs.
- The hash logic follows the selected variant. Plain and Rabin tags truncate
  by wiring. Rabin reconstructs the upper target bits from the lookup PC,
  including on inline-tag alias hits. Other inline compressed ways retain
  upper bits from the inserted PC. UTB-pointer ways use the lookup PC. The
  Pearson model treats its permutation ROM as SRAM for read energy. SHA3
  uses a full round datapath estimate and Zobrist uses average XOR fan-in.
- Bimodal charges counter reads and saturating updates. Perceptron and hashed
  perceptron charge score computation, history updates, and conditional
  training. The option selects the existing HARCOM implementations for
  built-in gshare and TAGE-SC-L. Explicit HARCOM module paths remain supported.

The estimates use the reference HARCOM technology. Global wiring and the
frontend clock tree remain outside the model. With LiteBTB, DIB and FDIP
core queue and next-PC logic remain outside predictor accounting. The cache
hierarchy option below can be enabled in the same configuration.

## Cache hierarchy and translation

```sh
./config.sh --no-compile-all-modules harcom/memory.json
make -j
bin/champsim-harcom-memory -w 1000000 -i 10000000 --json memory-results.json /path/to/trace.champsimtrace.xz
```

Set the top-level `"harcom_memory_energy": true` in another configuration
to enable the same accounting alongside its existing cache, replacement,
prefetcher, and frontend modules. The report has separate entries for cache
and TLB tags, payloads, replacement metadata, prefetch buffers, miss tracking,
request queues, refill staging, response ports, and each configured page-walk cache.

- Tag probes read all ways. Data SRAM reads use the selected way after a hit
  that returns data or when a dirty writeback is accepted. A fill or WRITE
  hit charges one full line or TLB payload write. Incoming queue writes are
  charged on admission and reads on consumption, so pending entries can span
  warmup without charging both operations to the same phase.
- The reference SRAM layout uses local banks of up to 64 rows and narrow
  word slices. Virtual and physical address fields follow the configured
  topology. The page walker uses live-entry comparisons and register accesses
  for outstanding steps because its configured MSHR size is not an occupancy
  limit. Page-walk caches charge each lookup, hit, and fill they perform.
- UBS has separate estimates for the full-line predictor, variable-width
  fragment ways, and byte-mask generation. The FDIP target and recent-entry
  queues are also modeled, independently of the core FTQ.
- FDIP address directories mirror virtual resident keys and the keys of
  pending requests. The reference design compares each searched group in
  parallel and combines the masks of matching UBS fragments. The report gives
  this design its own energy line so its cost is visible alongside the cache arrays.
- Cache and walker refill estimates charge register writes on admission,
  reads on each ready attempt, and release on consumption. A merge into a
  staged fill updates its metadata. Each response recipient charges a
  sending port write. DRRIP accounts for set classification, policy-selector
  reads and updates, periodic BRRIP insertions, and RRPV aging.
- The report flags ideal-mode arrays, unmodeled prefetch algorithms such as
  Berti and Pythia, custom prediction-flow probes, and unsupported replacement
  policies. Routing and global bank wiring also remain open.

The vendored HARCOM header is from
[CBP-NG commit `5ceea8358ddbab94e5621e2536530b860cc1e5b7`](https://github.com/AmpereComputing/cbp-ng/tree/5ceea8358ddbab94e5621e2536530b860cc1e5b7).


## Core backend and on-chip links

Set the top-level `"harcom_backend_energy": true` to enable backend and link
energy. It can be used independently or with predictor and memory energy.
The optional `"harcom_link_length_um"` sets the uniform point-to-point link
length in micrometres. The default is 1,000. Rebuild with the same configuration
and a different length to test link sensitivity.

| Component | Activity and hardware assumption |
| --- | --- |
| Fetch, decode, DIB-hit and dispatch queues | Configured depths. One logical row transfer per admitted or consumed instruction, with pointer and valid state. Fetch readiness and wrong-path invalidations are charged separately. Rows reserve the supported source and destination operand slots. |
| Reorder buffer | Dispatch, destination rename, issue, memory completion, execution completion, retirement and squash. Tags use the configured ROB depth. A branch recovery event charges redirect selection and recovery state. |
| Rename and free list | Speculative and committed RAT reads and writes, physical-ID FIFO, destination logical-register metadata, and full speculative map copy and restore. The checkpoint is charged only when replay opens one. |
| Scheduler and wakeup | A source-tag table and physical-ready scoreboard. Each eligible candidate observes its source readiness. A completed destination drives a tag broadcast sized to the execution width and source ports. Scheduling uses an analytical selection estimate. |
| Physical register file | One 64-bit row read per issued source and one write per completed destination. Serialized equivalent SRAM activations approximate multiport access. |
| Execution | A 64-bit add and result select for an aligned nonmemory, nonbranch instruction. Branches use a compare and target-add proxy. Each memory operand uses a 64-bit address add. Dependency-only trace records can drive queue and register events. The execution proxies apply to aligned instructions. |
| Load and store queues | Configured address rows, readiness and release updates, issue or drain reads, load-response address comparisons, and store-address search for forwarding. A forward activates the selected 64-bit store-data path. Accepted cache transfers are charged separately from local queue attempts. |
| Clock distribution | A per-cycle 1 mm reference core trunk and width-scaled gate input load. HARCOM register write costs already include local clock drive. |
| On-chip links | Point-to-point wire and driver per packet bit, from core through caches and translation structures to the memory controller. Accepted requests and consumed responses count once. Headers account for physical and virtual addresses and prefetch metadata. Requests also carry a PC, ASID, type and flags, plus a byte mask on links into UBS. Cache lines, page numbers, and page-table-entry responses have different payload assumptions. Endpoint FIFO and response-register costs belong to memory accounting. |

Wrong-path instructions contribute where they actually use modeled structures.
The activity hooks omit host containers, trace-statistics counters, and full
simulator instruction IDs. ChampSim traces omit opcodes, operand values, and
circuit placement. Treat the execution, clock and wire estimates as reference
assumptions. The full clock tree, physical multiport layout, network routing
and arbitration, and off-chip PHY and DRAM remain open. Link length changes
the wire estimate and leaves simulated timing unchanged.

## DRAM controller and device replay

Set `"harcom_dram_energy": true` to report three in-simulation components per
channel. The request-queue meter covers admission, address and metadata SRAM,
write-data SRAM, collision comparisons, merges, forwarding, and release. The
scheduling meter covers arbitration, bank and row state, mode changes, and the
refresh state machine. The interface-control meter covers digital burst and
response staging and bus arbitration. The on-chip link from step 4 still owns
wire energy. Analytics counters, host dependency vectors, and off-chip signal
energy are excluded from the controller costs.

The system example enables all four HARCOM options and uses the Git submodules
for DRAMPower v6.0.0 and DRAMUtils v1.12.1. The linked build requires GNU Make
4.3 or newer, CMake 3.24 or newer, and the dependencies from the vcpkg manifest.
Set `DRAMPOWER_CMAKE` if the default `cmake` on your path is older. Other
ChampSim configurations can opt in with `"link_drampower": true`.

```sh
GIT_LFS_SKIP_SMUDGE=1 git submodule update --init third_party/DRAMPower third_party/DRAMUtils third_party/DRAMSys
./config.sh --no-compile-all-modules harcom/system-energy.json
make -j
memspec=third_party/DRAMSys/configs/memspec/MICRON_4Gb_DDR4-2400_8bit_A.json
bin/champsim-harcom-system -w 1000000 -i 10000000 --json results.json \
    --drampower-memspec "$memspec" /path/to/trace.champsimtrace.xz
```

The linked mode sends commands to the library in-process. The replay maintains
one FIFO per bank and selects the earliest eligible command across banks,
preferring a data command on ties. ACT and PRE can overlap traffic at other
banks. The scheduler checks row and column timing, bank-group and rank spacing,
the four-activate window, burst direction and data-bus occupancy, and periodic
all-bank refresh. It uses the observed reservation as the earliest request
time and the observed data start as a lower bound on replay data time. The
scheduler starts with precharged banks and uses conservative channel-wide
refresh windows and rank turnarounds. ChampSim continues to time memory
completions with its native controller.

`--drampower-toggle` controls data activity and duty cycle. The final text
and JSON report the combined modeled energy and both elapsed times, with the
difference in absolute and percentage terms. Linked mode also reports mean and
maximum burst-start delay relative to the observed start rounded up to the
device clock. This shows delays that fit inside the measured interval.
The HARCOM and DRAMPower power
values retain their own time denominators. The combined joule total includes
DRAM background energy and uses the native interval for HARCOM and the replay
interval for the device and its off-chip interface. It is a partial
whole-system estimate.

The earlier serialized replay remains available for comparison with
`--drampower /path/to/DRAMPower/build/bin/cli` in addition to the memspec.
It uses the Python adapter and temporary command inputs. Run that mode from
the repository root or pass
`--drampower-adapter /path/to/ChampSim/harcom/dram/replay.py`.

Use `--dram-events` to export the measured phase's native events. The file
contains geometry, phase boundaries, absolute timestamps in picoseconds, bank
reservations with row hit or miss classification, cancellations, refresh due
and per-bank start/end events, and actual data-bus starts. Forwarded reads
produce no device burst. A reserved request can be canceled before it reaches
the data bus.

The example matches the geometry and clock of the
[DRAMSys 5.4.0 Micron 4 Gb DDR4-2400 profile](https://github.com/tukl-msd/DRAMSys/blob/v5.4.0/configs/memspec/MICRON_4Gb_DDR4-2400_8bit_A.json).
Use the profile from the pinned DRAMSys submodule and an upstream
[DRAMPower v6.0.0 CLI](https://github.com/tukl-msd/DRAMPower/tree/v6.0.0).
This release calls `CLI::App::ensure_utf8`. Use CLI11 2.6.2 for its build
if the default dependency fails. CMake accepts
`-DFETCHCONTENT_SOURCE_DIR_CLI11=/path/to/CLI11-2.6.2`.

```sh
GIT_LFS_SKIP_SMUDGE=1 git submodule update --init third_party/DRAMSys
./config.sh --no-compile-all-modules harcom/dram.json
make -j
bin/champsim-harcom-dram -w 1000000 -i 10000000 --json results.json \
    --dram-events dram-events.csv /path/to/trace.champsimtrace.xz
memspec=third_party/DRAMSys/configs/memspec/MICRON_4Gb_DDR4-2400_8bit_A.json
python3 harcom/dram/replay.py --events dram-events.csv --memspec "$memspec" \
    --drampower /path/to/DRAMPower/build/bin/cli --output dram-replay
```

Both replays accept matching DDR4 and DDR5 profiles with 1× all-bank refresh.
In the standalone Python replay, each channel starts precharged. The script preserves the observed burst order
and uses the observed data-bus start as a lower bound on replay data time. It
serializes commands using the profile's timing limits, generates ACT/PRE from
the replay row state, and issues periodic rank refreshes at the shorter of the
native interval and the device interval, when present in the profile.
ChampSim currently quantizes its refresh interval to whole microseconds.
The example therefore replays refreshes at close to 3 µs. The measured interval remains in the
replay, with additional time if needed to finish the observed bursts. DRAMPower
estimates both device and off-chip interface energy with an explicit data
toggle and duty-cycle assumption. Use `--toggle` to vary it and omit
`--drampower` to generate command traces alone.

Use `summary.json` for replay energy and average power over the **replay**
duration, and the per-channel DRAMPower JSON for core, background and interface
details. ChampSim changes bank state at reservation and schedules refreshes per
bank, so device energy at native ChampSim timing remains incomplete. The native
dynamic subtotal continues to report HARCOM activity, with the DRAMPower
estimate included in the combined total for streaming runs. The profile must match each
simulated channel's geometry and clocks. The default ChampSim configuration
has eight bank groups and differs from the DDR4 example.
