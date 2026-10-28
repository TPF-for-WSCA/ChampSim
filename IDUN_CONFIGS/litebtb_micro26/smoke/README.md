# Short LiteBTB comparison runs

Each JSON builds one executable under `bin/litebtb_smoke/`. The suite covers
basic_btb, all twelve LiteBTB variants, and LiteBTB with UBS. All use FDIP and
a 32 KiB conventional L1I, except `xbtbx_ubs`, which uses the existing 64-set
UBS way geometry (32,512 bytes of data including predictor lines).
The basic BTB and LiteBTB variants use the configured 512 sets and nine ways;
LiteBTB uses heterogeneous target widths. Two-level variants add
a 64-set, 8-way L1 BTB. These are functional comparisons, not equal-storage designs.

From the repository root:

```bash
./config.sh --join chain IDUN_CONFIGS/litebtb_micro26/smoke/*.json
make -j4 bin/litebtb_smoke/xbtbx bin/litebtb_smoke/rabin_xbtbx bin/litebtb_smoke/two_level_xbtbx bin/litebtb_smoke/basic_btb bin/litebtb_smoke/xbtbx_ubs
bin/litebtb_smoke/xbtbx --warmup-instructions 100000 --simulation-instructions 1000000 /path/to/trace.champsimtrace.xz
```

Use the same trace and instruction counts for each executable. Runtime lengths
are command-line options, not configuration keys. Reduce to 10,000 warmup and
100,000 simulation instructions for a very quick startup check.

At startup, `STORAGE` lines give bits, rounded-up bytes, and KiB (1024 bytes).
These are modeled storage estimates using 64-bit addresses, packed tags and
an LRU rank per entry. They exclude C++ padding, host containers, simulator
statistics, queues, prefetcher state, and implementation-dependent hash logic.
They are not physical SRAM area estimates or host RAM measurements.

Cache reports separate data, tags/valid/dirty/prefetch/LRU metadata, and UBS
predictor storage. UBS data sums the configured way capacities; predictor
storage includes one full line, tag/status and useful-byte mask per set.
A UBS entry's offset is counted; its size is fixed by its way. Conventional
cache replacement storage follows the selected policy; custom policies without
a storage model mark the report as partial.

BTB reports count target bits as data and tags/valid/two-bit branch type/LRU
as metadata. LiteBTB counts full tags in bypass ways according to its matching
logic. Auxiliary storage includes its 64-entry RAS and stack pointer, and the
entire L1 BTB in two-level variants. basic_btb auxiliary storage includes its
4096-entry indirect table, conditional history, RAS/pointer and 1024 64-bit
call-size trackers. Simulator timestamps are represented by compact LRU ranks;
trace bookkeeping and debug branch-type copies are excluded.

Measured startup reports for these configurations (modeled storage):

| Structure | Total bits | Rounded bytes | KiB |
| --- | ---: | ---: | ---: |
| Conventional L1I | 291,840 | 36,480 | 35.625 |
| UBS L1I, including predictor | 334,272 | 41,784 | 40.805 |
| Plain LiteBTB, including RAS | 190,471 | 23,809 | 23.251 |
| Two-level plain LiteBTB, including RAS | 234,503 | 29,313 | 28.626 |
| basic_btb, including auxiliary tables | 903,187 | 112,899 | 110.252 |

The historical `4k` filenames in the parent directory are artifact labels;
the configured geometry does not imply a total storage budget of 4 KiB.

Each report prints the total first, followed by component sizes. Component
bit counts sum exactly to the total; byte counts are rounded independently.
UBS predictor data, tags/status and useful-byte masks are included in the UBS
total and listed explicitly. LiteBTB lists targets, tags, valid bits, branch
types, replacement state, L1 BTB components and RAS state. The current LiteBTB
implementation has inline tags and targets: it has no separate UTB or tag
pointer table, so these components are explicitly reported as zero. This
report does not model an unimplemented UTB/pointer-based architecture.
