import argparse
import csv
import html
import json
import os
import re
import sys
import colorsys
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
    "workload_group",
    "config",
    "benchmark_subgroup",
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

BRANCH_PROGRESS_SAMPLE_RATE = 32 * 1024


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

    if len(parts) >= 3:
        workload_group = "/".join(parts[:-3]) or "const"
        config = parts[-3]
        benchmark_subgroup = parts[-2]
        application = parts[-1]
    elif len(parts) == 2:
        workload_group = "const"
        config = parts[0]
        benchmark_subgroup = "const"
        application = parts[1]
    elif len(parts) == 1:
        workload_group = "const"
        config = "const"
        benchmark_subgroup = "const"
        application = parts[0]
    else:
        workload_group = "const"
        config = "const"
        benchmark_subgroup = "const"
        application = log_path.stem.removesuffix("_log")

    return workload_group, config, benchmark_subgroup, application


def extract_latest_branch_progress(root, log_path):
    workload_group, config, benchmark_subgroup, application = label_for_log(root, log_path)
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
                "workload_group": workload_group,
                "config": config,
                "benchmark_subgroup": benchmark_subgroup,
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


def hex_to_rgb(color):
    color = color.lstrip("#")
    return tuple(int(color[i : i + 2], 16) / 255 for i in (0, 2, 4))


def rgb_to_hex(rgb):
    return "#" + "".join(f"{max(0, min(255, round(channel * 255))):02x}" for channel in rgb)


def shade_color(base_color, shade_idx, shade_count):
    if shade_count <= 1:
        return base_color

    hue, lightness, saturation = colorsys.rgb_to_hls(*hex_to_rgb(base_color))
    lightness_min = 0.34
    lightness_max = 0.76
    lightness = lightness_min + (lightness_max - lightness_min) * shade_idx / (shade_count - 1)
    saturation = min(0.92, max(0.42, saturation))
    return rgb_to_hex(colorsys.hls_to_rgb(hue, lightness, saturation))


def distinct_workload_color(idx, color_count):
    if color_count <= 1:
        hue = 0.58
    else:
        hue = idx / color_count

    return rgb_to_hex(colorsys.hls_to_rgb(hue, 0.52, 0.78))


def branch_progress_styles(rows):
    applications = sorted({row["application"] for row in rows})
    configs = sorted({row["config"] for row in rows})
    base_by_config = {
        config: distinct_workload_color(idx, len(configs))
        for idx, config in enumerate(configs)
    }
    shade_by_application = {
        application: idx
        for idx, application in enumerate(applications)
    }
    return base_by_config, shade_by_application, len(applications)


def branch_progress_x_value(row):
    return row["sim_cycles"] / BRANCH_PROGRESS_SAMPLE_RATE


def branch_progress_workload_label(application):
    if "_to_" not in application:
        return application

    warmup_workload, context_switch_workload = application.split("_to_", 1)
    return f"{warmup_workload} -> {context_switch_workload}"


def branch_progress_label(workload_group, config, benchmark_subgroup, application, cpu, cpu_count):
    label = f"{config}: {branch_progress_workload_label(application)}"
    if workload_group != "const":
        label = f"{workload_group}/{label}"
    if benchmark_subgroup != "const":
        label = f"{label} [{benchmark_subgroup}]"
    if cpu_count > 1:
        label += f"/CPU{cpu}"
    return label


def grouped_branch_progress(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["workload_group"], row["config"], row["benchmark_subgroup"], row["application"], row["cpu"])].append(row)
    return grouped


def plot_branch_progress_metric(rows, metric, ylabel, output_path):
    from matplotlib import pyplot

    fig, ax = pyplot.subplots(figsize=(15, 7))

    grouped = grouped_branch_progress(rows)

    cpu_count = len({row["cpu"] for row in rows})
    base_by_config, shade_by_application, application_count = branch_progress_styles(rows)
    for (workload_group, config, benchmark_subgroup, application, cpu), group in sorted(grouped.items()):
        group.sort(key=lambda row: row["sim_cycles"])
        label = branch_progress_label(workload_group, config, benchmark_subgroup, application, cpu, cpu_count)
        color = shade_color(
            base_by_config[config],
            shade_by_application[application],
            application_count,
        )
        ax.plot(
            [branch_progress_x_value(row) for row in group],
            [row[metric] for row in group],
            linewidth=1.4,
            label=label,
            color=color,
        )

    ax.set_xlabel("Sample after warmup")
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.25)
    ax.legend(loc="center left", bbox_to_anchor=(1.0, 0.5), fontsize="small")
    fig.tight_layout()
    fig.savefig(output_path, dpi=160)
    pyplot.close(fig)


def plot_branch_progress_metric_svg(rows, metric, ylabel, output_path):
    grouped = grouped_branch_progress(rows)

    width = 1400
    height = 720
    left = 90
    right = 360
    top = 40
    bottom = 70
    plot_width = width - left - right
    plot_height = height - top - bottom

    x_values = [branch_progress_x_value(row) for row in rows]
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
    base_by_config, shade_by_application, application_count = branch_progress_styles(rows)
    lines = []
    legend = []
    for idx, ((workload_group, config, benchmark_subgroup, application, cpu), group) in enumerate(sorted(grouped.items())):
        group.sort(key=lambda row: row["sim_cycles"])
        label = branch_progress_label(workload_group, config, benchmark_subgroup, application, cpu, cpu_count)
        color = shade_color(
            base_by_config[config],
            shade_by_application[application],
            application_count,
        )
        points = " ".join(f"{x_scale(branch_progress_x_value(row)):.2f},{y_scale(row[metric]):.2f}" for row in group)
        lines.append(f'<polyline fill="none" stroke="{color}" stroke-width="2" points="{points}" />')
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
  <text x="{left + plot_width / 2}" y="{height - 10}" text-anchor="middle" font-size="14">Sample after warmup</text>
  <text transform="translate(18 {top + plot_height / 2}) rotate(-90)" text-anchor="middle" font-size="14">{html.escape(ylabel)}</text>
  {"".join(legend)}
</svg>
"""
    )


def plot_branch_progress_interactive(rows, output_path):
    metrics = [
        ("aliasing_mpki", "Aliasing per kilo instruction"),
        ("branch_mpki", "Total branch misses per kilo instruction"),
    ]

    grouped = grouped_branch_progress(rows)
    cpu_count = len({row["cpu"] for row in rows})
    base_by_config, shade_by_application, application_count = branch_progress_styles(rows)
    traces = []

    for metric_idx, (metric, ylabel) in enumerate(metrics):
        for workload_group, config, benchmark_subgroup, application, cpu in sorted(grouped):
            group = sorted(grouped[(workload_group, config, benchmark_subgroup, application, cpu)], key=lambda row: row["sim_cycles"])
            label = branch_progress_label(workload_group, config, benchmark_subgroup, application, cpu, cpu_count)
            color = shade_color(
                base_by_config[config],
                shade_by_application[application],
                application_count,
            )
            customdata = [
                [
                    row["instructions"],
                    row["cycles"],
                    row["aliasing"],
                    row["branch_misses"],
                    row["log_file"],
                ]
                for row in group
            ]
            axis_suffix = "" if metric_idx == 0 else str(metric_idx + 1)
            traces.append(
                {
                    "type": "scatter",
                    "x": [branch_progress_x_value(row) for row in group],
                    "y": [row[metric] for row in group],
                    "customdata": customdata,
                    "mode": "lines",
                    "name": label,
                    "legendgroup": label,
                    "showlegend": metric_idx == 0,
                    "xaxis": f"x{axis_suffix}",
                    "yaxis": f"y{axis_suffix}",
                    "line": {"color": color, "width": 2},
                    "hovertemplate": (
                        "<b>%{fullData.name}</b><br>"
                        "sample=%{x}<br>"
                        f"{ylabel}=%{{y:.4g}}<br>"
                        "instructions=%{customdata[0]}<br>"
                        "cycles=%{customdata[1]}<br>"
                        "aliasing=%{customdata[2]}<br>"
                        "branch misses=%{customdata[3]}<br>"
                        "log=%{customdata[4]}"
                        "<extra></extra>"
                    ),
                }
            )

    layout = {
        "title": "Branch Progress Time Series",
        "template": "plotly_white",
        "hovermode": "closest",
        "height": 900,
        "margin": {"l": 80, "r": 280, "t": 80, "b": 70},
        "legend": {
            "title": {"text": "Workload group/config: warmup -> context switch [benchmark subgroup]"},
            "x": 1.02,
            "y": 1,
            "xanchor": "left",
            "yanchor": "top",
            "groupclick": "togglegroup",
        },
        "xaxis": {
            "domain": [0, 1],
            "anchor": "y",
            "showticklabels": False,
            "matches": "x2",
        },
        "yaxis": {
            "domain": [0.56, 1],
            "anchor": "x",
            "title": {"text": metrics[0][1]},
        },
        "xaxis2": {
            "domain": [0, 1],
            "anchor": "y2",
            "title": {"text": "Sample after warmup"},
        },
        "yaxis2": {
            "domain": [0, 0.44],
            "anchor": "x2",
            "title": {"text": metrics[1][1]},
        },
        "annotations": [
            {
                "text": metrics[0][1],
                "xref": "paper",
                "yref": "paper",
                "x": 0.5,
                "y": 1.04,
                "showarrow": False,
                "font": {"size": 16},
            },
            {
                "text": metrics[1][1],
                "xref": "paper",
                "yref": "paper",
                "x": 0.5,
                "y": 0.48,
                "showarrow": False,
                "font": {"size": 16},
            },
        ],
    }
    config = {
        "responsive": True,
        "displaylogo": False,
        "toImageButtonOptions": {
            "format": "png",
            "filename": output_path.stem,
            "height": 900,
            "width": 1400,
            "scale": 2,
        },
    }

    output_path.write_text(
        f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>Branch Progress Time Series</title>
  <script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
  <style>
    body {{ font-family: sans-serif; margin: 24px; }}
    #branch-progress {{ width: 100%; min-height: 900px; }}
  </style>
</head>
<body>
  <div id="branch-progress"></div>
  <script>
    const data = {json.dumps(traces)};
    const layout = {json.dumps(layout)};
    const config = {json.dumps(config)};
    Plotly.newPlot("branch-progress", data, layout, config);
  </script>
</body>
</html>
"""
    )


def plot_branch_progress_combined(rows, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        plot_branch_progress_interactive(
            rows,
            output_dir / "branch_progress_timeseries.html",
        )
        return
    except ModuleNotFoundError:
        pass

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
    rows.sort(key=lambda row: (row["workload_group"], row["config"], row["benchmark_subgroup"], row["application"], row["cpu"], row["sim_cycles"]))
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
