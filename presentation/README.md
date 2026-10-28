# Presentation IPC graph

Generated from `ubs_ipc_relative_plot.csv`, preserving its harmonic-mean values,
baseline and workloads. Nominal capacity labels are taken from configuration
names, with no halving of UBS sizes. The 2 KiB and 256 KiB axis groups are
excluded. Remaining groups are 4, 8, 16, 32, 64, and 128 KiB.

- `ubs_ipc_relative.pdf` / `.svg`: vector figures for slides.
- `ubs_ipc_relative.png`: raster preview.
- `ubs_ipc_relative_excel.csv`: chart-ready wide table, full numeric precision.
- `ubs_ipc_relative_plot.csv`: exact long-form data for every bar.

In Excel, import the Excel CSV (Data → From Text/CSV, comma delimiter), select
the table, and insert a **Clustered Column** chart. The first column supplies
nominal capacity labels; the other eight columns are the plotted series. Assign the same
color to each workload's Conventional/UBS pair, distinguishing UBS with a pattern
fill or lighter shade. Values are already percentage-point numbers: use a number
axis or custom format `0.0"%"`, rather than Excel's percentage format, which
would multiply them by 100. Both Conventional and UBS are included at every capacity from 4 through 128 KiB.
These labels describe nominal cache capacity, not total hardware storage including
metadata. UBS uses 508 data/predictor bytes per set versus 512 conventional data
bytes; the separate storage-DSE plot uses measured total storage.

Regenerate from the repository root:

```bash
python3 aggregation_scripts/plot_ubs_presentation.py
```
