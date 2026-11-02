# Experiment plots

`plot_ipc.py`, `plot_storage_dse.py`, and `plot_ideal_frontend.py` automatically
produce L1I MPKI plots alongside their existing IPC plots. Their commands and
trace-selection rules are unchanged.

Relative IPC graphs automatically use the smallest conventional cache present in
the result set (for example, 4 KiB when the sweep contains 4–256 KiB points).
UBS configurations do not determine the reference size. The chosen baseline is
used for every workload category, figure label, and relative CSV export.
`--baseline <configuration>` selects an explicit IPC reference instead. Missing
conventional capacities or ambiguous smallest baselines require this override.

For output `graphs.png` (or an output prefix `graphs`), the extra files are:

- `graphs_mpki.png`: arithmetic mean L1I LOAD MPKI, overall and by workload group.
- `graphs_mpki_reduction.png`: percentage reduction relative to the comparison
  configuration; positive values mean fewer misses.
- `graphs_mpki_plot.csv` and `graphs_mpki_reduction_plot.csv`: exact plotted
  values and trace counts, without rounding.
- `graphs_mpki.csv`: per-trace MPKI, comparison configuration, comparison MPKI,
  per-trace reduction, and source JSON path.
- `graphs_mpki_baselines.csv`: the comparison configuration used for every variant.

The generic script uses the requested figure extension. Storage DSE and ideal
front-end studies produce both PNG and PDF figures.

### Interactive storage DSE and presentation layers

`plot_storage_dse.py` also produces a self-contained Plotly HTML file. It needs
the `plotly` Python package in addition to Matplotlib. Open the HTML file locally
in a browser; no server or network connection is needed. The nominal L1I capacity
and BTB entry-count selectors apply together to Baseline, UBS, LiteBTB, and
Combined. These selectors refer to nominal capacity and entry count; the x-axis
still shows the measured total storage, including metadata. The per-design point
counts show when a selected comparison is missing from a partial sweep. Legend
clicks toggle a design in both panels, and hovering shows the configuration,
storage breakdown, IPC, and workload count.

For output prefix `storage_dse`, the script writes:

- `storage_dse.html`: interactive IPC and IPC/MiB scatter plots with size filters.
- `storage_dse.png` and `.pdf`: the existing complete static figure.
- `storage_dse_<design>.png` and `.pdf`: one design with transparent background,
  axes, and its legend entry (`baseline`, `ubs`, `litebtb`, or `combined`).
- `storage_dse_<design>_layer.png` and `.pdf`: transparent points-only layers.
- `storage_dse_axes.png` and `.pdf`: transparent axes, title, and full legend,
  without points, to use as the common presentation backdrop.

All static figures and layers share identical canvas dimensions, axis limits,
and plot positions. Stack the axes layer and any design point layers at the same
position and size, without cropping, to reveal designs independently during an
animation. Interactive filters change the HTML view; static exports contain the
complete retained sweep.

The right panel and the CSV `ipc_per_mib` column report ROI IPC divided by total
storage in MiB (`IPC * 1024 / total_storage_kib`). The storage x-axis remains in KiB.

```bash
python3 aggregation_scripts/plot_storage_dse.py \
  /cluster/work/romankb/results/ipc1/storage_dse \
  --allow-incomplete --output presentation/storage_dse
```

MPKI is `1000 * ROI L1I LOAD misses / retired ROI instructions`, pooling phases
before dividing. LOAD misses include merged requests and speculative fetches;
this is not a count of unique lower-level transactions or correct-path-only
misses. Missing counters and nonpositive instruction counts are N/A, not zero.

For a category, reduction is
`100 * (1 - mean(variant MPKI) / mean(comparison MPKI))`, with equal weight per
trace. It is a ratio of arithmetic means, not an average of trace percentages.
The variant and comparison use the same paired traces. Missing comparison
results, unavailable counters, or zero comparison mean MPKI produce N/A.
For the generic cache-size script, `--allow-incomplete` allows absolute means
to cover different trace sets; reduction means still use matched pairs.
For `plot_storage_dse.py`, `--allow-incomplete` skips missing configurations and
configurations with fewer workloads than the maximum found in the sweep. The
remaining configurations must cover identical trace sets. Skipped configurations
are reported on stderr and excluded from the IPC and MPKI plots and CSV exports.

Comparison configurations:

- Cache-size sweeps: the unique conventional cache at the same nominal capacity.
  Conventional bars therefore show zero reduction. When there is a unique
  smallest conventional configuration, an additional
  `graphs_conventional_vs_<size>k_mpki_reduction.png` compares conventional cache
  sizes against that baseline, with matching MPKI figures and CSV exports.
  This comparison is independent of the IPC `--baseline` override. For configuration names
  without a capacity, the generic script uses its selected IPC `--baseline`.
- Storage DSE: `baseline_i<size>k_b<entries>k`, matching both nominal I-cache
  capacity and BTB entry count.
- Ideal front end: `baseline`.

`plot_ubs_presentation.py` also accepts the MPKI `_plot.csv` files through
`--input`; it preserves their metric label and handles N/A values. For example:

```bash
python3 aggregation_scripts/plot_ubs_presentation.py \
  --input graphs_mpki_reduction_plot.csv --output presentation/ubs_mpki_reduction
```
