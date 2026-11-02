# FDIP instruction-cache sweep with fixed four-cycle timing

This directory copies `../fdip_icache_sizes_wrongpath`: seven conventional L1I
sizes (4, 8, 16, 32, 64, 128, and 256 KiB) and six UBS comparison points
(4 through 128 KiB), with wrong-path execution enabled.

Every configuration explicitly sets both L1I timing stages:

```json
"hit_latency": 4,
"fill_latency": 4
```

Every L1I tag lookup takes four cache cycles, including demand and prefetch
lookups. Fills wait four cache cycles after data returns from the next level.
These are separate stage delays: a miss also includes lookup, translation,
queueing, and downstream service time, so its complete latency is not fixed to
four cycles. Explicit hit and fill settings override the size-derived timing;
setting only `"latency": 4` would instead split four cycles between the stages.

L1I resources remain fixed at eight MSHRs, two tag checks per cycle, and two
fills per cycle. Cache geometry, replacement, prefetching, core settings, and
other cache levels match the source configurations. See the source directory's
README for the nominal UBS capacity convention and replacement policy.

Executables use the separate `bin/fdip_icache_wrongpath_fixed4/` directory.
Build with:

```sh
./idun_build.sh fdip_icache_sizes_wrongpath_fixed4
```

After the build job completes, submit the simulations with:

```sh
./idun_runner.sh fdip_icache_wrongpath_fixed4
```

The default results directory is
`/cluster/work/romankb/results/ipc1/fdip_icache_wrongpath_fixed4`.
