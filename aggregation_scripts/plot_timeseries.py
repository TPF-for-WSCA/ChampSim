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
    r"(?: region BTB induced misses: (?P<region_btb_misses>\d+) "
    r"REGION_BTB_MPKI: (?P<region_btb_mpki>[-+0-9.eE]+))?"
    r"(?: total BTB target mispredicts: (?P<btb_target_mispredicts>\d+) "
    r"BTB_TARGET_MPKI: (?P<btb_target_mpki>[-+0-9.eE]+))?"
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
    "btb_misses",
    "btb_mpki",
    "region_btb_misses",
    "region_btb_mpki",
    "btb_target_mispredicts",
    "btb_target_mpki",
    "log_file",
]

BRANCH_PROGRESS_SAMPLE_RATE = 32 * 1024
BRANCH_PROGRESS_PAPER_SAMPLE_LIMIT = 100
BRANCH_PROGRESS_IEEE_TEXT_WIDTH_IN = 7.16
BRANCH_PROGRESS_PAPER_FONT_SIZE = 12
BRANCH_PROGRESS_PAPER_SMALL_FIGSIZE = (BRANCH_PROGRESS_IEEE_TEXT_WIDTH_IN / 3, 2.1)
BRANCH_PROGRESS_PAPER_CONFIGS = [
    ("Baseline", "baseline"),
    ("Full Tag", "full_tag"),
    ("LiteBTB", "litebtb"),
]
BRANCH_PROGRESS_PAPER_METRICS = [
    ("aliasing_mpki", "Aliasing per kilo instruction"),
    ("branch_mpki", "Total branch misses per kilo instruction"),
    ("btb_mpki", "BTB misses per kilo instruction"),
    ("region_btb_mpki", "Region BTB misses per kilo instruction"),
    ("btb_target_mpki", "BTB target mispredicts per kilo instruction"),
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
            branch_misses = int(values["branch_misses"])
            branch_mpki = float(values["branch_mpki"])
            region_btb_misses = int(values["region_btb_misses"] or 0)
            region_btb_mpki = float(values["region_btb_mpki"] or 0.0)
            btb_misses = max(0, branch_misses - region_btb_misses)
            btb_mpki = max(0.0, branch_mpki - region_btb_mpki)
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
                "branch_misses": branch_misses,
                "branch_mpki": branch_mpki,
                "btb_misses": btb_misses,
                "btb_mpki": btb_mpki,
                "region_btb_misses": region_btb_misses,
                "region_btb_mpki": region_btb_mpki,
                "btb_target_mispredicts": int(values["btb_target_mispredicts"] or 0),
                "btb_target_mpki": float(values["btb_target_mpki"] or 0.0),
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
    hue_span = min(0.18, 0.035 * (shade_count - 1))
    hue_offset = (shade_idx / (shade_count - 1) - 0.5) * hue_span
    hue = (hue + hue_offset) % 1.0
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


def branch_progress_application_colors(rows):
    applications = sorted({row["application"] for row in rows})
    return {
        application: distinct_workload_color(idx, len(applications))
        for idx, application in enumerate(applications)
    }


def branch_progress_config_label(config):
    if "_abtb_" in config or config.endswith("_abtb"):
        return "Baseline"
    if "_rc_" in config or config.endswith("_rc"):
        return "LiteBTB"
    if config.endswith("_full") or "_full_" in config:
        return "Full Tag"
    return config


def branch_progress_x_value(row):
    return row["sim_cycles"] / BRANCH_PROGRESS_SAMPLE_RATE


def branch_progress_rows_in_sample_window(rows, x_limits):
    x_min, x_max = x_limits
    return [
        row for row in rows
        if x_min <= branch_progress_x_value(row) <= x_max
    ]


def branch_progress_metric_y_limits(rows, metric):
    y_values = [row[metric] for row in rows]
    y_min, y_max = min(y_values), max(y_values)
    if y_min == y_max:
        pad = abs(y_min) * 0.05 or 1.0
    else:
        pad = (y_max - y_min) * 0.05
    return y_min - pad, y_max + pad


def branch_progress_workload_label(application):
    if "_to_" not in application:
        return application

    warmup_workload, context_switch_workload = application.split("_to_", 1)
    return f"{warmup_workload} -> {context_switch_workload}"


def branch_progress_application_label(application):
    return branch_progress_workload_label(application)


def branch_progress_legend_label(config, application):
    return f"{branch_progress_config_label(config)} - {branch_progress_application_label(application)}"


def branch_progress_label(workload_group, config, benchmark_subgroup, application, cpu, cpu_count):
    label = branch_progress_legend_label(config, application)
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
        grouped[(row["config"], row["application"], row["workload_group"], row["benchmark_subgroup"], row["cpu"])].append(row)
    return grouped


def branch_progress_row_sort_key(row):
    return (
        row["config"],
        row["application"],
        row["workload_group"],
        row["benchmark_subgroup"],
        row["cpu"],
        row["sim_cycles"],
    )


def plot_branch_progress_metric(rows, metric, ylabel, output_path):
    from matplotlib import pyplot

    fig, ax = pyplot.subplots(figsize=(15, 7))

    grouped = grouped_branch_progress(rows)

    cpu_count = len({row["cpu"] for row in rows})
    base_by_config, shade_by_application, application_count = branch_progress_styles(rows)
    shown_labels = set()
    for (config, application, workload_group, benchmark_subgroup, cpu), group in sorted(grouped.items()):
        group.sort(key=lambda row: row["sim_cycles"])
        label = branch_progress_legend_label(config, application)
        show_label = label if label not in shown_labels else "_nolegend_"
        shown_labels.add(label)
        color = shade_color(
            base_by_config[config],
            shade_by_application[application],
            application_count,
        )
        ax.plot(
            [branch_progress_x_value(row) for row in group],
            [row[metric] for row in group],
            linewidth=1.4,
            label=show_label,
            color=color,
        )

    ax.set_xlabel("Sample after warmup")
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.25)
    ax.legend(loc="center left", bbox_to_anchor=(1.0, 0.5), fontsize="small")
    fig.tight_layout()
    fig.savefig(output_path, dpi=160)
    pyplot.close(fig)


def plot_branch_progress_paper_metric(rows, metric, ylabel, output_path):
    from matplotlib import pyplot
    from matplotlib.lines import Line2D

    fig, ax = pyplot.subplots(figsize=(6.6, 3.2))
    grouped = grouped_branch_progress(rows)

    base_by_config, shade_by_application, application_count = branch_progress_styles(rows)
    seen_labels = set()
    for (config, application, workload_group, benchmark_subgroup, cpu), group in sorted(grouped.items()):
        group.sort(key=lambda row: row["sim_cycles"])
        label = branch_progress_legend_label(config, application)
        color = shade_color(
            base_by_config[config],
            shade_by_application[application],
            application_count,
        )
        ax.plot(
            [branch_progress_x_value(row) for row in group],
            [row[metric] for row in group],
            linewidth=1.2,
            color=color,
        )
        seen_labels.add((label, color))

    legend_handles = [
        Line2D([0], [0], color=color, linewidth=2.0, label=label)
        for label, color in sorted(seen_labels)
    ]
    ax.legend(handles=legend_handles, loc="best", frameon=False, fontsize=8)
    ax.set_xlabel("Sample after warmup")
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.2, linewidth=0.6)
    fig.tight_layout()
    fig.savefig(output_path)
    pyplot.close(fig)


def plot_branch_progress_small_paper_metric(
    rows,
    style_rows,
    metric,
    ylabel,
    output_path,
    title,
    x_limits,
    y_limits,
):
    from matplotlib import pyplot

    fig, ax = pyplot.subplots(figsize=BRANCH_PROGRESS_PAPER_SMALL_FIGSIZE)
    grouped = grouped_branch_progress(rows)
    application_colors = branch_progress_application_colors(style_rows)

    for (config, application, workload_group, benchmark_subgroup, cpu), group in sorted(grouped.items()):
        group = sorted(group, key=lambda row: row["sim_cycles"])
        ax.plot(
            [branch_progress_x_value(row) for row in group],
            [row[metric] for row in group],
            linewidth=0.9,
            color=application_colors[application],
        )

    ax.set_title(title, fontsize=BRANCH_PROGRESS_PAPER_FONT_SIZE, pad=2)
    ax.set_xlabel("Sample after warmup", fontsize=BRANCH_PROGRESS_PAPER_FONT_SIZE)
    ax.set_ylabel(ylabel, fontsize=BRANCH_PROGRESS_PAPER_FONT_SIZE)
    ax.set_xlim(*x_limits)
    ax.set_ylim(*y_limits)
    ax.tick_params(axis="both", labelsize=BRANCH_PROGRESS_PAPER_FONT_SIZE, pad=1)
    ax.grid(True, alpha=0.2, linewidth=0.5)
    fig.tight_layout(pad=0.25)
    fig.savefig(output_path)
    pyplot.close(fig)


def plot_branch_progress_paper_metric_svg(rows, metric, ylabel, output_path):
    grouped = grouped_branch_progress(rows)

    width = 720
    height = 360
    left = 72
    right = 130
    top = 24
    bottom = 52
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

    y_padding = (y_max - y_min) * 0.04
    y_min -= y_padding
    y_max += y_padding

    def x_scale(value):
        return left + ((value - x_min) / (x_max - x_min)) * plot_width

    def y_scale(value):
        return top + plot_height - ((value - y_min) / (y_max - y_min)) * plot_height

    base_by_config, shade_by_application, application_count = branch_progress_styles(rows)
    lines = []
    for (config, application, workload_group, benchmark_subgroup, cpu), group in sorted(grouped.items()):
        group.sort(key=lambda row: row["sim_cycles"])
        color = shade_color(
            base_by_config[config],
            shade_by_application[application],
            application_count,
        )
        points = " ".join(f"{x_scale(branch_progress_x_value(row)):.2f},{y_scale(row[metric]):.2f}" for row in group)
        lines.append(f'<polyline fill="none" stroke="{color}" stroke-width="1.6" points="{points}" />')

    x_ticks = []
    y_ticks = []
    for i in range(5):
        x_value = x_min + (x_max - x_min) * i / 4
        x = x_scale(x_value)
        x_ticks.append(f'<line x1="{x:.2f}" y1="{top}" x2="{x:.2f}" y2="{top + plot_height}" stroke="#e6e6e6" />')
        x_ticks.append(f'<text x="{x:.2f}" y="{height - 18}" text-anchor="middle" font-size="11">{x_value:.0f}</text>')

        y_value = y_min + (y_max - y_min) * i / 4
        y = y_scale(y_value)
        y_ticks.append(f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_width}" y2="{y:.2f}" stroke="#e6e6e6" />')
        y_ticks.append(f'<text x="{left - 8}" y="{y + 4:.2f}" text-anchor="end" font-size="11">{y_value:.3g}</text>')

    legend = []
    legend_x = left + plot_width + 18
    legend_y = top + 14
    legend_items = {}
    for config, application, workload_group, benchmark_subgroup, cpu in sorted(grouped):
        label = branch_progress_legend_label(config, application)
        legend_items.setdefault(
            label,
            shade_color(base_by_config[config], shade_by_application[application], application_count),
        )
    for idx, (label, color) in enumerate(sorted(legend_items.items())):
        y = legend_y + idx * 18
        legend.append(f'<line x1="{legend_x}" y1="{y}" x2="{legend_x + 24}" y2="{y}" stroke="{color}" stroke-width="2.4" />')
        legend.append(f'<text x="{legend_x + 32}" y="{y + 4}" font-size="11">{html.escape(label)}</text>')

    output_path.write_text(
        f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <rect width="100%" height="100%" fill="white" />
  {"".join(x_ticks)}
  {"".join(y_ticks)}
  <rect x="{left}" y="{top}" width="{plot_width}" height="{plot_height}" fill="none" stroke="#333" />
  {"".join(lines)}
  <text x="{left + plot_width / 2}" y="{height - 4}" text-anchor="middle" font-size="12">Sample after warmup</text>
  <text transform="translate(16 {top + plot_height / 2}) rotate(-90)" text-anchor="middle" font-size="12">{html.escape(ylabel)}</text>
  {"".join(legend)}
</svg>
"""
    )


def plot_branch_progress_small_paper_metric_svg(
    rows,
    style_rows,
    metric,
    ylabel,
    output_path,
    title,
    x_limits,
    y_limits,
):
    grouped = grouped_branch_progress(rows)

    width = round(BRANCH_PROGRESS_IEEE_TEXT_WIDTH_IN / 3 * 100)
    height = 210
    left = 62
    right = 10
    top = 26
    bottom = 52
    plot_width = width - left - right
    plot_height = height - top - bottom
    x_min, x_max = x_limits
    y_min, y_max = y_limits

    def x_scale(value):
        return left + ((value - x_min) / (x_max - x_min)) * plot_width

    def y_scale(value):
        return top + plot_height - ((value - y_min) / (y_max - y_min)) * plot_height

    application_colors = branch_progress_application_colors(style_rows)
    lines = []
    for (config, application, workload_group, benchmark_subgroup, cpu), group in sorted(grouped.items()):
        group = sorted(group, key=lambda row: row["sim_cycles"])
        points = " ".join(
            f"{x_scale(branch_progress_x_value(row)):.2f},{y_scale(row[metric]):.2f}"
            for row in group
        )
        lines.append(
            f'<polyline fill="none" stroke="{application_colors[application]}" stroke-width="1" points="{points}" />'
        )

    x_ticks = []
    y_ticks = []
    for i in range(5):
        x_value = x_min + (x_max - x_min) * i / 4
        x = x_scale(x_value)
        x_ticks.append(f'<line x1="{x:.2f}" y1="{top}" x2="{x:.2f}" y2="{top + plot_height}" stroke="#e6e6e6" />')
        x_ticks.append(f'<text x="{x:.2f}" y="{height - 22}" text-anchor="middle" font-size="{BRANCH_PROGRESS_PAPER_FONT_SIZE}">{x_value:.0f}</text>')

        y_value = y_min + (y_max - y_min) * i / 4
        y = y_scale(y_value)
        y_ticks.append(f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_width}" y2="{y:.2f}" stroke="#e6e6e6" />')
        y_ticks.append(f'<text x="{left - 5}" y="{y + 2.5:.2f}" text-anchor="end" font-size="{BRANCH_PROGRESS_PAPER_FONT_SIZE}">{y_value:.3g}</text>')

    output_path.write_text(
        f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
  <rect width="100%" height="100%" fill="white" />
  <text x="{left + plot_width / 2}" y="12" text-anchor="middle" font-size="{BRANCH_PROGRESS_PAPER_FONT_SIZE}">{html.escape(title)}</text>
  {"".join(x_ticks)}
  {"".join(y_ticks)}
  <rect x="{left}" y="{top}" width="{plot_width}" height="{plot_height}" fill="none" stroke="#333" stroke-width="0.8" />
  {"".join(lines)}
  <text x="{left + plot_width / 2}" y="{height - 6}" text-anchor="middle" font-size="{BRANCH_PROGRESS_PAPER_FONT_SIZE}">Sample after warmup</text>
  <text transform="translate(12 {top + plot_height / 2}) rotate(-90)" text-anchor="middle" font-size="{BRANCH_PROGRESS_PAPER_FONT_SIZE}">{html.escape(ylabel)}</text>
</svg>
"""
    )


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
    shown_labels = set()
    for idx, ((config, application, workload_group, benchmark_subgroup, cpu), group) in enumerate(sorted(grouped.items())):
        group.sort(key=lambda row: row["sim_cycles"])
        label = branch_progress_legend_label(config, application)
        color = shade_color(
            base_by_config[config],
            shade_by_application[application],
            application_count,
        )
        points = " ".join(f"{x_scale(branch_progress_x_value(row)):.2f},{y_scale(row[metric]):.2f}" for row in group)
        lines.append(f'<polyline fill="none" stroke="{color}" stroke-width="2" points="{points}" />')
        if label in shown_labels:
            continue
        shown_labels.add(label)
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
        ("btb_mpki", "BTB misses per kilo instruction"),
        ("region_btb_mpki", "Region BTB misses per kilo instruction"),
        ("btb_target_mpki", "BTB target mispredicts per kilo instruction"),
    ]

    grouped = grouped_branch_progress(rows)
    cpu_count = len({row["cpu"] for row in rows})
    base_by_config, shade_by_application, application_count = branch_progress_styles(rows)
    traces = []
    shown_legend_entries = set()

    for metric_idx, (metric, ylabel) in enumerate(metrics):
        for config, application, workload_group, benchmark_subgroup, cpu in sorted(grouped):
            group = sorted(grouped[(config, application, workload_group, benchmark_subgroup, cpu)], key=lambda row: row["sim_cycles"])
            label = branch_progress_legend_label(config, application)
            detail_label = branch_progress_label(workload_group, config, benchmark_subgroup, application, cpu, cpu_count)
            legend_key = (config, application)
            showlegend = metric_idx == 0 and legend_key not in shown_legend_entries
            if showlegend:
                shown_legend_entries.add(legend_key)
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
                    row["btb_misses"],
                    row["region_btb_misses"],
                    row["btb_target_mispredicts"],
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
                    "legendgroup": config,
                    "legendgrouptitle": {"text": branch_progress_config_label(config)},
                    "showlegend": showlegend,
                    "xaxis": f"x{axis_suffix}",
                    "yaxis": f"y{axis_suffix}",
                    "line": {"color": color, "width": 2},
                    "hovertemplate": (
                        f"<b>{html.escape(detail_label)}</b><br>"
                        "sample=%{x}<br>"
                        f"{ylabel}=%{{y:.4g}}<br>"
                        "instructions=%{customdata[0]}<br>"
                        "cycles=%{customdata[1]}<br>"
                        "aliasing=%{customdata[2]}<br>"
                        "total branch misses=%{customdata[3]}<br>"
                        "BTB misses=%{customdata[4]}<br>"
                        "region BTB misses=%{customdata[5]}<br>"
                        "BTB target mispredicts=%{customdata[6]}<br>"
                        "log=%{customdata[7]}"
                        "<extra></extra>"
                    ),
                }
            )

    plot_count = len(metrics)
    plot_gap = 0.06
    plot_height = (1.0 - plot_gap * (plot_count - 1)) / plot_count
    matched_xaxis = f"x{plot_count if plot_count > 1 else ''}"
    layout = {
        "title": "Branch Progress Time Series",
        "template": "plotly_white",
        "hovermode": "closest",
        "height": 1500,
        "margin": {"l": 80, "r": 280, "t": 80, "b": 70},
        "legend": {
            "title": {"text": "Config - application. Click one item to toggle its config group."},
            "x": 1.02,
            "y": 1,
            "xanchor": "left",
            "yanchor": "top",
            "groupclick": "togglegroup",
        },
        "annotations": [],
    }
    for metric_idx, (_, ylabel) in enumerate(metrics):
        suffix = "" if metric_idx == 0 else str(metric_idx + 1)
        xaxis_name = f"xaxis{suffix}"
        yaxis_name = f"yaxis{suffix}"
        xref = f"x{suffix}"
        yref = f"y{suffix}"
        domain_top = 1.0 - metric_idx * (plot_height + plot_gap)
        domain_bottom = domain_top - plot_height
        layout[xaxis_name] = {
            "domain": [0, 1],
            "anchor": yref,
            "showticklabels": metric_idx == plot_count - 1,
        }
        if metric_idx != plot_count - 1:
            layout[xaxis_name]["matches"] = matched_xaxis
        else:
            layout[xaxis_name]["title"] = {"text": "Sample after warmup"}
        layout[yaxis_name] = {
            "domain": [domain_bottom, domain_top],
            "anchor": xref,
            "title": {"text": ylabel},
        }
        layout["annotations"].append(
            {
                "text": ylabel,
                "xref": "paper",
                "yref": "paper",
                "x": 0.5,
                "y": min(1.04, domain_top + 0.04),
                "showarrow": False,
                "font": {"size": 16},
            }
        )
    config = {
        "responsive": True,
        "displaylogo": False,
        "toImageButtonOptions": {
            "format": "png",
            "filename": output_path.stem,
            "height": 1500,
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
    #branch-progress {{ width: 100%; min-height: 1500px; }}
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


def plot_branch_progress_separate_paper_metrics(rows, output_dir, svg=False):
    x_limits = (0, BRANCH_PROGRESS_PAPER_SAMPLE_LIMIT)
    paper_config_labels = {label for label, slug in BRANCH_PROGRESS_PAPER_CONFIGS}
    paper_rows = [
        row for row in branch_progress_rows_in_sample_window(rows, x_limits)
        if branch_progress_config_label(row["config"]) in paper_config_labels
    ]
    if not paper_rows:
        return

    plotter = (
        plot_branch_progress_small_paper_metric_svg
        if svg
        else plot_branch_progress_small_paper_metric
    )
    suffix = "svg" if svg else "pdf"
    for metric, ylabel in BRANCH_PROGRESS_PAPER_METRICS:
        y_limits = branch_progress_metric_y_limits(paper_rows, metric)
        for config_label, config_slug in BRANCH_PROGRESS_PAPER_CONFIGS:
            config_rows = [
                row for row in paper_rows
                if branch_progress_config_label(row["config"]) == config_label
            ]
            if not config_rows:
                continue
            plotter(
                config_rows,
                paper_rows,
                metric,
                ylabel,
                output_dir / f"branch_progress_{metric}_paper_{config_slug}.{suffix}",
                config_label,
                x_limits,
                y_limits,
            )


def plot_branch_progress_combined(rows, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        plot_branch_progress_separate_paper_metrics(rows, output_dir)
        plot_branch_progress_paper_metric(
            rows,
            "aliasing_mpki",
            "Aliasing per kilo instruction",
            output_dir / "branch_progress_aliasing_mpki_paper.pdf",
        )
        plot_branch_progress_paper_metric(
            rows,
            "branch_mpki",
            "Total branch misses per kilo instruction",
            output_dir / "branch_progress_branch_mpki_paper.pdf",
        )
        plot_branch_progress_paper_metric(
            rows,
            "btb_mpki",
            "BTB misses per kilo instruction",
            output_dir / "branch_progress_btb_mpki_paper.pdf",
        )
        plot_branch_progress_paper_metric(
            rows,
            "region_btb_mpki",
            "Region BTB misses per kilo instruction",
            output_dir / "branch_progress_region_btb_mpki_paper.pdf",
        )
        plot_branch_progress_paper_metric(
            rows,
            "btb_target_mpki",
            "BTB target mispredicts per kilo instruction",
            output_dir / "branch_progress_btb_target_mpki_paper.pdf",
        )
    except ModuleNotFoundError:
        plot_branch_progress_separate_paper_metrics(rows, output_dir, svg=True)
        plot_branch_progress_paper_metric_svg(
            rows,
            "aliasing_mpki",
            "Aliasing per kilo instruction",
            output_dir / "branch_progress_aliasing_mpki_paper.svg",
        )
        plot_branch_progress_paper_metric_svg(
            rows,
            "branch_mpki",
            "Total branch misses per kilo instruction",
            output_dir / "branch_progress_branch_mpki_paper.svg",
        )
        plot_branch_progress_paper_metric_svg(
            rows,
            "btb_mpki",
            "BTB misses per kilo instruction",
            output_dir / "branch_progress_btb_mpki_paper.svg",
        )
        plot_branch_progress_paper_metric_svg(
            rows,
            "region_btb_mpki",
            "Region BTB misses per kilo instruction",
            output_dir / "branch_progress_region_btb_mpki_paper.svg",
        )
        plot_branch_progress_paper_metric_svg(
            rows,
            "btb_target_mpki",
            "BTB target mispredicts per kilo instruction",
            output_dir / "branch_progress_btb_target_mpki_paper.svg",
        )

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
        btb_path = output_dir / "branch_progress_btb_mpki_timeseries.png"
        region_btb_path = output_dir / "branch_progress_region_btb_mpki_timeseries.png"
        btb_target_path = output_dir / "branch_progress_btb_target_mpki_timeseries.png"

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
        plot_branch_progress_metric(
            rows,
            "btb_mpki",
            "BTB misses per kilo instruction",
            btb_path,
        )
        plot_branch_progress_metric(
            rows,
            "region_btb_mpki",
            "Region BTB misses per kilo instruction",
            region_btb_path,
        )
        plot_branch_progress_metric(
            rows,
            "btb_target_mpki",
            "BTB target mispredicts per kilo instruction",
            btb_target_path,
        )
    except ModuleNotFoundError:
        aliasing_path = output_dir / "branch_progress_aliasing_mpki_timeseries.svg"
        branch_path = output_dir / "branch_progress_branch_mpki_timeseries.svg"
        btb_path = output_dir / "branch_progress_btb_mpki_timeseries.svg"
        region_btb_path = output_dir / "branch_progress_region_btb_mpki_timeseries.svg"
        btb_target_path = output_dir / "branch_progress_btb_target_mpki_timeseries.svg"

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
        plot_branch_progress_metric_svg(
            rows,
            "btb_mpki",
            "BTB misses per kilo instruction",
            btb_path,
        )
        plot_branch_progress_metric_svg(
            rows,
            "region_btb_mpki",
            "Region BTB misses per kilo instruction",
            region_btb_path,
        )
        plot_branch_progress_metric_svg(
            rows,
            "btb_target_mpki",
            "BTB target mispredicts per kilo instruction",
            btb_target_path,
        )

    write_html_wrapper(
        output_dir / "branch_progress_timeseries.html",
        [aliasing_path, branch_path, btb_path, region_btb_path, btb_target_path],
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
    rows.sort(key=branch_progress_row_sort_key)
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
