# Pythia L2 port

Source: https://github.com/CMU-SAFARI/Pythia
Commit: `2626efe006a525dd9318e653b9c0198141d72f4d`
The upstream MIT license is retained in LICENSE. The artifact calls the Pythia
implementation `Scooby`; its printed statistics retain that name.

Select with:

```json
"L2C": {
  "prefetcher": "pythia",
  "virtual_prefetch": false,
  "prefetch_activate": "LOAD,RFO,PREFETCH"
}
```

`upstream/` contains the original SARSA/featurewise learning engines, tiled
Q-tables, state features, action selection, dynamic degree, reward/tracking logic,
and helper functions, isolated in namespace `pythia_impl`. Each module owns its
own engine, tables and random generators. `knobs.cc` freezes defaults plus the
artifact's `config/pythia.ini` (retained as `published.ini`); action count is derived
from its 15 actions. The artifact's unrelated simulator and INI/command-line
framework is not imported. Parameters can be changed in `upstream/knobs.cc`.
Detailed state/action diagnostic collection is disabled to avoid unbounded debug
maps; this does not change learning or predictions.

The adapter follows `multi.l2c_pref`: predictions target L2, and training tracks
predictions even if the PQ rejects issuance. Fill callbacks mark timeliness;
useful-prefetch demand hits feed accuracy. Demand loads, RFOs and incoming
prefetches train the engine. The current cache API suppresses callbacks for this
cache's own prefetch probes; the original separate prefetch-hit notification is
only available for incoming prefetch hits here. The upstream `pf_cache_hit` field
is diagnostic and is not consulted by its learning algorithm. A compatible four-argument `CACHE::prefetch_line` overload preserves the issuing
PC on requests from both ported prefetchers for downstream PC-based training. Metadata passes through unchanged. Blocks must be 64 bytes and pages 4 KiB.

System feedback uses actual DRAM read-queue enqueue bytes and aggregate peak bus
bandwidth, sampled every 256 local L2 cycles, with the artifact's half-previous
plus-current smoothing and four quartiles. `inc/prefetch_bandwidth.h` provides the
small simulator-wide counter, reset by memory-controller initialization; warmup
bypasses are excluded. Multiple channels contribute to the peak bandwidth.
Prefetch accuracy uses the original 1024-cycle interval, half-old/half-new
smoothing, and ten buckets. Unlike the original framework's core-cycle broadcast,
these intervals follow the configured L2 clock. Core IPC is unused by the chosen
PC/PC-delta features and is not broadcast. The upstream high-bandwidth reward
threshold remains 4 even though bandwidth levels are 0--3, matching its defaults;
bandwidth still affects dynamic action fallback.

Port corrections: value-initialize statistics containing vectors instead of
zeroing their object representation; initialize accuracy and per-page prediction
counts; retain the tracker pointer before deque erasure; fix enum-to-string table
indices; free untracked prediction states, predictor trackers, Q-tables, and random
distributions on teardown.
Private headers use `.hpp` so configuration generation includes only `pythia.h`.
This port has not been performance-validated against the paper's simulator.
