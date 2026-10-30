# ChampSim

![GitHub](https://img.shields.io/github/license/ChampSim/ChampSim)
![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/ChampSim/ChampSim/test.yml)
![GitHub forks](https://img.shields.io/github/forks/ChampSim/ChampSim)
[![Coverage Status](https://coveralls.io/repos/github/ChampSim/ChampSim/badge.svg?branch=develop)](https://coveralls.io/github/ChampSim/ChampSim?branch=develop)

ChampSim is a trace-based simulator for a microarchitecture study. If you have questions about how to use ChampSim, we encourage you to search the threads in the Discussions tab or start your own thread. If you are aware of a bug or have a feature request, open a new Issue.

# Research extensions in this branch

This branch contains forward ports of the Uneven Block Size instruction cache
(UBS) and LiteBTB to the current ChampSim configuration and module APIs.

## Instruction-cache MPKI

Compute demand MPKI from ROI statistics as
`1000 * roi.cpu0_L1I.LOAD.miss[0] / roi.cores[0].instructions`.
For UBS, `miss` already includes both full misses and partial misses (matching
line tags whose resident bytes do not cover the fetch). `partial_miss` is a
subset of `miss`; do not add it again. Demand MPKI includes merged demand misses
and excludes prefetch requests. A demand served by the prefetch buffer is a hit.

## Instruction prefetch buffer

All L1 instruction caches use a 32-entry, fully associative FIFO prefetch buffer
by default, with conventional and UBS organizations sharing the same behavior.
Prefetch fills enter this buffer without replacing cache lines or the UBS
predictor. A demand that misses the cache but hits the buffer consumes the entry
and promotes its full line through the cache's normal demand insertion path.
Subsequent accesses use the cache; promotion is counted as one useful prefetch
and a hit, without adding a lower-level request or a second memory fill.
Prefetch probes leave buffered entries and their FIFO order unchanged. Unused
FIFO victims count as useless prefetches. Demand requests that merge with an
inflight prefetch insert directly into the cache when the data returns.

Set `"prefetch_buffer_size"` in `"L1I"` to change the entry count; zero restores
direct prefetch insertion. Other cache levels retain direct insertion by default.
The buffer is independent of the prefetcher module, so it also applies to FDIP.
Its data, tags, metadata, valid bits and FIFO state appear in a separate
`Prefetcher <cache name> prefetch buffer STORAGE` report and belong to the
prefetcher budget. They are excluded from reported I-cache capacity and storage.
The default buffer holds 2 KiB of data for 64-byte lines. Buffer hits use the
cache's configured tag-lookup latency; promotion adds no separate cycle delay.
Rebuild existing simulator binaries to apply this default to experiments.

## Uneven Block Size instruction cache (UBS)

UBS is the MICRO-24 instruction-cache organization called VCL in the original
artifact. Instead of dividing every set into equally sized cache lines, UBS
uses a full-line predictor entry and unevenly sized ways for the instruction
bytes that were actually used. The default paper organization has 16 ways of
4–64 bytes and considers four successively larger candidate ways during its
modified-LRU replacement.

Enable UBS on an L1 instruction cache with `"ubs": true`:

```json
{
  "L1I": {
    "sets": 64,
    "ubs": true,
    "prefetcher": "fdip"
  }
}
```

On a UBS partial miss, existing fragments remain hittable while missing bytes
are fetched. Uncovered accesses merge into the pending refill; covered accesses
can complete independently, including with all MSHRs occupied. Refill migrates
surviving fragments into the full-line predictor. MSHRs retain a byte-coverage
snapshot if normal replacement evicts a fragment before the refill arrives;
fragments are not pinned against normal replacement.

UBS currently supports instruction caches with 64-byte lines. It uses its
internal replacement policy rather than the configured replacement module;
data-cache writebacks are not supported. The way geometry can be changed with
`ubs_way_sizes` and `ubs_candidate_ways`. See
[`IDUN_CONFIGS/fdip_icache_sizes`](IDUN_CONFIGS/fdip_icache_sizes/README.md) for
paired conventional/UBS capacity sweeps and
[`Creating-a-configuration-file.rst`](docs/src/Creating-a-configuration-file.rst#uneven-block-size-instruction-cache)
for the complete constraints and statistics.

## LiteBTB

LiteBTB is the variable-target-width BTB used for the MICRO'26 experiments. It
is named `xbtbx` in the paper artifact and therefore retains that module name
for compatibility. Each way has a configured target-offset width, allowing
near targets to occupy narrower ways while a full-width way handles distant
targets. The port also includes the artifact's half-XOR, full-XOR, geometric,
Pearson, Rabin, SHA-3, and Zobrist tag variants. The artifact's plain,
half-XOR, full-XOR, and geometric two-level forms are included as well.

The final 4 KiB LiteBTB geometry can be selected as follows:

```json
{
  "ooo_cpu": [{
    "btb": "xbtbx",
    "btb_sets": 512,
    "btb_ways": 9,
    "btb_clipped_tag": true,
    "btb_target_sizes": [0, 4, 5, 7, 9, 11, 19, 25, 64],
    "btb_tag_size": 13
  }]
}
```

The final plain and Rabin-tag MICRO'26 configurations are in
[`IDUN_CONFIGS/litebtb_micro26`](IDUN_CONFIGS/litebtb_micro26/README.md). They
port the no-region state tagged `MICRO'26` at commit `12894621`; select the
Rabin version with `"btb": "rabin_xbtbx"`.

Build either experiment in the same way as any other ChampSim configuration:

```sh
./config.sh IDUN_CONFIGS/litebtb_micro26/litebtb_4k.json
make -j
```

# Using ChampSim

ChampSim is the result of academic research. If you use this software in your work, please cite it using the following reference:

    Gober, N., Chacon, G., Wang, L., Gratz, P. V., Jimenez, D. A., Teran, E., Pugsley, S., & Kim, J. (2022). The Championship Simulator: Architectural Simulation for Education and Competition. https://doi.org/10.48550/arXiv.2210.14324

If you use ChampSim in your work, you may submit a pull request modifying `PUBLICATIONS_USING_CHAMPSIM.bib` to have it featured in [the documentation](https://champsim.github.io/ChampSim/master/Publications-using-champsim.html).

# Download dependencies

ChampSim uses [vcpkg](https://vcpkg.io) to manage its dependencies. In this repository, vcpkg is included as a submodule. You can download the dependencies with
```
git submodule update --init
vcpkg/bootstrap-vcpkg.sh
vcpkg/vcpkg install
```

# Compile

ChampSim takes a JSON configuration script. Examine `champsim_config.json` for a fully-specified example. All options described in this file are optional and will be replaced with defaults if not specified. The configuration scrip can also be run without input, in which case an empty file is assumed.
```
$ ./config.sh <configuration file>
$ make
```

# Download DPC-3 trace

Traces used for the 3rd Data Prefetching Championship (DPC-3) can be found here. (https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/) A set of traces used for the 2nd Cache Replacement Championship (CRC-2) can be found from this link. (http://bit.ly/2t2nkUj)

Storage for these traces is kindly provided by Daniel Jimenez (Texas A&M University) and Mike Ferdman (Stony Brook University). If you find yourself frequently using ChampSim, it is highly encouraged that you maintain your own repository of traces, in case the links ever break.

# Run simulation

Execute the binary directly.
```
$ bin/champsim --warmup-instructions 200000000 --simulation-instructions 500000000 ~/path/to/traces/600.perlbench_s-210B.champsimtrace.xz
```

The number of warmup and simulation instructions given will be the number of instructions retired. Note that the statistics printed at the end of the simulation include only the simulation phase.

# Add your own branch predictor, data prefetchers, and replacement policy
**Copy an empty template**
```
$ mkdir prefetcher/mypref
$ cp prefetcher/no_l2c/no.cc prefetcher/mypref/mypref.cc
```

**Work on your algorithms with your favorite text editor**
```
$ vim prefetcher/mypref/mypref.cc
```

**Compile and test**
Add your prefetcher to the configuration file.
```
{
    "L2C": {
        "prefetcher": "mypref"
    }
}
```
Note that the example prefetcher is an L2 prefetcher. You might design a prefetcher for a different level.

```
$ ./config.sh <configuration file>
$ make
$ bin/champsim --warmup-instructions 200000000 --simulation-instructions 500000000 600.perlbench_s-210B.champsimtrace.xz
```

# How to create traces

Program traces are available in a variety of locations, however, many ChampSim users wish to trace their own programs for research purposes.
Example tracing utilities are provided in the `tracer/` directory.

# Evaluate Simulation

ChampSim measures the IPC (Instruction Per Cycle) value as a performance metric. <br>
There are some other useful metrics printed out at the end of simulation. <br>

Good luck and be a champion! <br>

### Trace-inferred wrong-path frontend

Set `wrong_path` in each `ooo_cpu` configuration (default: `false`):

```json
{
  "executable_name": "champsim-ti-wp",
  "ooo_cpu": [{"wrong_path": true, "wrong_path_history_size": 65536}]
}
```

Build with `./config.sh your-config.json && make`, then run with ordinary
ChampSim correct-path traces and the usual simulation arguments. Set
`"wrong_path": false` to run the original stall-on-misprediction frontend.
`wrong_path_history_size` bounds the per-core instruction history with FIFO
replacement; zero disables history recording. History is populated during
warmup and retained into simulation.

This implements frontend-only trace-inferred replay, as distinguished from
execution-driven wrong-path traces in
[Correct Wrong Path Simulation](https://webs.um.es/aros/papers/pdfs/cpepi-ispass26.pdf).
On a correct-path misprediction, trace consumption pauses while the frontend
follows its local direction predictor and BTB through previously seen instruction
PCs. Sequential successors are learned only from observed non-taken transitions;
no fixed instruction length, future trace lookahead, or stale branch outcome is
used to choose the replay path. Unknown PCs, targets, or fall-throughs stop replay
until branch recovery. Replay is enabled in detailed simulation; warmup retains
its existing fast behavior.

Replayed instructions use fetch bandwidth, I-cache/ITLB requests, DIB lookups,
decode bandwidth, and frontend buffer space. Wrong-path branch predictions notify
the instruction prefetcher. They wait at dispatch and never enter the ROB,
allocate registers, issue data accesses, train branch outcomes, or retire.
Decode/execute recovery discards all replay entries and applies the existing
misprediction penalty. Already-issued instruction requests and prefetches remain
in flight, preserving their cache effects. This models frontend effects only;
it does not model wrong-path data execution or predictor-history rollback.
The existing `fdip` prefetcher remains branch-callback driven, rather than a new
decoupled FTQ implementation.

Text and JSON CPU statistics report `wrong_path_fetched`, `wrong_path_squashed`,
and `wrong_path_unknown` (attempts to replay an unrecorded PC). These counters
reset at phase boundaries; instruction and IPC counts remain correct-path only.

The FDIP size sweeps pin L1I miss concurrency to 8 MSHRs and tag/fill bandwidth
to 2 per cycle for both conventional and UBS caches. Frontend instruction fetches
share a `FETCH_WIDTH` instruction budget per cycle, independently of the L1I
request budget. FDIP keeps the exact predicted virtual target PC and skips
queued targets already covered by a cache line, UBS predictor/sub-block bytes,
or the instruction-prefetch buffer. Presence probes do not train predictors,
change replacement state, consume buffered prefetches, or count as accesses.
