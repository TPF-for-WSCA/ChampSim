#!/usr/bin/env python3
"""Plot ROI performance against measured L1I + BTB storage, sorted by storage."""
import argparse
import csv
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
                             total_storage_kib=total, ipc=ipc, ipc_per_kib=ipc / total))
        if len({(r["l1i_bits"], r["btb_bits"]) for r in rows}) != 1:
            raise ValueError(f"storage changes between traces for {name}; mixed/stale binaries")
        points.append(rows)
    if not points:
        raise ValueError("no results found")
    expected = {r["trace"] for r in points[0]}
    for rows in points:
        if {r["trace"] for r in rows} != expected:
            raise ValueError(f"unbalanced trace coverage for {rows[0]['configuration']}; compare identical trace sets")
    summaries = []
    for rows in points:
        first = rows[0]
        ipc = statistics.harmonic_mean(r["ipc"] for r in rows)
        summaries.append({k: v for k, v in first.items() if k not in ("trace", "group", "ipc", "ipc_per_kib")} |
                         dict(trace_count=len(rows), ipc=ipc, ipc_per_kib=ipc / first["total_storage_kib"]))
    order = lambda r: (r["total_storage_kib"], r["design"], r["configuration"], r.get("trace", ""))
    return sorted(summaries, key=order), sorted([r for rows in points for r in rows], key=order)


def write_csv(path, rows):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


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
    for label, color, marker in DESIGNS.values():
        rows = [r for r in summary if r["design"] == label]
        if not rows:
            continue
        for ax, metric in zip(axes, ("ipc", "ipc_per_kib")):
            ax.scatter([r["total_storage_kib"] for r in rows], [r[metric] for r in rows],
                       label=label, c=color, marker=marker, s=48, alpha=0.8, edgecolors="black", linewidths=0.4)
    for ax, ylabel in zip(axes, ("Harmonic mean ROI IPC", "Harmonic mean ROI IPC / KiB")):
        ax.set_xlabel("Total L1I + BTB storage (KiB, including metadata and predictors)")
        ax.set_ylabel(ylabel)
        ax.grid(alpha=0.25)
        ax.legend()
    fig.suptitle(title or f"Instruction front-end design space · {summary[0]['trace_count']} trace(s)")
    for suffix in (".png", ".pdf"):
        fig.savefig(output.with_suffix(suffix), dpi=180)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--configs", type=Path, default=Path(__file__).resolve().parents[1] / "IDUN_CONFIGS/storage_dse")
    parser.add_argument("--output", type=Path, default=Path("storage_dse"))
    parser.add_argument("--title", help="optional figure title")
    parser.add_argument("--allow-incomplete", action="store_true", help="skip configurations with no results (trace coverage must still match)")
    args = parser.parse_args()
    try:
        summary, traces = collect(args.results, args.configs, args.allow_incomplete)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write_csv(args.output.with_suffix(".csv"), summary)
        write_csv(args.output.with_name(args.output.name + "_per_trace.csv"), traces)
        plot(summary, args.output, args.title)
    except (ValueError, OSError, KeyError, ImportError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Wrote {len(summary)} storage-sorted points to {args.output}.[csv,png,pdf]")


if __name__ == "__main__":
    main()
