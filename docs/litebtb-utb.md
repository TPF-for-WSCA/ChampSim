# LiteBTB unique tag buffer

Set `btb_utb_entries` and `btb_utb_ways` to enable the UTB in any LiteBTB/XBTBX
variant. Set `btb_utb_replacement` to `"lru"` (the default) or
`"reference_count"`, then configure and rebuild the experiment as usual:

```json
{
  "ooo_cpu": [{
    "btb": "xbtbx",
    "btb_sets": 1024,
    "btb_ways": 8,
    "btb_clipped_tag": true,
    "btb_tag_size": 13,
    "btb_full_target_tag_size": 15,
    "btb_target_sizes": [0, 4, 5, 7, 9, 11, 19, 64],
    "btb_utb_entries": 640,
    "btb_utb_ways": 10,
    "btb_utb_replacement": "reference_count"
  }]
}
```

Switch the last field to `"lru"` to compare policies. BTB replacement remains
LRU. With `btb_utb_entries: 0` (the default) and `btb_utb_ways: 0`, the UTB is
disabled and the previous inline-tag behavior is retained. Legacy
`btb_tag_regions` settings are not used by this implementation.

The storage sweep generators in `IDUN_CONFIGS/storage_dse` and
`IDUN_CONFIGS/storage_dse_wrongpath` enable the UTB for LiteBTB and Combined,
using the following geometry from Table V of the
[LiteBTB paper](https://roman.brunner.best/assets/pdf/LiteBTB_MICRO26.pdf):

| BTB entries | UTB entries | UTB ways | UTB sets |
| --- | --- | --- | --- |
| 2K | 384 | 12 | 32 |
| 4K | 640 | 10 | 64 |
| 8K | 640 | 10 | 64 |
| 16K | 768 | 12 | 64 |
| 32K | 512 | 16 | 32 |

These sweeps default to `"lru"`. Their existing target widths and full-target
tag width are preserved. The paper uses eight compressed-target ways; the
sweeps retain their seven compressed ways and one full-target way. The
full-target way bypasses the UTB and retains its inline tag. Two-level
variants also retain full tags in the L1 BTB.

The UTB indexes with the low bits of the configured clipped (and, where
applicable, hashed) BTB tag and compares its remaining upper bits. BTB entries
store the resulting UTB slot pointer. A UTB miss is a miss for compressed ways;
BTB lookup compares pointers without validating their age, allowing the stale
pointer aliasing and tag resurrection described in the paper.

`reference_count` evicts the entry with the smallest count in the indexed UTB
set; ties use LRU. Empty slots are preferred. The counter increases when a BTB
entry acquires a pointer and decreases when that BTB entry is replaced,
invalidated, or migrated. Replacing a BTB entry with another referencing the
same resident tag has no net effect. Predictions do not increase the count.

A newly allocated UTB tag starts at **one**, counting only its inserting BTB
entry. UTB eviction retains stale BTB pointers but resets the counter. Old
pointers do not contribute to the replacement tag's counter, and their later
removal cannot decrement it. A revived stale pointer is counted only when a
BTB update attaches it to the current UTB allocation. Generation identifiers
track this ownership in simulator bookkeeping; they are not used to suppress
stale-pointer hits.

At decode, a branch-type mismatch or a decoded nonbranch invalidates the
predicted BTB entry. Prediction tokens pass through the FTQ and instruction
buffers, identifying the particular occupant seen at fetch. If intervening
training already replaced or repaired that occupant, decoding the old token
does not invalidate the newer entry. This preserves the simulator's existing
fetch-time BTB training behavior. Both ordinary decode and decoded-cache hits
perform scrubbing.

Startup storage reporting includes UTB upper tags, valid bits, LRU state,
reference counters (for `reference_count`), and BTB tag pointers. Counter width
can represent all BTB entries. Generation identifiers and prediction tokens
are simulator bookkeeping and are excluded from modeled storage; the count
policy is an experiment model, not a hardware implementation of stale-owner
tracking.
