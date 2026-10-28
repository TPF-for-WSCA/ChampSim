# FDIP instruction-cache size sweep

This experiment contains paired conventional and UBS instruction-cache points
at nominal sizes from 4 KiB through 128 KiB. Both organizations use the same set
count for each nominal size:

| Nominal size | Sets |
| ---: | ---: |
| 4 KiB | 8 |
| 8 KiB | 16 |
| 16 KiB | 32 |
| 32 KiB | 64 |
| 64 KiB | 128 |
| 128 KiB | 256 |

The conventional configurations use eight 64-byte ways (512 bytes) per set. UBS
ignores that conventional `ways` geometry and uses 444 bytes of fixed uneven-way
capacity plus one 64-byte predictor entry (508 bytes) per set; therefore, the size
in a UBS filename is the nominal, set-scaled comparison point rather than the exact
sum of data bytes.

Each UBS configuration specifies its uneven layout with `ubs_way_sizes`. Change
that array to experiment with another layout; every entry is a way capacity in
bytes. The entries must be positive, sorted from smallest to largest, and the last
entry must be 64. The number of array entries determines the number of UBS ways.
The pre-existing conventional 256 KiB point remains available as an additional
unpaired reference.

Build and run every JSON configuration in this directory with:

```sh
./idun_build.sh fdip_icache_sizes
./idun_runner.sh fdip_icache
```

The result directory can then be passed to
`aggregation_scripts/plot_ipc.py`. The script uses the conventional 16 KiB
configuration as its default relative-performance baseline when both cache
organizations are present.

Every figure also gets an exact plot-data CSV: for example,
`ipc_comparison.pdf` produces `ipc_comparison_plot.csv`, and
`ipc_comparison_relative.pdf` produces `ipc_comparison_relative_plot.csv`.
These contain one row per configuration/category bar, including the plotted
value, trace count, metric, and storage-axis label. Values retain full float
precision. The existing `.csv` and `_relative.csv` files retain the underlying
per-trace values, also without decimal rounding.

IPC bars use the harmonic mean across traces in each category. Relative bars
use `100 * (harmonic_mean(IPC / baseline_IPC) - 1)`, so negative percentage
changes are handled through positive speedup ratios. Exact plot CSVs use these
same aggregates; per-trace exports remain unchanged.
