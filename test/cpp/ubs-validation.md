# UBS small-scale validation — 2026-10-29

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
