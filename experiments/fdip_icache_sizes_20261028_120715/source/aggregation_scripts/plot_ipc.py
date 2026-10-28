#!/usr/bin/env python3
"""Plot ROI IPC for the configurations in a ChampSim result directory."""

import argparse
import csv
import json
import os
import re
import statistics
import sys
import tempfile
from collections import defaultdict
from pathlib import Path


TRACE_SUFFIX = ".champsimtrace.xz"
DEFAULT_BASELINE_SIZE = "16k"


def natural_key(value):
    """Sort names containing numbers in human order (8k, 16k, 32k, ...)."""
    return tuple(int(part) if part.isdigit() else part.lower() for part in re.split(r"(\d+)", value))


def configuration_storage(configuration):
    """Return nominal cache capacity in KiB from the configuration name."""
    match = re.search(r"(?:^|[_-])(\d+)k(?:$|[_-])", configuration, re.IGNORECASE)
    if match is None:
        return None
    size = int(match.group(1))
    return size


def configuration_key(configuration):
    storage = configuration_storage(configuration)
    is_ubs = bool(re.search(r"(?:^|[_-])ubs(?:$|[_-])", configuration, re.IGNORECASE))
    return (storage is None, storage or 0, is_ubs, natural_key(configuration))


def trace_name(trace_path):
    name = Path(trace_path).name
    return name[: -len(TRACE_SUFFIX)] if name.endswith(TRACE_SUFFIX) else Path(name).stem


def trace_group(name):
    return name.split("_", maxsplit=1)[0]


def load_result(path, core_index):
    """Return (trace name, group, weighted ROI IPC) from one ChampSim JSON file."""
    with path.open(encoding="utf-8") as stream:
        phases = json.load(stream)

    if isinstance(phases, dict):
        phases = [phases]
    if not isinstance(phases, list) or not phases:
        raise ValueError("expected a non-empty phase list")

    instructions = 0
    cycles = 0
    traces = set()
    for phase in phases:
        phase_traces = phase.get("traces", [])
        if len(phase_traces) != 1:
            raise ValueError("expected exactly one trace per phase")
        traces.add(trace_name(phase_traces[0]))

        cores = phase.get("roi", {}).get("cores", [])
        if core_index >= len(cores):
            raise ValueError(f"ROI does not contain core {core_index}")
        instructions += int(cores[core_index]["instructions"])
        cycles += int(cores[core_index]["cycles"])

    if len(traces) != 1:
        raise ValueError("phases refer to different traces")
    if cycles <= 0:
        raise ValueError("ROI cycle count must be positive")

    name = traces.pop()
    return name, trace_group(name), instructions / cycles


def infer_configuration(results_root, result_path, trace):
    """Infer the binary/configuration name from runner or flat smoke output."""
    relative = result_path.relative_to(results_root)

    # idun_runner.sh layout: <binary>/<trace-group>/<trace>.json
    if len(relative.parts) >= 3:
        return relative.parts[0]

    # If the user points directly at one binary directory, use its name.
    if len(relative.parts) == 2:
        return results_root.name

    # Flat smoke-test layout: <configuration>-<trace>.json
    suffix = f"-{trace}"
    if result_path.stem.endswith(suffix):
        return result_path.stem[: -len(suffix)]
    return result_path.stem


def collect_results(results_root, core_index):
    records = defaultdict(dict)
    errors = []

    for path in sorted(results_root.rglob("*.json")):
        try:
            trace, group, ipc = load_result(path, core_index)
            configuration = infer_configuration(results_root, path, trace)
            if trace in records[configuration]:
                raise ValueError(f"duplicate result for {configuration}/{trace}")
            records[configuration][trace] = {"group": group, "ipc": ipc, "path": path}
        except (KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
            errors.append(f"{path}: {error}")

    return records, errors


def select_traces(records, allow_incomplete):
    trace_sets = [set(configuration) for configuration in records.values()]
    if allow_incomplete:
        return set.union(*trace_sets)
    return set.intersection(*trace_sets)


def summarize(records, configurations, selected_traces, relative=False):
    groups = sorted(
        {
            entry["group"]
            for configuration in records.values()
            for trace, entry in configuration.items()
            if trace in selected_traces
        },
        key=natural_key,
    )
    categories = ["overall", *groups]
    summary = defaultdict(dict)

    for configuration in configurations:
        for category in categories:
            values = [
                entry["ipc"]
                for trace, entry in records[configuration].items()
                if trace in selected_traces and (category == "overall" or entry["group"] == category)
            ]
            if values:
                summary[configuration][category] = {
                    # Relative values may be negative; average their positive
                    # speedup ratios harmonically before converting back to %.
                    "mean": (100 * (statistics.harmonic_mean(1 + v / 100 for v in values) - 1)
                             if relative else statistics.harmonic_mean(values)),
                    "count": len(values),
                }

    return categories, summary


def select_default_baseline(configurations):
    """Return the configuration whose name identifies it as the 16 KiB variant."""
    size_pattern = re.compile(rf"(?:^|[_-]){DEFAULT_BASELINE_SIZE}(?:$|[_-])", re.IGNORECASE)
    matches = [configuration for configuration in configurations if size_pattern.search(configuration)]
    baseline_matches = [
        configuration
        for configuration in matches
        if not re.search(r"(?:^|[_-])ubs(?:$|[_-])", configuration, re.IGNORECASE)
    ]
    if len(baseline_matches) == 1:
        return baseline_matches[0]
    if len(matches) != 1:
        available = ", ".join(configurations)
        if matches:
            raise ValueError(f"multiple 16k baseline configurations found: {', '.join(matches)}")
        raise ValueError(
            f"no 16k baseline configuration found; use --baseline to select one from: {available}"
        )
    return matches[0]


def deviation_from_baseline(records, configurations, selected_traces, baseline):
    """Return per-trace IPC percentage deviations from the baseline configuration."""
    deviations = defaultdict(dict)
    baseline_records = records[baseline]

    for configuration in configurations:
        for trace in selected_traces:
            entry = records[configuration].get(trace)
            baseline_entry = baseline_records.get(trace)
            if entry is None or baseline_entry is None:
                continue
            if baseline_entry["ipc"] <= 0:
                raise ValueError(f"baseline IPC must be positive for trace {trace}")
            deviations[configuration][trace] = {
                **entry,
                "ipc": 100.0 * (entry["ipc"] / baseline_entry["ipc"] - 1.0),
            }

    return deviations


def write_csv(records, configurations, selected_traces, output_path, value_name="ipc"):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["configuration", "trace_group", "trace", value_name, "source"])
        for configuration in configurations:
            for trace in sorted(selected_traces, key=natural_key):
                entry = records[configuration].get(trace)
                if entry is None:
                    continue
                writer.writerow([configuration, entry["group"], trace, entry["ipc"], entry["path"]])


def write_plot_csv(summary, configurations, categories, output_path, value_name):
    """Export the aggregated bar heights, preserving Python float precision."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["configuration", "category", "storage_kib", "metric", "value", "trace_count"])
        for configuration in sorted(configurations, key=configuration_key):
            for category in categories:
                details = summary[configuration].get(category)
                if details is not None:
                    writer.writerow([configuration, category, configuration_storage(configuration),
                                     value_name, details["mean"], details["count"]])


def plot(
    summary,
    configurations,
    categories,
    output_path,
    title,
    ylabel,
    reference_line=None,
    center_on_reference=False,
):
    cache_dir = Path(tempfile.gettempdir()) / f"champsim-matplotlib-{os.getuid()}"
    cache_dir.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("MPLCONFIGDIR", str(cache_dir))

    try:
        # Some cluster Matplotlib installations require the backport package
        # even on Python versions that provide the same API in the stdlib.
        try:
            import importlib_resources  # noqa: F401
        except ModuleNotFoundError:
            from importlib import resources

            sys.modules["importlib_resources"] = resources

        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.patches import Patch
    except ImportError as error:
        raise RuntimeError("matplotlib and its Python dependencies are required to generate the plot") from error

    plt.style.use("tableau-colorblind10")
    storage_groups = defaultdict(list)
    for configuration in sorted(configurations, key=configuration_key):
        storage = configuration_storage(configuration)
        storage_groups[storage if storage is not None else configuration].append(configuration)

    figure_width = max(7.0, 1.6 * len(storage_groups))
    figure, axis = plt.subplots(figsize=(figure_width, 4.8), constrained_layout=True)

    group_width = 0.82
    variant_count = max(map(len, storage_groups.values()))
    bar_width = group_width / (len(categories) * variant_count)
    colors = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    for group_index, members in enumerate(storage_groups.values()):
        for category_index, category in enumerate(categories):
            for variant_index, configuration in enumerate(members):
                offset = (category_index - (len(categories) - 1) / 2) * variant_count * bar_width
                offset += (variant_index - (len(members) - 1) / 2) * bar_width
                is_ubs = bool(re.search(r"(?:^|[_-])ubs(?:$|[_-])", configuration, re.IGNORECASE))
                value = summary[configuration].get(category, {}).get("mean", float("nan"))
                axis.bar(
                    group_index + offset, value, width=bar_width,
                    color=colors[category_index % len(colors)],
                    hatch="///" if is_ubs else "", edgecolor="black", linewidth=0.4,
                )

    if reference_line is not None:
        axis.axhline(reference_line, color="black", linewidth=1, linestyle="--", alpha=0.65)

    axis.set_title(title)
    axis.set_ylabel(ylabel)
    axis.set_xlabel("Nominal cache capacity (KiB)")
    labels = [f"{storage:g} KiB" if isinstance(storage, (int, float)) else storage for storage in storage_groups]
    axis.set_xticks(range(len(storage_groups)), labels)
    if center_on_reference:
        values = [
            details["mean"]
            for configuration in configurations
            for details in summary[configuration].values()
        ]
        extent = max((abs(value - reference_line) for value in values), default=0.0)
        extent = max(1.0, extent * 1.1)
        axis.set_ylim(reference_line - extent, reference_line + extent)
    else:
        axis.set_ylim(bottom=0)
    axis.grid(axis="y", linestyle=":", alpha=0.55)
    handles = [Patch(facecolor=colors[index % len(colors)], label=category) for index, category in enumerate(categories)]
    handles.extend([
        Patch(facecolor="white", edgecolor="black", label="Conventional"),
        Patch(facecolor="white", edgecolor="black", hatch="///", label="UBS"),
    ])
    axis.legend(handles=handles, ncols=min(4, len(handles)))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_path, dpi=200)
    plot_csv_path = output_path.with_name(output_path.stem + "_plot.csv")
    write_plot_csv(summary, configurations, categories, plot_csv_path, ylabel)
    plt.close(figure)


def suffixed_path(path, suffix):
    return path.with_name(f"{path.stem}{suffix}{path.suffix}")


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Plot mean ROI IPC for every binary configuration in a result directory. "
            "Configurations are ordered by nominal cache capacity; UBS and conventional sizes pair directly. "
            "By default, only traces present for every configuration are compared."
        )
    )
    parser.add_argument("results_dir", type=Path, help="Result root produced by idun_runner.sh")
    parser.add_argument("-o", "--output", type=Path, help="Output plot (.pdf, .png, etc.)")
    parser.add_argument("--csv", type=Path, help="Optional path for the extracted per-trace IPC data")
    parser.add_argument(
        "--relative-output",
        type=Path,
        help="Relative IPC deviation plot path (default: <output stem>_relative<suffix>)",
    )
    parser.add_argument(
        "--relative-csv",
        type=Path,
        help="Per-trace IPC deviation data path (default: alongside the relative plot)",
    )
    parser.add_argument(
        "--baseline",
        help="Configuration used as the relative baseline (default: the 16k configuration)",
    )
    parser.add_argument("--core", type=int, default=0, help="Core index to plot (default: 0)")
    parser.add_argument("--title", default="ChampSim IPC comparison", help="Plot title")
    parser.add_argument(
        "--allow-incomplete",
        action="store_true",
        help="Include unmatched traces; means may then cover different workloads",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    results_root = args.results_dir.resolve()
    if not results_root.is_dir():
        raise SystemExit(f"Result directory does not exist: {results_root}")
    if args.core < 0:
        raise SystemExit("--core must be non-negative")

    output_path = args.output or results_root / "ipc_comparison.pdf"
    csv_path = args.csv or output_path.with_suffix(".csv")
    relative_output_path = args.relative_output or suffixed_path(output_path, "_relative")
    relative_csv_path = args.relative_csv or relative_output_path.with_suffix(".csv")

    records, errors = collect_results(results_root, args.core)
    for error in errors:
        print(f"warning: {error}", file=sys.stderr)
    if not records:
        raise SystemExit(f"No ChampSim JSON results found below {results_root}")

    configurations = sorted(records, key=configuration_key)
    try:
        baseline = args.baseline or select_default_baseline(configurations)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    if baseline not in records:
        available = ", ".join(configurations)
        raise SystemExit(f"Unknown baseline {baseline!r}; available configurations: {available}")

    selected_traces = select_traces(records, args.allow_incomplete)
    if not selected_traces:
        raise SystemExit("Configurations have no matching traces to compare")

    categories, summary = summarize(records, configurations, selected_traces)
    try:
        deviation_records = deviation_from_baseline(records, configurations, selected_traces, baseline)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    relative_categories, relative_summary = summarize(deviation_records, configurations, selected_traces, relative=True)

    write_csv(records, configurations, selected_traces, csv_path)
    write_csv(
        deviation_records,
        configurations,
        selected_traces,
        relative_csv_path,
        value_name=f"ipc_percent_deviation_from_{baseline}",
    )
    plot(summary, configurations, categories, output_path, args.title, "Harmonic mean ROI IPC")
    plot(
        relative_summary,
        configurations,
        relative_categories,
        relative_output_path,
        f"{args.title} (deviation from {baseline})",
        f"Harmonic mean speedup vs {baseline} (%)",
        reference_line=0.0,
        center_on_reference=True,
    )

    mode = "union" if args.allow_incomplete else "intersection"
    print(f"Compared {len(configurations)} configuration(s) using {len(selected_traces)} trace(s), mode={mode}.")
    for configuration in configurations:
        values = summary[configuration]
        details = ", ".join(
            f'{category}={values[category]["mean"]:.4f} ({values[category]["count"]})'
            for category in categories
            if category in values
        )
        print(f"  {configuration}: {details}")
    print(f"Plot: {output_path}")
    print(f"CSV:  {csv_path}")
    print(f"Exact plot CSV: {output_path.with_name(output_path.stem + '_plot.csv')}")
    print(f"Relative baseline: {baseline}")
    print(f"Relative plot: {relative_output_path}")
    print(f"Relative CSV:  {relative_csv_path}")
    print(f"Exact relative plot CSV: {relative_output_path.with_name(relative_output_path.stem + '_plot.csv')}")


if __name__ == "__main__":
    main()
