#!/usr/bin/env python3
"""Compare matched replacement-policy configurations using ROI IPC."""

import argparse
import csv
import math
import os
import statistics
import sys
import tempfile
from pathlib import Path

from plot_ipc import collect_results, natural_key


def compare(records, baseline="lru", policy="reference_count", pairwise=False):
    """Match policy suffixes and compute means on identical traces."""
    if baseline == policy or not baseline or not policy:
        raise ValueError("choose two distinct, nonempty policy suffixes")
    pairs = {}
    for name in records:
        for suffix in (baseline, policy):
            if name.endswith("_" + suffix):
                key = name[: -(len(suffix) + 1)]
                pairs.setdefault(key, {})[suffix] = name
    if not pairs:
        raise ValueError("no configurations with the requested policy suffixes found")
    for key, members in pairs.items():
        if len(members) != 2:
            raise ValueError(f"missing policy counterpart for {key}: expected _{baseline} and _{policy}")
    common = set.intersection(*(set(records[name]) for members in pairs.values() for name in members.values()))
    if not pairwise and not common:
        raise ValueError("no traces common to the whole sweep; use --pairwise for matched traces within each pair")
    rows, summary = [], []
    for key in sorted(pairs, key=natural_key):
        base_name, policy_name = pairs[key][baseline], pairs[key][policy]
        traces = (set(records[base_name]) & set(records[policy_name])) if pairwise else common
        if not traces:
            raise ValueError(f"no matched traces for {key}")
        paired = []
        for trace in sorted(traces, key=natural_key):
            base, variant = records[base_name][trace], records[policy_name][trace]
            if any(not math.isfinite(e["ipc"]) or e["ipc"] <= 0 for e in (base, variant)):
                raise ValueError(f"IPC must be finite and positive: {key}/{trace}")
            row = dict(configuration=key, baseline=base_name, policy=policy_name,
                       trace_group=base["group"], trace=trace, baseline_ipc=base["ipc"],
                       policy_ipc=variant["ipc"], speedup_percent=100 * (variant["ipc"] / base["ipc"] - 1),
                       baseline_source=base["path"], policy_source=variant["path"])
            paired.append(row)
        rows.extend(paired)
        for category in ["overall", *sorted({r["trace_group"] for r in paired}, key=natural_key)]:
            selected = [r for r in paired if category == "overall" or r["trace_group"] == category]
            summary.append(dict(configuration=key, category=category, trace_count=len(selected),
                                baseline_ipc=statistics.harmonic_mean(r["baseline_ipc"] for r in selected),
                                policy_ipc=statistics.harmonic_mean(r["policy_ipc"] for r in selected),
                                speedup_percent=100 * (statistics.harmonic_mean(
                                    r["policy_ipc"] / r["baseline_ipc"] for r in selected) - 1)))
    return rows, summary


def compare_reference(records, reference="litebtb_i32k_b4k", baseline="lru", policy="reference_count", pairwise=False):
    """Compare both replacement policies against a fixed configuration."""
    if reference not in records:
        raise ValueError(f"missing baseline configuration: {reference}; provide --baseline-results-dir if stored separately")
    # Validate suffix pairs before selecting the common reference workload.
    compare(records, baseline, policy, pairwise)
    names = sorted((name for name in records if name.endswith(("_" + baseline, "_" + policy))), key=natural_key)
    common = set.intersection(set(records[reference]), *(set(records[name]) for name in names))
    if not pairwise and not common:
        raise ValueError("no traces common to the baseline and whole sweep")
    rows, summary = [], []
    for name in names:
        traces = set(records[reference]) & set(records[name]) if pairwise else common
        if not traces:
            raise ValueError(f"no matched traces for {name} and {reference}")
        selected_records = {
            name + "_baseline": {t: records[reference][t] for t in traces},
            name + "_variant": {t: records[name][t] for t in traces},
        }
        per_trace, aggregated = compare(selected_records, "baseline", "variant")
        for row in per_trace:
            row.update(baseline=reference, policy=name)
        rows.extend(per_trace)
        summary.extend(aggregated)
    return rows, summary


def write_csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def plot(summary, output, baseline, policy, title):
    os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / f"champsim-matplotlib-{os.getuid()}"))
    try:
        import importlib_resources  # noqa: F401
    except ModuleNotFoundError:
        from importlib import resources
        sys.modules["importlib_resources"] = resources
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    configurations = sorted({r["configuration"] for r in summary}, key=natural_key)
    categories = ["overall", *sorted({r["category"] for r in summary} - {"overall"}, key=natural_key)]
    figure, axes = plt.subplots(len(categories), 2, squeeze=False,
                                figsize=(max(12, len(configurations) * 2), 3.6 * len(categories)),
                                constrained_layout=True)
    lookup = {(r["configuration"], r["category"]): r for r in summary}
    for category, (absolute, relative) in zip(categories, axes):
        entries = [lookup.get((name, category), {}) for name in configurations]
        x = list(range(len(configurations)))
        for offset, metric, label, color in ((-0.2, "baseline_ipc", baseline, "#0072B2"),
                                             (0.2, "policy_ipc", policy, "#D55E00")):
            absolute.bar([i + offset for i in x], [r.get(metric, float("nan")) for r in entries],
                         width=0.4, label=label, color=color)
        relative.bar(x, [r.get("speedup_percent", float("nan")) for r in entries], color="#009E73")
        relative.axhline(0, color="black", linewidth=0.8)
        absolute.set_ylabel("Harmonic mean ROI IPC")
        relative.set_ylabel(f"{policy} vs {baseline} (%)")
        absolute.legend()
        for axis in (absolute, relative):
            axis.set_title(category)
            axis.set_xticks(x, configurations, rotation=25, ha="right", fontsize=8)
            axis.grid(axis="y", linestyle=":", alpha=0.5)
    figure.suptitle(title)
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=200)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results_dir", type=Path, help="Experiment result root containing configuration directories")
    parser.add_argument("-o", "--output", type=Path, help="Figure path (default: <results_dir>/replacement_comparison.pdf)")
    parser.add_argument("--baseline", default="litebtb_i32k_b4k", help="Fixed reference configuration (default: litebtb_i32k_b4k)")
    parser.add_argument("--baseline-results-dir", type=Path, help="Separate result root containing the reference configuration")
    parser.add_argument("--baseline-policy", default="lru", help="Baseline configuration suffix (default: lru)")
    parser.add_argument("--policy", default="reference_count", help="Compared configuration suffix (default: reference_count)")
    parser.add_argument("--core", type=int, default=0)
    parser.add_argument("--pairwise", action="store_true", help="Use common traces within each pair; sizes may cover different workloads")
    parser.add_argument("--title", default="Replacement policy IPC comparison")
    args = parser.parse_args()
    if args.core < 0 or not args.results_dir.is_dir():
        parser.error("provide an existing result directory and a non-negative core index")
    records, errors = collect_results(args.results_dir.resolve(), args.core)
    if args.baseline_results_dir:
        reference_records, reference_errors = collect_results(args.baseline_results_dir.resolve(), args.core)
        errors.extend(reference_errors)
        if args.baseline in reference_records:
            records[args.baseline] = reference_records[args.baseline]
    # Silently dropping malformed results could bias a policy comparison.
    if errors:
        parser.error("invalid result files:\n" + "\n".join(errors))
    try:
        rows, summary = compare_reference(records, args.baseline, args.baseline_policy, args.policy, args.pairwise)
    except ValueError as error:
        parser.error(str(error))
    output = args.output or args.results_dir / "replacement_comparison.pdf"
    trace_csv = output.with_suffix(".csv")
    plot_csv = output.with_name(output.stem + "_plot.csv")
    write_csv(trace_csv, rows)
    write_csv(plot_csv, summary)
    plot(summary, output, args.baseline, "replacement policy", args.title)
    for row in summary:
        if row["category"] == "overall":
            print(f"{row['configuration']}: {row['speedup_percent']:+.3f}% ({row['trace_count']} paired traces)")
    print(f"Plot: {output}\nPer-trace CSV: {trace_csv}\nPlot CSV: {plot_csv}")


if __name__ == "__main__":
    main()
