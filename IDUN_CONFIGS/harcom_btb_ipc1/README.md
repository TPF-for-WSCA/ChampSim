# IPC1 baseline BTB vs LiteBTB

The sweep models one classic Zen 6 core, using EPYC 9686F measurements where
available. It varies **2K, 4K, 8K, 16K and 32K** nominal direct-table entries.
Both designs have eight BTB ways and use TAGE-SC-L, FDIP, wrong-path replay and
linked DRAMPower. The baseline uses the basic
BTB. LiteBTB uses `xbtbx` with seven compressed-target ways and one
full-target way. Entry counts cover the direct table. The startup log
includes the UTB, ITTAGE and return-stack storage. Data prefetchers
are disabled in both designs because their energy is unmodeled.

## Tags and UTB

Both designs use the per-capacity clipped tag widths from Table IV.
LiteBTB uses the corresponding UTB geometry from Table V with LRU.
Its full-target way uses an inline tag of the same width. The resulting
UTB tag stores the bits above the UTB set index.

| BTB entries | Clipped tag bits | LiteBTB UTB entries | UTB ways × sets | UTB upper-tag bits | UTB pointer bits |
| --- | ---: | ---: | ---: | ---: | ---: |
| 2K | 13 | 384 | 12 × 32 | 8 | 9 |
| 4K | 14 | 640 | 10 × 64 | 8 | 10 |
| 8K | 13 | 640 | 10 × 64 | 7 | 10 |
| 16K | 12 | 768 | 12 × 64 | 6 | 10 |
| 32K | 13 | 512 | 16 × 32 | 8 | 9 |

The paper compares BTB-X with eight compressed-target ways against
LiteBTB with eight UTB-backed ways. This sweep keeps the requested
basic BTB comparison and retains the existing full-target LiteBTB
way for branches beyond the compressed target range.
[LiteBTB paper](https://roman.brunner.best/assets/pdf/LiteBTB_MICRO26.pdf)

## Core parameters

| Structure | ChampSim configuration |
| --- | --- |
| L1I and L1D | 32 KiB, eight ways and 48 KiB, 12 ways, respectively. Both have 64 sets and 64-byte lines. |
| Private unified L2 | 1 MiB, 16 ways and 1,024 sets. One refill per cycle approximates the published 64-byte/cycle transfer rate. |
| L1 TLBs | 64 instruction entries and 96 data entries. Sixteen sets at four and six ways, respectively, are ChampSim geometry choices. |
| Reorder buffer and register file | 576 ROB entries and 288 entries in the unified ChampSim register file, using measured classic Zen 6 capacities. |
| Pipeline widths | Eight instruction slots for fetch, decode, dispatch and retirement. The fetch width is a trace-level proxy. Six generic execution slots approximate the six scalar integer ALUs. |
| Load and store path | Up to four load issues and two store operations per cycle. 128 store entries use the disclosed Zen 6c size as a proxy. The ChampSim load queue keeps its 128-entry default. |
| Last-level cache | 4 MiB, 16 ways and 4,096 sets, representing one core's capacity share of the classic CCD's 48 MiB L3. |

The L1D uses a four-cycle cache lookup as a timing proxy. ChampSim uses
one generic execution pool and its own scheduler scan, DIB, fill stages, TLB
page handling and frontend queues. The reported limit of 280 in-flight loads
on classic Zen 6 does not directly size a ChampSim load queue, which releases
entries on completion. Clock, remaining latencies and queue defaults follow
the experiment's existing simulator settings. The linked DDR4-2400 reference
keeps the memory system fixed across pairs. These settings model structural
differences in the simulator without calibrating energy to EPYC silicon.

[AMD EPYC 9006 architecture overview](https://docs.amd.com/api/khub/documents/erxSLln6OQx26yZNNjGVzw/content)
· [AMD Venice performance monitor documentation](https://docs.amd.com/v/u/en-US/69163-VenicePMC-pub)
· [EPYC 9686F and 9996 microbenchmarks](https://chipsandcheese.com/p/amds-zen-6-family-feeding-the-beast)
· [AMD architect interview on Zen 6 data paths](https://chipsandcheese.com/p/talking-zen-6-with-mike-clark-at)
· [AMD Zen 6c technical briefing slide](https://www.storagereview.com/wp-content/uploads/2026/11/StorageReview-AMD-Venice-Slides-14.png)

## Build and run

The launcher takes every `*.champsimtrace.xz` under
`/cluster/work/romankb/dataset/IPC1_new_translated`. The current dataset
contains 50 traces, for 500 runs. Each uses **50M warmup + 50M evaluation**
instructions. With explicit instruction limits, ChampSim repeats a trace
at EOF. Nine of the current traces contain fewer than 100M records.

The launcher uses the Micron DDR4-2400 memory specification from the pinned
DRAMSys v5.4.0 submodule.
From the repository root, initialize the submodules and install the vcpkg
dependencies before submitting.

```sh
GIT_LFS_SKIP_SMUDGE=1 git submodule update --init
vcpkg/bootstrap-vcpkg.sh
vcpkg/vcpkg install
IDUN_CONFIGS/harcom_btb_ipc1/submit.sh /path/to/shared/results
```

To use a custom memory specification, pass its path before the results
directory. The launcher saves the selected profile as `memspec.json` in the
experiment directory.

The launcher stages a source snapshot and a sorted trace manifest under the
shared results root, submits one build, and submits the run array with an
`afterok` dependency. Each run emits a text log and a JSON report with IPC,
modeled energy and DRAM timing. Outputs preserve the trace's path relative
to the dataset root. Set `TRACE_ROOT`, `SLURM_ACCOUNT`,
`ARRAY_MAX_CONCURRENT`, `GCC_MODULE` or `DRAMPOWER_CMAKE` to override the
cluster defaults. The build job uses the initialized DRAMPower and DRAMUtils
submodules and the installed vcpkg packages from the shared filesystem.

The basic BTB adapter reports DIB, FTQ and next-PC energy. LiteBTB leaves
those core frontend meters outside its report. Exclude those three components
from the baseline subtotal when comparing matched energy coverage. BTB totals
include each design's target tables, ITTAGE, return stack and UTB where present.

## Plot IPC per joule

Pass the experiment directory printed by `submit.sh` after the array finishes.

```sh
python3 aggregation_scripts/plot_ipc_per_joule.py /path/to/ipc1-btb.XXXXXX
```

The plot divides each trace's ROI IPC by its matched CPU-plus-DRAM joules.
The denominator uses the reported combined modeled energy with the three
baseline-only frontend meters removed. Each point takes the geometric mean
of per-trace IPC/J across the shared manifest. The script requires every
configuration and trace result in the experiment snapshot. It writes
`ipc_per_joule.png`, `ipc_per_joule.csv`, and
`ipc_per_joule_per_trace.csv` there by default. It also writes
`ipc_per_joule_components.png` with a stacked hardware-block breakdown,
`ipc_per_joule_components.csv` with the dynamic, leakage, and total energy
of each reported meter, and `ipc_per_joule_static_dynamic.png` with on-chip
dynamic energy, on-chip leakage, and combined DRAMPower replay energy.
The breakdowns use arithmetic means over the shared traces so that their
components sum to the mean ROI energy. The two charts use the same coverage
as the IPC/J denominator. The CSV includes the baseline-only frontend meters
and flags their exclusion. DRAMPower replay has blank dynamic and leakage
columns because the saved phase reports provide its combined energy.
Use `--output` to set a different filename prefix.
