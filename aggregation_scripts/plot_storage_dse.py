#!/usr/bin/env python3
"""Plot storage DSE performance, interactive comparisons, presentation layers, and L1I MPKI."""
import argparse
import csv
import html
import json
import math
import statistics
import os
import tempfile
import re
import sys
from pathlib import Path

from plot_ipc import load_result

DESIGNS = {"baseline": ("Baseline", "#0072B2", "o"), "ubs": ("UBS", "#D55E00", "s"),
           "litebtb": ("LiteBTB", "#009E73", "^"), "combined": ("Combined", "#CC79A7", "D")}
METRICS = (("ipc", "Harmonic mean ROI IPC"), ("ipc_per_mib", "Harmonic mean ROI IPC / MiB"))
STORAGE_LABEL = "Total L1I + BTB storage (KiB, including metadata and predictors)"
HEADER = re.compile(r"^(.*?) STORAGE .*total=(\d+) bits", re.M)


def storage_from_log(path):
    text = path.read_text()
    if "Simulation complete" not in text:
        raise ValueError(f"incomplete simulation: {path}")
    if any("cpu0_L1I " in line and "partial:" in line for line in text.splitlines()):
        raise ValueError(f"partial L1I storage model: {path}")
    l1i, btb = [], []
    for name, bits in HEADER.findall(text):
        if name.startswith("cpu0_L1I "):
            l1i.append(int(bits))
        elif name.startswith("CPU 0 ") and ("LiteBTB" in name or "basic_btb" in name):
            btb.append(int(bits))
    if len(l1i) != 1 or len(btb) != 1:
        raise ValueError(f"expected exactly one L1I and one BTB storage report: {path}")
    return l1i[0], btb[0]


def collect(results, configs, allow_incomplete=False):
    points = []
    for config_path in sorted(configs.glob("*.json")):
        config = json.loads(config_path.read_text())
        name = Path(config["executable_name"]).name
        match = re.fullmatch(r"(baseline|ubs|litebtb|combined)_i(\d+)k_b(\d+)k", name)
        if match is None:
            raise ValueError(f"unexpected configuration name: {name}")
        files = sorted((results / name).rglob("*.json"))
        if not files:
            if allow_incomplete:
                print(f"Skipping missing configuration: {name}", file=sys.stderr)
                continue
            raise ValueError(f"no results for {name}; use --allow-incomplete for a partial sweep")
        rows = []
        seen = set()
        for path in files:
            trace, group, ipc = load_result(path, 0)
            if trace in seen:
                raise ValueError(f"duplicate trace {trace} for {name}")
            seen.add(trace)
            l1i, btb = storage_from_log(path.with_suffix(".log"))
            if not math.isfinite(ipc) or ipc <= 0:
                raise ValueError(f"invalid IPC: {path}")
            total = (l1i + btb) / 8192
            rows.append(dict(configuration=name, design=DESIGNS[match[1]][0],
                             nominal_icache_kib=int(match[2]), btb_entries=int(match[3]) * 1024,
                             trace=trace, group=group, l1i_bits=l1i, btb_bits=btb,
                             total_storage_kib=total, ipc=ipc, ipc_per_mib=ipc * 1024 / total))
        if len({(r["l1i_bits"], r["btb_bits"]) for r in rows}) != 1:
            raise ValueError(f"storage changes between traces for {name}; mixed/stale binaries")
        points.append(rows)
    if not points:
        raise ValueError("no results found")
    if allow_incomplete:
        max_workloads = max(len(rows) for rows in points)
        for rows in points:
            if len(rows) < max_workloads:
                print(f"Skipping incomplete configuration: {rows[0]['configuration']} "
                      f"({len(rows)}/{max_workloads} workloads)", file=sys.stderr)
        points = [rows for rows in points if len(rows) == max_workloads]
    expected = {r["trace"] for r in points[0]}
    for rows in points:
        if {r["trace"] for r in rows} != expected:
            raise ValueError(f"unbalanced trace coverage for {rows[0]['configuration']}; compare identical trace sets")
    summaries = []
    for rows in points:
        first = rows[0]
        ipc = statistics.harmonic_mean(r["ipc"] for r in rows)
        summaries.append({k: v for k, v in first.items() if k not in ("trace", "group", "ipc", "ipc_per_mib")} |
                         dict(trace_count=len(rows), ipc=ipc, ipc_per_mib=ipc * 1024 / first["total_storage_kib"]))
    order = lambda r: (r["total_storage_kib"], r["design"], r["configuration"], r.get("trace", ""))
    return sorted(summaries, key=order), sorted([r for rows in points for r in rows], key=order)


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def pareto_boundary(rows, metric):
    """Minimize storage and maximize performance; collapse duplicate coordinates."""
    boundary = []
    best = -math.inf
    for row in sorted(rows, key=lambda r: (r['total_storage_kib'], -r[metric])):
        if row[metric] > best:
            boundary.append(row)
            best = row[metric]
    return boundary


def plot(summary, output, title=None):
    os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "champsim-storage-dse-matplotlib"))
    # Match plot_ipc's compatibility support for the cluster's Matplotlib.
    try:
        import importlib_resources
    except ModuleNotFoundError:
        from importlib import resources
        sys.modules["importlib_resources"] = resources
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 2, figsize=(13, 5), layout="constrained")
    artists = {}
    for key, (label, color, marker) in DESIGNS.items():
        rows = [r for r in summary if r["design"] == label]
        if not rows:
            continue
        artists[key] = [ax.scatter([r["total_storage_kib"] for r in rows], [r[metric] for r in rows],
                                  label=label, c=color, marker=marker, s=48, alpha=0.8,
                                  edgecolors="black", linewidths=0.4)
                        for ax, (metric, _) in zip(axes, METRICS)]
    boundaries = []
    for ax, (metric, ylabel) in zip(axes, METRICS):
        rows = pareto_boundary(summary, metric)
        boundaries.extend(ax.plot([r['total_storage_kib'] for r in rows],
                                  [r[metric] for r in rows], 'k--', linewidth=1.5,
                                  label='Pareto boundary', zorder=4))
        ax.set_xlabel(STORAGE_LABEL)
        ax.set_ylabel(ylabel)
        ax.grid(alpha=0.25)
        ax.legend()
    heading = fig.suptitle(title or f"Instruction front-end design space · {summary[0]['trace_count']} trace(s)")
    try:
        # Solve layout once, then freeze positions and limits for every export.
        fig.canvas.draw()
        positions = [ax.get_position().frozen() for ax in axes]
        if hasattr(fig, "set_layout_engine"):
            fig.set_layout_engine("none")
        else:
            fig.set_constrained_layout(False)
        for ax, position in zip(axes, positions):
            ax.set_position(position)
            ax.set_autoscale_on(False)

        def save(name, transparent=False):
            destination = output.with_name(output.name + name)
            for suffix in (".png", ".pdf"):
                fig.savefig(destination.with_suffix(suffix), dpi=180, transparent=transparent)

        save("")
        for boundary in boundaries:
            boundary.set_visible(False)
        for key in artists:
            for design, collections in artists.items():
                for collection in collections:
                    collection.set_visible(design == key)
            # Keep the legend's position fixed, hiding other entries in place.
            for ax in axes:
                legend = ax.get_legend()
                handles = getattr(legend, "legend_handles", None)
                if handles is None:
                    handles = legend.legendHandles
                for handle, text in zip(handles, legend.get_texts()):
                    handle.set_visible(text.get_text() == DESIGNS[key][0])
                for text in legend.get_texts():
                    text.set_visible(text.get_text() == DESIGNS[key][0])
            save("_" + key, transparent=True)
            for ax in axes:
                ax.set_axis_off()
                ax.get_legend().set_visible(False)
            heading.set_visible(False)
            save("_" + key + "_layer", transparent=True)
            for ax in axes:
                ax.set_axis_on()
                ax.get_legend().set_visible(True)
            heading.set_visible(True)

        for collections in artists.values():
            for collection in collections:
                collection.set_visible(False)
        for ax in axes:
            legend = ax.get_legend()
            for child in legend.findobj():
                child.set_visible(True)
        save("_axes", transparent=True)
    finally:
        plt.close(fig)


def interactive_figure(summary, title=None):
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots

    fig = make_subplots(rows=1, cols=2, horizontal_spacing=0.12,
                        subplot_titles=[label for _, label in METRICS])
    symbols = dict(baseline="circle", ubs="square", litebtb="triangle-up", combined="diamond")
    for key, (label, color, _) in DESIGNS.items():
        rows = [row for row in summary if row['design'] == label]
        if not rows:
            continue
        customdata = [[r['configuration'], r['nominal_icache_kib'], r['btb_entries'],
                       r['l1i_bits'] / 8192, r['btb_bits'] / 8192, r['trace_count']] for r in rows]
        for column, (metric, ylabel) in enumerate(METRICS, 1):
            fig.add_trace(go.Scatter(
                x=[r['total_storage_kib'] for r in rows], y=[r[metric] for r in rows],
                mode="markers", name=label, legendgroup=key, showlegend=column == 1,
                meta=dict(design=key, metric=metric), customdata=customdata,
                marker=dict(color=color, symbol=symbols[key], size=10, opacity=0.8,
                            line=dict(color="black", width=0.5)),
                hovertemplate=("<b>%{customdata[0]}</b><br>"
                               "Nominal L1I: %{customdata[1]} KiB<br>BTB: %{customdata[2]} entries<br>"
                               "L1I storage: %{customdata[3]:.3f} KiB<br>BTB storage: %{customdata[4]:.3f} KiB<br>"
                               "Total storage: %{x:.3f} KiB<br>" + ylabel + ": %{y:.6f}<br>"
                               "Workloads: %{customdata[5]}<extra>%{fullData.name}</extra>")), row=1, col=column)
            fig.update_xaxes(title_text=STORAGE_LABEL, row=1, col=column)
            fig.update_yaxes(title_text=ylabel, row=1, col=column)
    for column, (metric, _) in enumerate(METRICS, 1):
        rows = pareto_boundary(summary, metric)
        fig.add_trace(go.Scatter(
            x=[r['total_storage_kib'] for r in rows], y=[r[metric] for r in rows],
            mode="lines", name="Pareto boundary", legendgroup="pareto", showlegend=column == 1,
            meta=dict(design="pareto", metric=metric),
            customdata=[[r['configuration']] for r in rows],
            line=dict(color="black", dash="dash", width=2),
            hovertemplate="%{customdata[0]}<br>Storage: %{x:.3f} KiB<br>Value: %{y:.6f}<extra>Pareto boundary</extra>"
        ), row=1, col=column)
    fig.update_layout(template="plotly_white", height=650, uirevision="storage-dse",
                      title=dict(text=html.escape(title or f"Instruction front-end design space · {summary[0]['trace_count']} trace(s)")),
                      legend=dict(orientation="h", y=-0.25, groupclick="togglegroup"),
                      margin=dict(l=80, r=40, t=100, b=140))
    return fig


def plot_interactive(summary, output, title=None):
    fig = interactive_figure(summary, title)
    script = Path(__file__).with_name("storage_dse_interactive.js").read_text()
    graph = fig.to_html(full_html=False, include_plotlyjs=True, div_id="storage-dse-plot", post_script=script,
                        config=dict(responsive=True, displaylogo=False,
                                    toImageButtonOptions=dict(format="png", filename=output.stem)))
    document = ('<!doctype html><html lang="en"><head><meta charset="utf-8">'
                '<meta name="viewport" content="width=device-width,initial-scale=1">'
                '<title>Storage DSE</title><style>'
                'body{font:16px system-ui,sans-serif;margin:24px;color:#182433}'
                'main{max-width:1500px;margin:auto}.controls{display:flex;flex-wrap:wrap;gap:24px;align-items:center}'
                'label{display:flex;align-items:center;gap:8px}select{font:inherit;padding:6px}'
                'p{color:#526174}#selection-status{font-variant-numeric:tabular-nums}'
                '</style></head><body><main>'
                '<p>Choose the same nominal L1I capacity and BTB entry count across all designs. '
                'Click a legend entry to hide a design; double-click to isolate it. '
                'The dashed Pareto boundary uses all designs matching the capacity filters, including hidden designs. Hover for storage and performance details. Drag to zoom; double-click the plot to reset.</p>'
                + graph + '</main></body></html>')
    fig_path = output.with_suffix(".html")
    fig_path.write_text(document, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--configs", type=Path, default=Path(__file__).resolve().parents[1] / "IDUN_CONFIGS/storage_dse")
    parser.add_argument("--output", type=Path, default=Path("storage_dse"))
    parser.add_argument("--title", help="optional figure title")
    parser.add_argument("--allow-incomplete", action="store_true",
                        help="skip missing configurations and those with fewer workloads than the maximum (remaining trace sets must match)")
    args = parser.parse_args()
    try:
        summary, traces = collect(args.results, args.configs, args.allow_incomplete)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write_csv(args.output.with_suffix(".csv"), summary)
        write_csv(args.output.with_name(args.output.name + "_per_trace.csv"), traces)
        plot(summary, args.output, args.title)
        plot_interactive(summary, args.output, args.title)
        from plot_mpki import experiment_mpki
        # Match both nominal I-cache capacity and BTB entry count.
        baselines = {row['configuration']: re.sub(r'^(ubs|litebtb|combined)_', 'baseline_', row['configuration'])
                     for row in summary}
        experiment_mpki(args.results, traces, baselines, args.output,
                        {row['configuration']: row['configuration'] for row in summary})
    except (ValueError, OSError, KeyError, ImportError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Wrote {len(summary)} storage-sorted points to {args.output}.[csv,png,pdf,html] "
          "plus transparent per-design figures, point layers, and axes")


if __name__ == "__main__":
    main()
