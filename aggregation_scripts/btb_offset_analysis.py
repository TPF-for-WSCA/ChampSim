"""
Plot offset BTBs fill level over time and the amount of sharing.
"""

import argparse
import os

from collections import defaultdict

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

plt.style.use("tableau-colorblind10")


def extract_single_workload(path):
    result_per_offset = []
    for i in range(4):
        file_path = os.path.join(path, f"cpu0_offset_btb_{i}_sharing_over_time.json")
        print(f"\tPloting {file_path}")
        offset_information = pd.read_json(file_path)
        num_offset_entries_by_time = {}
        num_idx_entries_by_time = {}
        max_share_by_time = {}
        compression_factor_by_time = {}
        j = 0
        for single_record in offset_information.ref_frequencies:
            offset_entries = 0
            idx_entries = 0
            max_shared_offset = 0
            for entry in single_record:
                if entry["ref_count"] == 0:
                    continue
                idx_entries += entry["ref_count"] * entry["frequency"]
                offset_entries += entry["frequency"]
                if max_shared_offset < entry["ref_count"]:
                    max_shared_offset = entry["ref_count"]
            num_offset_entries_by_time[j] = offset_entries
            num_idx_entries_by_time[j] = idx_entries
            compression_factor_by_time[j] = idx_entries / offset_entries
            max_share_by_time[j] = max_shared_offset
            j += 1
        num_offset_entries_by_time = np.array(list(num_offset_entries_by_time.items()))[
            :, 1
        ]
        num_idx_entries_by_time = np.array(list(num_idx_entries_by_time.items()))[:, 1]
        max_share_by_time = np.array(list(max_share_by_time.items()))[:, 1]
        compression_factor_by_time = np.array(list(compression_factor_by_time.items()))[
            :, 1
        ]
        plt.figure(figsize=(10, 20))
        plt.subplot(212)
        plt.plot(max_share_by_time, label="Max Shared Offset")
        plt.legend(loc="upper right")

        plt.subplot(211)
        plt.plot(num_idx_entries_by_time, "r--", label="Index BTB entries")
        plt.plot(num_offset_entries_by_time, "b-", label="Offset BTB entries")
        plt.legend(loc="lower right")
        ax2 = plt.gca().twinx()
        ax2.plot(compression_factor_by_time, "g-", label="Compression Factor")
        ax2.legend(loc="upper right")
        benchmark_name = os.path.basename(os.path.normpath(path))
        plt.savefig(os.path.join(path, f"{benchmark_name}_btb_{i}_offset_stats.pdf"))
        result_per_offset.append(
            (
                num_offset_entries_by_time,
                num_idx_entries_by_time,
                max_share_by_time,
                compression_factor_by_time,
            )
        )
    return result_per_offset


def plot_config(results_by_application_by_benchmark, graph_dir, filename):
    from mpl_toolkits.axes_grid1 import make_axes_locatable

    cm = 1 / 2.54
    fig = plt.figure(figsize=(18 * cm, 8 * cm))
    fig.subplots_adjust(bottom=0.39)
    ax1 = fig.add_subplot(1, 1, 1)
    divider = make_axes_locatable(ax1)

    ax1.set_ylabel("Compression Factor")
    ax2 = divider.append_axes("right", size="400%", pad=0.05)
    ax2.sharey(ax1)
    ax2.yaxis.set_tick_params(labelleft=False)
    fig.add_axes(ax2)
    ax3 = divider.append_axes("right", size="88%", pad=0.05)
    ax3.sharey(ax1)
    ax3.yaxis.set_tick_params(labelleft=False)
    fig.add_axes(ax3)

    axes = [ax1, ax2, ax3]


def main(args):
    results_by_application_by_benchmark_by_config = defaultdict(
        lambda: defaultdict(lambda: defaultdict(lambda: 0))
    )
    for benchmark in os.listdir(args.logdir):
        if benchmark not in ["ipc1_server", "ipc1_client", "ipc1_spec"]:
            continue
        benchmark_name = benchmark.split("_")[1]
        benchmark_path = os.path.join(args.logdir, benchmark)
        for config in os.listdir(benchmark_path):
            if not config.endswith("offset_btb"):
                continue
            config_path = os.path.join(benchmark_path, config)
            results_by_application = {}
            for application in os.listdir(config_path):
                app_path = os.path.join(config_path, application)
                result = extract_single_workload(app_path)
                results_by_application_by_benchmark_by_config[config][benchmark_name][
                    application
                ] = result[3]

    graph_dir = os.path.join(args.logdir, "graphs")
    os.makedirs(graph_dir, exist_ok=True)
    for config, results in results_by_application_by_benchmark_by_config.items():
        pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse offset json files and analyse it per offset btb"
    )
    parser.add_argument("logdir")
    args = parser.parse_args()
    main(args)
