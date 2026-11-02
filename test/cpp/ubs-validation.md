# UBS small-scale validation — 2026-10-29

## Speculative replay dataflow — 2026-11-02

This supersedes the backend-NOP model described below. Replayed instructions now
retain their logical operands and execute through the ROB, physical register
file, scheduler, execution/completion stages, and load/store queues. Unknown PCs
use the user-selected basic-block heuristic: a load at entry, arithmetic reading
the latest load inside the block, and a store reading the latest register writer
at the predicted branch. Not-taken branches also start new blocks; a singleton
block uses the store rule. Addresses start at the most recently issued data
access plus 64 bytes and advance by 64 for subsequent replay memory operands.
The first address is `0x1000` if no prior data access exists.

Loads issue normal data-cache/translation reads and consumers wait for their
responses. Stores allocate SQ entries, execute, and forward to younger loads,
but never commit data-cache writes. All replay remains non-retiring. Recovery
cleans forwarding references before releasing LQ slots, removes speculative SQ
and ROB entries, and rolls back register allocation. A checkpoint at the first
replayed rename preserves older in-flight producers; source-only speculative
allocations are reclaimed as well. The two inferred logical registers do not
alias any trace register and share the normal physical-register capacity.

Backend verification also exposed an existing scheduler defect: it rechecked
already-scheduled, physically renamed operands as logical register IDs and
charged their destination registers again. This could stop scanning under
register pressure, even when the next instruction needed no new registers.
Allocation checks now apply only to unscheduled instructions, and renaming
cannot pass an older unready instruction. This correction applies with replay
disabled too; old and new performance results require matching baseline reruns.

The fresh assertion-enabled runner in `/tmp/champsim-wrongpath-data-20261102/`
covers frontend/FDIP/replay, branch predictors, ideal frontend, register renaming,
ROB/load scheduling, cache tests 400–432, UBS, and SRRIP. The new dataflow tests
cover taken/not-taken boundaries with and without FDIP, load-to-ALU-to-store
readiness, actual data requests, queue capacity, recovery with older producers,
256 repeated register exhaustion/recovery cycles, forwarding-slot reuse, late
memory responses, logical/physical ID separation, and register-free consumers
under register pressure. The full Python suite passed 264 tests with one skip.

Final C++ result: **152 cases / 27,273 assertions passed**. The new
register-pressure regression fails against the old scheduler (zero instructions
renamed when one register-free consumer should rename), then passes with the
correction. The Python result is **263 passed / one skipped**.

Six fresh production configurations (conventional/UBS with replay disabled,
enabled, and enabled with zero history) completed `server_004` with 100K warmup
and 250K measured instructions. The normal conventional replay run dispatched
596,728 instructions, executed 114,332, issued 21,368 loads, and executed 4,028
stores. UBS dispatched 589,523, executed 113,274, issued 21,144 loads, and
executed 3,908 stores. Both zero-history variants also generated and executed
inferred loads, arithmetic and stores. Disabled runs report zero replay counters.
Artifacts and build logs are in `/tmp/champsim-wrongpath-data-smoke-20261102/`.
Production binaries under `bin/` and existing results were not overwritten.

Same-build original-core and old-scheduler-only controls reproduce identical
replay-disabled counters, isolating the baseline change to the scheduler fix.
For this short `server_004` window, conventional 4 KiB increases from 949,260 to
949,330 cycles, while UBS 4 KiB increases from 949,172 to 953,237 cycles. These
are early-window functional controls, not replacements for the 50M-warmup/50M-ROI
performance runs. Changing backend scheduling can affect cache and prefetch
timing even when the corrected rule permits more scheduling progress.

## DIB branch-target recovery — 2026-11-01

Decoded-instruction-buffer hits previously skipped the recovery check applied
to freshly decoded instructions. Direct jumps, direct calls and conditional
branches with correct direction but incorrect targets therefore waited for
execution only when they hit the DIB. Both paths now run the same target-error
recovery before dispatch; conditional direction errors still await execution.
The existing deferred squash removes replay after the decode-buffer iteration,
and clearing the branch's misprediction flag prevents a second redirect at
execution. DIB remains enabled and recovery delays are unchanged.

The regression checks all three branch classes with DIB hit/miss and replay
enabled/disabled, including FDIP redirection and replay squash. All six DIB-hit
combinations fail before the fix. A separate check retains execution-stage
recovery for direction errors. The two new cases pass 261 assertions; the
isolated runner in `/tmp/champsim-wrongpath-nop/` passes 128 cases / 25,797
assertions after this change.

## Wrong-path backend capacity — 2026-11-01

Wrong-path replay previously stopped at dispatch. Its 32-entry dispatch buffer
could fill while the older mispredicted branch waited on execution, even though
the 352-entry ROB had spare capacity. Replay now enters the ROB as backend NOPs,
consuming normal dispatch, scheduler, execution, completion and ROB resources.
Known branch identity still directs replay predictions, but recorded registers,
memory addresses and pipeline state are discarded: they describe a different
dynamic execution. Dispatch enforces the no-data-operands invariant again.
Recovery squashes replay from both frontend buffers and the ROB, and retirement
explicitly excludes wrong-path instructions. Completion walks the ROB by index
so branch recovery may safely remove its younger tail.

This remains an approximation without speculative data execution or register
dependencies. Unknown PCs are NOPs, and no unavailable addresses are invented.
FDIP already follows predicted control flow with `wrong_path=false`, so that
configuration still incurs wrong-path instruction prefetch traffic; the option
adds instruction demand fetches and replay occupancy. A small difference between
the two policies is not by itself evidence of missing wrong-path prefetching.

New regressions fill a 128-entry ROB behind a pending branch with a four-entry
dispatch buffer, complete and squash replay without retiring it, verify empty
data queues and unchanged physical-register availability, and discard stale
historical pipeline state. The capacity test fails against the prior core
(one ROB entry versus the required 128), establishing that it detects the
dispatch-only cap. The isolated assertion-enabled runner in
`/tmp/champsim-wrongpath-nop/` passes 121 cases / 25,500 assertions, including
22 wrong-path cases / 22,022 assertions. Full workload effects require reruns;
these checks establish resource accounting and recovery invariants.

## Demand coverage through prefetch merges — 2026-11-01

Prefetch probe masks previously flowed into the fill's `requested_bytes` union
when a demand and prefetch merged. UBS then treated predicted but unused bytes
as demand-accessed data. For example, a 32-byte prefetch probe merging with a
four-byte demand could allocate a 32-byte fragment instead of a four-byte
fragment when the predictor was drained. This reduces effective capacity.

Fill records now retain demand byte coverage only; tag lookups still use the
prefetch's complete probe mask. The regression exercises both merge orders and
demand offsets 0 and 60, verifies one lower request, the retained demand mask,
and the eventual single four-byte fragment. The unpatched constructor fails
all four generated combinations; the corrected case passes 64 assertions.
Rebuilt cache and UBS-test objects linked into the isolated parity runner pass
**107 cases / 3,562 assertions**, including **18 UBS cases / 2,018 assertions**.
Builds and before/after logs are in `/tmp/ubs-coverage-tests/`.

This fixes artificial fragment inflation; it does not force IPC to track a
twice-sized conventional cache. The configured layout has 17 tags per set
including the predictor, but retains only four full-line regions, compared with
eight in the same-size conventional cache. Dense code, disjoint useful regions,
replacement and prefetch timing can produce real slowdowns. Fresh workload
measurements are necessary to quantify this fix.

## Baseline/UBS frontend parity — 2026-11-01

The two size sweeps already match outside cache organization: each has six
baseline/UBS pairs (4–128 KiB) and an unpaired conventional 256 KiB cache.
The wrong-path and non-wrong-path sweeps differ only in executable names and
`ooo_cpu[0].wrong_path`. Twenty storage-DSE pairs also match outside L1I
organization. New Python checks compare both source JSON and resolved simulator
elements, including cores, lower caches, translations, queues and modules.
No experiment JSON changes were necessary.

The implementation previously selected different FDIP behavior using `ubs`:
conventional caches deduplicated queued targets by line, aligned prefetch
addresses to the line, and retained recently prefetched lines after completion;
UBS deduplicated by instruction PC, preserved offsets and forgot completed
requests. CPU demand fetches also carried requested-byte masks only for UBS.

Both caches now use the existing UBS-compatible FDIP rules. Queued targets retain
their instruction PCs, issued requests suppress duplicates by line, and fills
or prefetch hits remove the recent entry so later predictions recheck residency.
Both predicted-target and miss-triggered next-line requests explicitly carry
four-byte coverage through the cache prefetch API. Demand fetches describe the
same instruction-byte union with either cache, including wrong-path fetches.
There are no cache-organization branches in FDIP or CPU fetch packet generation.

UBS integration remains active inside the cache: virtual-PC presence probes
check the union of resident fragment bytes; partial instruction coverage remains
a miss; full-line predictor/buffer hits and useful-byte training are unchanged.
Conventional caches still satisfy accesses with full lines. The fragment sizes,
bounded SRRIP replacement, predictor, and effective storage remain inherent
organization differences. In particular, the nominal capacity labels are not
exact equal-data-byte budgets: conventional storage is 512 bytes per set, while
UBS has 444 fragment bytes plus a 64-byte predictor per set. Both use the same
32-entry prefetch buffer.

Fresh C++ compilation with libstdc++ assertions enabled passed **106 cases /
3,498 assertions**, covering fetch, wrong-path replay/recovery, FDIP, cache tests
400–432 and replacement tests 442–444. New regressions compare queue contents,
request addresses and byte masks, completion suppression, next-line requests,
and runtime timing/resources at all six matched sizes. Existing UBS coverage
and partial-miss regressions continue to pass. Python passed **261 tests with
one skip**.

All four 4 KiB executables were freshly configured and built from the same
sources in `/tmp/champsim-icache-parity/experiment/`. Each completed `server_004`
with 100,000 warmup and 1,000,000 ROI instructions. Non-wrong-path runs fetched
zero wrong-path instructions; both enabled runs fetched and squashed wrong-path
instructions. UBS reported partial misses in both modes, while conventional
caches reported zero. Logs, JSON results and the isolated C++ runner are in
`/tmp/champsim-icache-parity/`. These are functional smoke runs; different cache
timing can naturally change dynamic fetch/prefetch counts and wrong-path length.
Existing binaries under the repository's `bin/` were not replaced and must be
rebuilt before collecting comparable results.

## Dummy PCs and SRRIP validation — 2026-10-31

Restored the historical translated-IPC1 convention: PCs not divisible by four
are dependency-only dummy instructions. Fetch marks them complete without an
I-cache request, I-cache fetch bandwidth, return dependency, or useful-byte mask.
Their instruction IDs and source/destination registers remain intact, and they
continue through normal decode/backend processing. Request grouping excludes
dummies from its instruction budget. Fetch test fixtures now use aligned PCs
when representing real instructions.

The working-tree bounded SRRIP implementation is connected to both size-sweep
configurations. A Python regression parses every JSON in both sweep directories
and verifies generated `.replacement<class srrip>()`, plus UBS enablement and
geometry where selected. UBS runtime regressions verify bounded aging, RRPV-2
insertion, RRPV-0 promotion of covering fragments, and fitting-invalid-way
selection outside the victim window. Conventional regressions verify insertion
into both empty and replaced ways. The explicit SRRIP fill hook prevents the
legacy update-only adapter from suppressing insertion updates; miss lookups do
not index a nonexistent victim.

A fresh isolated C++ build with libstdc++ assertions enabled passed **35 cases /
2,398 assertions**, covering tests 150, 151, 409, 410, 442, 443 and 444. Python
configuration/instantiation tests passed **144 tests with one additional skip**.
Fresh conventional and UBS binaries generated from the wrong-path 4 KiB JSONs
both completed a server_004 smoke run with 10,000 warmup and 50,000 ROI
instructions. The UBS runtime storage report charges 256 replacement bits for
128 fragments (two-bit SRRIP), and the binary contains the bounded-victim and
explicit SRRIP fill hooks. Smoke artifacts are in
`/tmp/champsim-dummy-pc-repair/`; this run is too short for an IPC comparison.
Existing experimental binaries must be reconfigured and rebuilt before reruns.
These tests establish functional behavior, not full-workload IPC equivalence.

## Partial-miss residency with concurrent refills

Partial-miss acceptance no longer invalidates resident fragments. Covered
requests hit during an outstanding refill, while uncovered requests merge and
wait for the returning data. Existing refill handling snapshots and removes
surviving fragments immediately before installing the predictor victim and
new predictor line. Per-MSHR coverage snapshots still survive intervening normal
replacement; fragments are not pinned.

The regression exercises 2- and 8-entry MSHR configurations, two independent
partial misses, covered hits with both misses outstanding (including a full
2-entry MSHR), same-line uncovered merging, and younger-before-older refill
completion. It checks that pending requests do not complete prematurely, refill
leaves no duplicate fragments, and both lines retain their byte coverage. The
existing snapshot regression also forces an intervening eviction before a
second merged request. Focused frontend/FDIP/UBS/prefetch-buffer regression:
**40 cases / 2,440 assertions passed**.

## Shared instruction-prefetch buffer — 2026-10-29

All L1 instruction-cache organizations now default to a 32-entry fully
associative FIFO prefetch buffer. Unused prefetch fills stay outside the cache
and UBS predictor. A demand miss that hits the buffer consumes and promotes the
line through normal cache insertion; subsequent accesses use the cache.
Prefetch probes do not refresh FIFO order. Demand/prefetch MSHR merges retain
direct demand insertion. UBS prefetch partial misses leave existing fragments
intact until demand promotion, which preserves their useful-byte masks.

`L1I.prefetch_buffer_size` overrides the entry count; zero restores direct
prefetch insertion. Data-cache defaults remain unchanged. The default buffer
uses 2 KiB of data and 2.357 KiB including modeled tags, module metadata, valid
bits, FIFO pointers and occupancy. The separate `Prefetcher <name> prefetch
buffer STORAGE` report charges this to the prefetcher budget and does not add it
to I-cache capacity or the L1I + BTB storage plot.

Validation: a freshly compiled runner containing existing cache/prefetch tests
400–432 and the UBS tests passed **68 cases / 2,139 assertions**. New tests cover
both conventional and UBS promotion, persistent residency after buffer overflow,
FIFO probe behavior, unused eviction accounting, invalidation, demand merging
into an inflight prefetch, zero-sized fallback, and UBS fragment preservation.
Configuration-generation and storage-DSE Python tests ran **80 tests**, with
**79 passing and one skipped**. No workload IPC claim is made by these checks.
The focused runner compiled the simulator sources (excluding main and generated
environment), test helpers, LRU and no-prefetcher modules in an isolated `/tmp`
build. Existing experiment binaries must be rebuilt before using this behavior.

## Historical LRU2BOUND4 restoration

The working tree now restores the historical global fitting-empty-slot search,
size-transition victim windows, rank-14 insertion for 16-way sets, and rank-zero
hit promotion. The focused UBS runner passes 11 cases / 1,266 assertions; the
broader cache runner passes 85 cases / 2,310 assertions. The broader runner links
existing cache-test objects with the rebuilt UBS test and cache implementation.
Source-extracted comparison matches legacy occupancy and saturated victim
eligibility for all nine region sizes. Separate tests verify insertion-rank
updates, hit promotion, and byte-mask preservation through merged partial misses.

Four-byte cyclic working sets of 17 lines per set now hit throughout the second
pass, including the predictor, with the default bound of four. Full-line capacity
remains four lines per set. Separate prefetch-buffer and merge-register behavior
has not been restored by this replacement-policy change.

The following sections preserve the **pre-restoration** measurements and findings.

## Pre-restoration measurements

The current placement policy can severely underutilize the configured capacity.
All ways are reachable, but a fixed region size reaches only its candidate
window. The checks did not find a wiring failure in the exercised paths.

## Test result

`409-ubs-cache.cc`: **11 test cases, 18,172 assertions passed**. Seven new cases
extend the four existing cases. Parameter generators exercise different sizes,
candidate counts and conventional-cache associativities within those cases.

The focused runner used the repository's Catch test main and freshly rebuilt
cache/frontend objects, with the UBS test translation unit compiled at `-Og -g0`.
The full test-runner build was interrupted because unrelated translation units
were slow to build. The complete simulator suite was not run. The standard
command for these tests is:

```sh
make test TEST_NUM=409 -j4
```

Covered behavior:

- 65 sequential unique-line requests per size/candidate-count combination;
  region sizes 4, 8, 12, 16, 24, 32, 36, 52, 64; candidate counts 1, 2, 4, 16.
  Every insertion stayed in its expected window. Within that window, insertion
  counts differed by at most one under equal-size streaming pressure.
- Mixed region sizes populated all 16 physical ways.
- Two passes over cyclic working sets of 5, 6, 8, 9, 16 and 17 lines, for
  four-byte and full-line regions, against 8-way and 16-way conventional caches.
- Hits refresh sub-block LRU; predictor hits do not evict sub-blocks.
- Same-line outstanding misses merge into one lower request and retain both
  requested masks. Subsequent predictor hits add useful bytes. Eviction retains
  the union, including bytes at the end of the line.
- A partial miss snapshots all physically resident bytes, including bytes not
  requested by either incoming access. The snapshot survives fragment
  invalidation and a second merged miss, then returns to the predictor on fill.
  Evicting that predictor without re-accessing the old bytes retains the complete
  union in sub-block storage, and a subsequent combined request hits.
- The frontend emits the union of four-byte instruction masks and clips at
  the line boundary; dependent instruction IDs accompany the request.
- Existing cases cover partial misses, preserving old fragments through partial
  fills under occupied candidates, union coverage across sub-blocks, conventional
  behavior, and rejection of unsupported layouts.

## Measured capacity

The experiment layout is
`[4,4,8,8,8,12,12,16,24,32,36,36,52,64,64,64]`.
One set has 444 bytes of sub-block data plus a 64-byte predictor line, compared
with 512 bytes in an eight-way conventional set.

| Organization | Requested region | Steady cyclic capacity per set |
| --- | ---: | ---: |
| UBS, 4 candidates (current configuration) | 4 bytes | 5 lines |
| UBS, 16 candidates (diagnostic variant) | 4 bytes | 17 lines |
| UBS, either candidate count | 64 bytes | 4 lines |
| Conventional, 8 ways | Either | 8 lines |
| Conventional, 16 ways | Either | 16 lines |

For each tested working set at or below these thresholds, the second pass had
zero lower requests. Above the threshold, every access in the second pass
missed. These are serialized synthetic capacity checks, not IPC predictions.

The default candidate windows, using zero-based way indices, are:

| Region size | Candidate ways |
| ---: | --- |
| 4 | 0–3 |
| 8 | 2–5 |
| 12 | 5–8 |
| 16 | 7–10 |
| 24 | 8–11 |
| 32 | 9–12 |
| 36 | 10–13 |
| 52 | 12–15 |
| 64 | 13–15 |

`CACHE::install_ubs_subblock` finds the first way large enough and uses only
that way and the next `candidate_ways - 1` ways, clipped at the end of the set.
Consequently, a stream of four-byte regions leaves 12 ways unused despite
balanced replacement among its four candidates. For 64-byte regions only three
physical ways fit, regardless of the candidate count.

## Predictor and register interpretation

The implementation's `ubs_predictor` is one tagged, full-line buffer per set,
with an accumulated useful-byte mask. A conflicting fill drains its predecessor
into sub-block ways. A matching predictor line supplies any bytes in the full
line and records demand accesses. There is no separate learned size-prediction
table or explicit intermediate hardware register bank in this UBS path.
The tests verify the modeled state transfers through frontend requests,
tag lookup, merged miss/fill state, predictor and sub-block storage. They do not
establish conformance to an external UBS hardware design.

The frontend approximates every instruction as four bytes because traces lack
decoded instruction lengths. This assumption can affect fragmentation and
candidate pressure on variable-length instruction workloads.

## Implication

Twice as many physical tags do not guarantee twice the usable working-set
capacity: candidate restrictions, multiple fragments per line and the region
size distribution matter. The wider-window diagnostic recovers the expected
entry-count advantage for small regions, which points to placement pressure
as a plausible contributor to the unexpected results. It does not prove that
this explains the measured workload IPC gap.

No cache algorithm or experiment configuration was changed. Before rerunning
large experiments, compare the intended UBS candidate-selection rule with this
fixed contiguous-window rule and validate placement on real workload region-size
distributions. Increasing the window is a useful experiment, but changes the
modeled design and is not established here as the correct architectural fix.

The subsequent [legacy comparison](../../experiments/ubs_legacy_comparison.md)
confirms that the old implementation searched all fitting invalid ways, used
different saturated victim windows and insertion ranks, and included separate
prefetch-buffer and merge-register paths. The results above describe the current
port, not the behavior of historical UBS.
