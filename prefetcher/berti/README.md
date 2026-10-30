# Berti L1D port

Source: https://github.com/agusnt/Berti-Artifact
Commit: `5db245732198ee7cf67f91c5ce32b5a017ba3a4e`
File: `ChampSim/Berti/prefetcher/berti.l1d_pref` (Berti, not Berti+ or VBerti).
The upstream license texts are retained in LICENSE and LICENSE.champsim.

Select with:

```json
"L1D": {
  "prefetcher": "berti",
  "virtual_prefetch": true,
  "prefetch_activate": "LOAD,RFO"
}
```

The adapter preserves the original current-page, history, latency, recorded-page,
and IP tables, delta scoring, burst logic, and long-reuse predictions in
`algorithm.inc`. The tables are private to each module instance; their original
CPU dimension is a single slot. The original source's global CPU selector is now
an instance member. Simulator time is converted to local cache cycles, and the
new address API supplies virtual fill and eviction addresses. Critical-IP input
was unused upstream and remains zero. Queue checks use this cache's internal PQ
occupancy; issuance uses the current fill-this-level API and respects rejection.
Metadata passes through unchanged.

Requires 64-byte blocks, 4-KiB pages and virtual prefetching; initialization rejects
other configurations instead of silently training on incorrect addresses.
Berti's temporary delta arrays have an additional sentinel slot: upstream wrote
an end marker after eight results into an eight-element array.

The source is kept close to the artifact for review; its existing signedness and
narrowing warnings are not suppressed. This is a functional simulator port, not
a validation that published speedups reproduce on a different core/cache model.
