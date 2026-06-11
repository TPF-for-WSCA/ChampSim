import argparse
import csv
import html
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

BRANCH_PROGRESS_RE = re.compile(
    r"Branch progress CPU (?P<cpu>\d+) "
    r"sim cycles: (?P<sim_cycles>\d+) "
    r"cycles: (?P<cycles>\d+) "
    r"instructions: (?P<instructions>\d+) "
    r"aliasing: (?P<aliasing>\d+) "
    r"aliasing MPKI: (?P<aliasing_mpki>[-+0-9.eE]+) "
    r"total branch misses: (?P<branch_misses>\d+) "
    r"BRANCH_MPKI: (?P<branch_mpki>[-+0-9.eE]+)"
)

BRANCH_PROGRESS_COLUMNS = [
    "config",
    "application",
    "cpu",
    "sim_cycles",
    "cycles",
    "instructions",
    "aliasing",
    "aliasing_mpki",
    "branch_misses",
    "branch_mpki",
    "log_file",
]


def draw_invalid_entry(path, fix, ax, workload, color="r"):
    import numpy as np
    import pandas as pd

    dt = np.dtype([("Invalid Entries", "<u4")])
    data = np.fromfile(path, dtype=dt)
    df = pd.DataFrame(data, columns=data.dtype.names)
    (line,) = ax.plot(df, color)
    mean = df.loc[:, "Invalid Entries"].mean()
    line.set_label(f"{workload} [{mean:.2f}]")
    print(f"{path}:\t{mean}")
    ax.axhline(y=mean, color=color)


def legacy_invalid_entry_plot():
    from matplotlib import pyplot, colors

    color_names = list(colors.cnames.keys())
    fix, ax = pyplot.subplots()
    if len(sys.argv) > 2 and sys.argv[2] == "multi":
        nc = int(len(color_names) / 2)
        for workload in os.listdir(sys.argv[1]):
            if not os.path.isdir(os.path.join(sys.argv[1], workload)):
                continue
            if workload in ["graphs", "raw_data"]:
                continue
            draw_invalid_entry(
                f"{sys.argv[1]}/{workload}/cpu0_L1I_cl_num_invalid_blocks.bin",
                fix,
                ax,
                workload,
                color_names[nc],
            )
            nc = (nc + 1) % len(color_names)
    else:
        draw_invalid_entry(
            f"{sys.argv[1]}/cpu0_L1I_cl_num_invalid_blocks.bin",
            fix,
            ax,
            sys.argv[1],
        )

    max_entries = 18 * 64
    ax.axhline(y=max_entries)
    ax.legend()
    pyplot.show()


def iter_log_files(root):
    skip_dirs = {"graphs", "raw_data", ".git"}
    for log_path in root.rglob("*_log.txt"):
        if any(part in skip_dirs for part in log_path.relative_to(root).parts):
            continue
        yield log_path


def label_for_log(root, log_path):
    parent = log_path.parent
    rel_parent = parent.relative_to(root)
    parts = rel_parent.parts

    if len(parts) >= 2:
        config = parts[0]
        application = parts[1]
    elif len(parts) == 1:
        config = "const"
        application = parts[0]
    else:
        config = "const"
        application = log_path.stem.removesuffix("_log")

    return config, application


def extract_latest_branch_progress(root, log_path):
    config, application = label_for_log(root, log_path)
    rows = []
    with log_path.open() as log:
        for line in log:
            if "NEW RUN -" in line:
                rows = []
                continue

            match = BRANCH_PROGRESS_RE.search(line)
            if not match:
                continue

            values = match.groupdict()
            row = {
                "config": config,
                "application": application,
                "cpu": int(values["cpu"]),
                "sim_cycles": int(values["sim_cycles"]),
                "cycles": int(values["cycles"]),
                "instructions": int(values["instructions"]),
                "aliasing": int(values["aliasing"]),
                "aliasing_mpki": float(values["aliasing_mpki"]),
                "branch_misses": int(values["branch_misses"]),
                "branch_mpki": float(values["branch_mpki"]),
                "log_file": str(log_path),
            }
            rows.append(row)
    return rows


def collect_branch_progress(root):
    rows = []
    for log_path in iter_log_files(root):
        rows.extend(extract_latest_branch_progress(root, log_path))
    return rows


def write_branch_progress_tsv(rows, out_tsv):
    with out_tsv.open("w", newline="") as outfile:
        writer = csv.DictWriter(outfile, fieldnames=BRANCH_PROGRESS_COLUMNS, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def write_html_wrapper(html_path, image_paths, title):
    image_tags = "\n".join(
        f'<h2>{image.stem.replace("_", " ").title()}</h2><img src="{image.name}" alt="{image.stem}">'
        for image in image_paths
    )
    html_path.write_text(
        f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>{title}</title>
  <style>
    body {{ font-family: sans-serif; margin: 24px; }}
    img {{ max-width: 100%; height: auto; display: block; margin-bottom: 32px; }}
  </style>
</head>
<body>
  <h1>{title}</h1>
  {image_tags}
</body>
</html>
"""
    )


def plot_branch_progress_metric(rows, metric, ylabel, output_path):
    from matplotlib import pyplot

    fig, ax = pyplot.subplots(figsize=(15, 7))

    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["config"], row["application"], row["cpu"])].append(row)

    cpu_count = len({row["cpu"] for row in rows})
    for (config, application, cpu), group in sorted(grouped.items()):
        group.sort(key=lambda row: row["sim_cycles"])
        label = f"{config}/{application}"
        if cpu_count > 1:
            label += f"/CPU{cpu}"
        ax.plot(
            [row["sim_cycles"] for row in group],
            [row[metric] for row in group],
            marker=".",
            linewidth=1.4,
            label=label,
        )

    ax.set_xlabel("Simulation cycles after warmup")
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.25)
    ax.legend(loc="center left", bbox_to_anchor=(1.0, 0.5), fontsize="small")
    fig.tight_layout()
    fig.savefig(output_path, dpi=160)
    pyplot.close(fig)


def plot_branch_progress_metric_svg(rows, metric, ylabel, output_path):
    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["config"], row["application"], row["cpu"])].append(row)

    width = 1400
    height = 720
    left = 90
    right = 360
    top = 40
    bottom = 70
    plot_width = width - left - right
    plot_height = height - top - bottom
    palette = [
        "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b",
        "#e377c2", "#7f7f7f", "#bcbd22", "#17becf", "#393b79", "#637939",
        "#8c6d31", "#843c39", "#7b4173", "#3182bd", "#e6550d", "#31a354",
    ]

    x_values = [row["sim_cycles"] for row in rows]
    y_values = [row[metric] for row in rows]
    x_min, x_max = min(x_values), max(x_values)
    y_min, y_max = min(y_values), max(y_values)
    if x_min == x_max:
        x_max = x_min + 1
    if y_min == y_max:
        y_max = y_min + 1

    def x_scale(value):
        return left + ((value - x_min) / (x_max - x_min)) * plot_width

    def y_scale(value):
        return top + plot_height - ((value - y_min) / (y_max - y_min)) * plot_height

    cpu_count = len({row["cpu"] for row in rows})
    lines = []
    legend = []
    for idx, ((config, application, cpu), group) in enumerate(sorted(grouped.items())):
        group.sort(key=lambda row: row["sim_cycles"])
        label = f"{config}/{application}"
        if cpu_count > 1:
            label += f"/CPU{cpu}"
        color = palette[idx % len(palette)]
        points = " ".join(f"{x_scale(row['sim_cycles']):.2f},{y_scale(row[metric]):.2f}" for row in group)
        lines.append(f'<polyline fill="none" stroke="{color}" stroke-width="2" points="{points}" />')
        for row in group:
            lines.append(f'<circle cx="{x_scale(row["sim_cycles"]):.2f}" cy="{y_scale(row[metric]):.2f}" r="2.4" fill="{color}" />')
        legend_y = top + 18 * idx
        legend.append(f'<line x1="{width - right + 25}" y1="{legend_y}" x2="{width - right + 55}" y2="{legend_y}" stroke="{color}" stroke-width="3" />')
        legend.append(f'<text x="{width - right + 65}" y="{legend_y + 4}" font-size="12">{html.escape(label)}</text>')

    x_ticks = []
    y_ticks = []
    for i in range(6):
        x_value = x_min + (x_max - x_min) * i / 5
        x = x_scale(x_value)
        x_ticks.append(f'<line x1="{x:.2f}" y1="{top}" x2="{x:.2f}" y2="{top + plot_height}" stroke="#ddd" />')
        x_ticks.append(f'<text x="{x:.2f}" y="{height - 35}" text-anchor="middle" font-size="12">{x_value:.0f}</text>')

        y_value = y_min + (y_max - y_min) * i / 5
        y = y_scale(y_value)
        y_ticks.append(f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_width}" y2="{y:.2f}" stroke="#ddd" />')
        y_ticks.append(f'<text x="{left - 12}" y="{y + 4:.2f}" text-anchor="end" font-size="12">{y_value:.3g}</text>')

    output_path.write_text(
        f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <rect width="100%" height="100%" fill="white" />
  <text x="{width / 2}" y="24" text-anchor="middle" font-size="20">{html.escape(ylabel)}</text>
  {"".join(x_ticks)}
  {"".join(y_ticks)}
  <rect x="{left}" y="{top}" width="{plot_width}" height="{plot_height}" fill="none" stroke="#333" />
  {"".join(lines)}
  <text x="{left + plot_width / 2}" y="{height - 10}" text-anchor="middle" font-size="14">Simulation cycles after warmup</text>
  <text transform="translate(18 {top + plot_height / 2}) rotate(-90)" text-anchor="middle" font-size="14">{html.escape(ylabel)}</text>
  {"".join(legend)}
</svg>
"""
    )


def plot_branch_progress_combined(rows, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        aliasing_path = output_dir / "branch_progress_aliasing_mpki_timeseries.png"
        branch_path = output_dir / "branch_progress_branch_mpki_timeseries.png"

        plot_branch_progress_metric(
            rows,
            "aliasing_mpki",
            "Aliasing per kilo instruction",
            aliasing_path,
        )
        plot_branch_progress_metric(
            rows,
            "branch_mpki",
            "Total branch misses per kilo instruction",
            branch_path,
        )
    except ModuleNotFoundError:
        aliasing_path = output_dir / "branch_progress_aliasing_mpki_timeseries.svg"
        branch_path = output_dir / "branch_progress_branch_mpki_timeseries.svg"

        plot_branch_progress_metric_svg(
            rows,
            "aliasing_mpki",
            "Aliasing per kilo instruction",
            aliasing_path,
        )
        plot_branch_progress_metric_svg(
            rows,
            "branch_mpki",
            "Total branch misses per kilo instruction",
            branch_path,
        )

    write_html_wrapper(
        output_dir / "branch_progress_timeseries.html",
        [aliasing_path, branch_path],
        "Branch Progress Time Series",
    )


def branch_progress_mode():
    parser = argparse.ArgumentParser(
        description="Collect and plot ChampSim branch-progress time series from *_log.txt files."
    )
    parser.add_argument("result_dir", type=Path)
    parser.add_argument(
        "--raw-data-dir",
        type=Path,
        default=None,
        help="Directory for the combined TSV. Defaults to RESULT_DIR/raw_data.",
    )
    parser.add_argument(
        "--graphs-dir",
        type=Path,
        default=None,
        help="Directory for generated plots. Defaults to RESULT_DIR/graphs.",
    )
    args = parser.parse_args(sys.argv[2:])

    root = args.result_dir
    raw_data_dir = args.raw_data_dir or root / "raw_data"
    graphs_dir = args.graphs_dir or root / "graphs"
    raw_data_dir.mkdir(parents=True, exist_ok=True)

    rows = collect_branch_progress(root)
    rows.sort(key=lambda row: (row["config"], row["application"], row["cpu"], row["sim_cycles"]))
    out_tsv = raw_data_dir / "branch_progress_timeseries.tsv"
    if not rows:
        print(f"No branch progress samples found under {root}")
        write_branch_progress_tsv(rows, out_tsv)
        return

    write_branch_progress_tsv(rows, out_tsv)
    plot_branch_progress_combined(rows, graphs_dir)
    print(f"Wrote {out_tsv}")
    print(f"Wrote {graphs_dir / 'branch_progress_timeseries.html'}")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--branch-progress":
        branch_progress_mode()
    else:
        legacy_invalid_entry_plot()
