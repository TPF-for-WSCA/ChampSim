#!/usr/bin/env python3
"""Plot matched CPU-plus-DRAM ROI IPC per joule for the IPC1 BTB sweep."""

import argparse
import csv
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

TRACE_SUFFIX = ".champsimtrace.xz"
BASELINE_FRONTEND = {"DIB", "FDIP FTQ", "FDIP next-PC logic"}
DESIGNS = {
    "baseline": ("Baseline BTB", "#0072B2", "o"),
    "litebtb": ("LiteBTB", "#009E73", "^"),
}
DRAM_REPLAY = "DRAMPower replay"
UTB_GROUP = "Unique tag buffer"
COMPONENT_COLORS = {
    "BTB target table": "#0072B2",
    UTB_GROUP: "#56B4E9",
    "ITTAGE": "#2D4B84",
    "Return-address stack": "#4B93A5",
    "Direction predictor": "#009E73",
    "Core backend": "#D55E00",
    "L1I": "#E69F00",
    "L1D": "#F0CC75",
    "L2": "#A46628",
    "LLC": "#B185B8",
    "Address translation": "#CC79A7",
    "On-chip links": "#718B45",
    "DRAM controller": "#3D7562",
    DRAM_REPLAY: "#8D9BA8",
}


def component_group(instance, model):
    """Identify the hardware block for a reported energy meter."""
    if model in BASELINE_FRONTEND:
        return model
    if model == "HARCOM on-chip point-to-point link":
        return "On-chip links"
    if model == "HARCOM DRAM controller":
        return "DRAM controller"

    block = instance.split(maxsplit=1)[0]
    if block.startswith("cpu0_"):
        block = block.removeprefix("cpu0_")
    if block in {"ITLB", "DTLB", "STLB", "PTW"}:
        return "Address translation"
    if block in {"L1I", "L1D", "L2C", "LLC"}:
        return "L2" if block == "L2C" else block
    if instance.startswith("CPU 0 branch predictor "):
        return "Direction predictor"
    if model in {"basic BTB direct table", "xbtbx target table"}:
        return "BTB target table"
    for suffix, group in ((" UTB", UTB_GROUP), (" ITTAGE", "ITTAGE"),
                          (" return-address stack", "Return-address stack")):
        if model.endswith(suffix):
            return group
    return "Core backend" if block == "CPU" else instance


def load_run(path, trace, design):
    """Read ROI metrics and retain all reported component meters."""
    try:
        (phase,) = json.loads(path.read_text(encoding="utf-8"))
        if phase["name"] != "Simulation":
            raise ValueError("expected a measured Simulation phase")
        (recorded_trace,) = phase["traces"]
        if Path(recorded_trace).parts[-len(trace.parts):] != trace.parts:
            raise ValueError(f"expected trace {trace}, found {recorded_trace}")
        (core,) = phase["roi"]["cores"]
        instructions, cycles = core["instructions"], core["cycles"]
        if instructions <= 0 or cycles <= 0:
            raise ValueError("ROI instructions and cycles must be positive")

        energy = phase["energy"]
        reported = energy["combined_modeled_energy_joules"]
        frontend = [component for component in energy["components"] if component["model"] in BASELINE_FRONTEND]
        expected = BASELINE_FRONTEND if design == "baseline" else set()
        if len(frontend) != len(expected) or {component["model"] for component in frontend} != expected:
            raise ValueError("unexpected DIB, FDIP FTQ or next-PC meter coverage")
        excluded = math.fsum(component["modeled_total_energy_joules"] for component in frontend)
        dram = energy["dram_replay"]["energy_joules"]
        matched = reported - excluded
        if not all(math.isfinite(value) and value >= 0 for value in (reported, excluded, dram)) or matched <= 0:
            raise ValueError("invalid modeled energy")
        meters = {}
        for component in energy["components"]:
            instance, model = component["instance"], component["model"]
            key = (instance, model)
            if key in meters:
                raise ValueError(f"duplicate energy meter {instance} ({model})")
            dynamic = component["dynamic_energy_joules"]
            leakage = component["leakage_energy_joules"]
            total = component["modeled_total_energy_joules"]
            if not math.isclose(dynamic + (leakage or 0), total, rel_tol=1e-9, abs_tol=1e-15):
                raise ValueError(f"energy split does not match total for {instance} ({model})")
            meters[key] = dynamic, leakage, total
        meters[(DRAM_REPLAY, DRAM_REPLAY)] = (None, None, dram)
        included_total = math.fsum(total for (_, model), (_, _, total) in meters.items() if model not in BASELINE_FRONTEND)
        if not math.isclose(included_total, matched, rel_tol=1e-9):
            raise ValueError(f"component energy {included_total} J does not match IPC/J denominator {matched} J")
    except (ValueError, KeyError, TypeError) as error:
        raise ValueError(f"{path}: {error}") from error

    ipc = instructions / cycles
    return dict(instructions=instructions, cycles=cycles, ipc=ipc, dram_energy_joules=dram,
                reported_energy_joules=reported, excluded_frontend_energy_joules=excluded,
                matched_energy_joules=matched, ipc_per_joule=ipc / matched), meters


def mean_meter_energy(values, trace_count):
    """Average over the full manifest, with zero contribution from absent meters."""
    reported = [value for value in values if value is not None]
    return math.fsum(reported) / trace_count if reported else None


def collect(experiment):
    """Collect the complete sweep using the snapshot and manifest saved by Slurm."""
    manifest = (experiment / "traces.txt").read_text(encoding="utf-8").splitlines()
    traces = [Path(entry) for entry in manifest]
    if not traces or len(traces) != len(set(traces)):
        raise ValueError("traces.txt must contain a nonempty list of unique traces")
    if any(trace.is_absolute() or ".." in trace.parts or not str(trace).endswith(TRACE_SUFFIX) for trace in traces):
        raise ValueError("traces.txt must list relative ChampSim trace paths")

    config_dir = experiment / "build/IDUN_CONFIGS/harcom_btb_ipc1"
    configs = {}
    for path in sorted(config_dir.glob("*.json")):
        cfg = json.loads(path.read_text(encoding="utf-8"))
        name = Path(cfg["executable_name"]).name
        design = name.split("_", 1)[0]
        (cpu,) = cfg["ooo_cpu"]
        entries = cpu["btb_sets"] * cpu["btb_ways"]
        key = (entries, design)
        if design not in DESIGNS or entries <= 0 or key in configs:
            raise ValueError(f"invalid or duplicate sweep configuration: {path}")
        configs[key] = name
    if not configs:
        raise ValueError(f"no saved configs in {config_dir}")
    for entries in {entries for entries, _ in configs}:
        if any((entries, design) not in configs for design in DESIGNS):
            raise ValueError(f"missing baseline or LiteBTB configuration at {entries} entries")

    summary = []
    runs = []
    components = []
    missing = []
    for (entries, design), name in sorted(configs.items()):
        metadata = dict(configuration=name, design=design, btb_entries=entries)
        config_runs = []
        config_meters = defaultdict(list)
        for trace in traces:
            result = str(trace)[:-len(TRACE_SUFFIX)] + ".json"
            path = experiment / "results" / name / result
            if not path.is_file():
                missing.append(path.relative_to(experiment))
                continue
            metrics, meters = load_run(path, trace, design)
            config_runs.append(dict(metadata, trace=str(trace), **metrics))
            for key, values in meters.items():
                config_meters[key].append(values)
        runs.extend(config_runs)
        if len(config_runs) == len(traces):
            ipc_per_joule = statistics.geometric_mean(row["ipc_per_joule"] for row in config_runs)
            summary.append(dict(metadata, trace_count=len(traces), ipc_per_joule=ipc_per_joule))
            for (instance, model), samples in sorted(config_meters.items()):
                dynamic, leakage, total = (mean_meter_energy(values, len(traces)) for values in zip(*samples))
                group = DRAM_REPLAY if model == DRAM_REPLAY else component_group(instance, model)
                components.append(dict(metadata, trace_count=len(traces), traces_reporting=len(samples),
                                       component=instance, model=model, group=group,
                                       included_in_ipc_per_joule=model not in BASELINE_FRONTEND,
                                       mean_dynamic_energy_joules=dynamic, mean_leakage_energy_joules=leakage,
                                       mean_total_energy_joules=total))

    if missing:
        examples = ", ".join(map(str, missing[:3]))
        raise ValueError(f"missing {len(missing)} completed result(s), including {examples}")
    return summary, runs, components


def write_csv(path, rows):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def plot(summary, path):
    from matplotlib.backends.backend_agg import FigureCanvasAgg
    from matplotlib.figure import Figure

    figure = Figure(figsize=(7.2, 4.2), layout="constrained")
    FigureCanvasAgg(figure)
    ax = figure.subplots()
    for design, (label, color, marker) in DESIGNS.items():
        rows = [row for row in summary if row["design"] == design]
        ax.plot([row["btb_entries"] for row in rows], [row["ipc_per_joule"] for row in rows],
                label=label, color=color, marker=marker)
    capacities = sorted({row["btb_entries"] for row in summary})
    ax.set_xscale("log", base=2)
    ax.set_xticks(capacities, [f"{size // 1024}K" if size % 1024 == 0 else str(size) for size in capacities])
    ax.set_xlabel("BTB entries")
    ax.set_ylabel("Geometric mean of ROI IPC/J")
    ax.set_title(f"IPC1 · {summary[0]['trace_count']} traces per configuration", loc="left")
    ax.grid(axis="y", alpha=0.25)
    ax.legend(frameon=False)
    figure.savefig(path, dpi=200)


def plot_energy(summary, energy, colors, title, path):
    """Plot mean ROI energy per configuration as stacked bars."""
    from matplotlib.backends.backend_agg import FigureCanvasAgg
    from matplotlib.figure import Figure

    figure = Figure(figsize=(10.5, 5.8), layout="constrained")
    FigureCanvasAgg(figure)
    ax = figure.subplots()
    configs = [row["configuration"] for row in summary]
    positions = range(len(configs))
    bottoms = [0.0] * len(configs)
    groups = {group for totals in energy.values() for group in totals}
    for group in [name for name in colors if name in groups] + sorted(groups - colors.keys()):
        values = [energy[name].get(group, 0) * 1e3 for name in configs]
        ax.bar(positions, values, bottom=bottoms, width=0.76, label=group, color=colors.get(group))
        bottoms = [bottom + value for bottom, value in zip(bottoms, values)]
    labels = []
    for row in summary:
        size = row["btb_entries"]
        count = f"{size // 1024}K" if size % 1024 == 0 else str(size)
        labels.append(f"{DESIGNS[row['design']][0]}\n{count}")
    ax.set_xticks(positions, labels, fontsize=8)
    ax.set_ylabel("Mean ROI energy (mJ)")
    ax.set_title(title, loc="left")
    ax.set_axisbelow(True)
    ax.grid(axis="y", alpha=0.25)
    ax.legend(loc="upper left", bbox_to_anchor=(1.0, 1.0), frameon=False)
    figure.savefig(path, dpi=200)


def plot_energy_breakdowns(summary, components, prefix):
    by_component = defaultdict(lambda: defaultdict(float))
    by_type = defaultdict(lambda: defaultdict(float))
    for row in components:
        if not row["included_in_ipc_per_joule"]:
            continue
        name, group = row["configuration"], row["group"]
        total = row["mean_total_energy_joules"]
        by_component[name][group] += total
        if group == DRAM_REPLAY:
            by_type[name][DRAM_REPLAY] += total
        else:
            by_type[name]["Dynamic (on-chip)"] += row["mean_dynamic_energy_joules"] or 0
            by_type[name]["Static (on-chip leakage)"] += row["mean_leakage_energy_joules"] or 0
    by_type_colors = {"Dynamic (on-chip)": "#0072B2", "Static (on-chip leakage)": "#E69F00",
                      DRAM_REPLAY: COMPONENT_COLORS[DRAM_REPLAY]}
    for energy, colors, title, suffix in (
        (by_component, COMPONENT_COLORS, "Matched energy by hardware component", "components"),
        (by_type, by_type_colors, "Matched energy by type", "static_dynamic"),
    ):
        plot_energy(summary, energy, colors, title, prefix.parent / f"{prefix.name}_{suffix}.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiment", type=Path, help="experiment directory printed by submit.sh")
    parser.add_argument("--output", type=Path, help="output filename prefix (default: EXPERIMENT/ipc_per_joule)")
    args = parser.parse_args()
    prefix = args.output or args.experiment / "ipc_per_joule"
    png = prefix.parent / f"{prefix.name}.png"
    try:
        summary, runs, components = collect(args.experiment)
        prefix.parent.mkdir(parents=True, exist_ok=True)
        plot(summary, png)
        write_csv(prefix.parent / f"{prefix.name}.csv", summary)
        write_csv(prefix.parent / f"{prefix.name}_per_trace.csv", runs)
        write_csv(prefix.parent / f"{prefix.name}_components.csv", components)
        plot_energy_breakdowns(summary, components, prefix)
    except (ValueError, OSError, KeyError, TypeError, ImportError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Wrote {len(summary)} configuration points from {len(runs)} runs to {png}")


if __name__ == "__main__":
    main()
