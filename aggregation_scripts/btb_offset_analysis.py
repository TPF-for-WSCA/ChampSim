"""
Plot offset BTBs fill level over time and the amount of sharing.
"""

import argparse
import os

from collections import defaultdict

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import multiprocessing as mp

plt.style.use("tableau-colorblind10")
plt.rcParams.update({"font.size": 7})

# TODO: move configuration flags up to the beginning of the file


def dd():
    return defaultdict(int)


def extract_single_workload(path):

    result_per_offset = []
    summary_results = defaultdict(dd)
    for i in range(2):
        file_path = os.path.join(path, f"cpu0_offset_btb_{i}_sharing_over_time.json")
        print(f"\tPloting {file_path}")
        try:
            offset_information = pd.read_json(file_path)
        except:
            print(f"\tERROR: Could not open {file_path}")
            return None
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
                summary_results[i][entry["ref_count"]] += 1
                offset_entries += entry["frequency"]
                if max_shared_offset < entry["ref_count"]:
                    max_shared_offset = entry["ref_count"]
            num_offset_entries_by_time[j] = offset_entries
            num_idx_entries_by_time[j] = idx_entries
            compression_factor_by_time[j] = (
                0 if offset_entries == 0 else idx_entries / offset_entries
            )
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
        plt.close()
        result_per_offset.append(
            (
                num_offset_entries_by_time,
                num_idx_entries_by_time,
                max_share_by_time,
                compression_factor_by_time,
            )
        )
    return result_per_offset, summary_results


result_names = ["num_offsets", "num_idx", "max_shared", "compression_factor"]


def plot_config(results_by_application, graph_dir, filename, offset_idx, rv_idx):
    from mpl_toolkits.axes_grid1 import make_axes_locatable
    import matplotlib.ticker as mtick

    def set_axis_style(ax, labels):
        ax.set_xticks(np.arange(1, len(labels) + 1), labels=labels, rotation=90)
        ax.set_xlim(0.25, len(labels) + 0.75)

    data_per_workload = dict(sorted(results_by_application.items()))
    groups = set([label.split("_")[0] for label in data_per_workload.keys()])

    cm = 1 / 2.54
    violin_fig = plt.figure(figsize=(18 * cm, 8 * cm))
    stacked_fig = plt.figure(figsize=(18 * cm, 8 * cm))
    violin_fig.subplots_adjust(bottom=0.39)
    stacked_fig.subplots_adjust(left=0.06, bottom=0.39, right=0.65)
    violin_ax1 = violin_fig.add_subplot(1, 1, 1)
    stacked_ax1 = stacked_fig.add_subplot(1, 1, 1)
    violin_divider = make_axes_locatable(violin_ax1)
    stacked_divider = make_axes_locatable(stacked_ax1)

    stacked_ax1.set_ylabel("Relative Frequency of Reference Counts")
    violin_ax1.set_ylabel("Compression Factor")
    violin_ax2 = violin_divider.append_axes("right", size="400%", pad=0.05)
    violin_ax2.sharey(violin_ax1)
    violin_ax2.yaxis.set_tick_params(labelleft=False)
    violin_fig.add_axes(violin_ax2)
    violin_ax3 = violin_divider.append_axes("right", size="88%", pad=0.05)
    violin_ax3.sharey(violin_ax1)
    violin_ax3.yaxis.set_tick_params(labelleft=False)
    violin_fig.add_axes(violin_ax3)

    stacked_ax2 = stacked_divider.append_axes("right", size="400%", pad=0.05)
    stacked_ax2.sharey(stacked_ax1)
    stacked_ax2.yaxis.set_tick_params(labelleft=False)
    stacked_fig.add_axes(stacked_ax2)
    stacked_ax3 = stacked_divider.append_axes("right", size="88%", pad=0.05)
    stacked_ax3.sharey(stacked_ax1)
    stacked_ax3.yaxis.set_tick_params(labelleft=False)
    stacked_fig.add_axes(stacked_ax3)

    violin_axes = [violin_ax1, violin_ax2, violin_ax3]
    stacked_axed = [stacked_ax1, stacked_ax2, stacked_ax3]

    max_limit = 1.0
    for idx, group in enumerate(sorted(groups)):
        avg = []
        data_per_benchmark = {}
        summary_per_benchmark = {}
        benchmarks = []
        ref_counts = set()
        for key, value in data_per_workload.items():
            value = value.get()
            if not value:
                continue
            value, summary = value
            if key.startswith(group):
                value = value[offset_idx][rv_idx]
                avg.append(sum(value) / len(value))
                data_per_benchmark[key] = value
                key = key.split(".", 1)[0]
                benchmarks.append(key)
                if idx == 1:
                    max_limit = max(max_limit, max(value))
                summary_per_benchmark[key] = {
                    sum_key: (val / sum(summary[offset_idx].values()))
                    for sum_key, val in summary[offset_idx].items()
                }

                ref_counts.update(summary_per_benchmark[key].keys())
                # assert sum(summary_per_benchmark[key].values()) == 1.0 # Floating point precision sometimes isn't good enough

        bottom = np.zeros(len(benchmarks))
        for ref_count in sorted(list(ref_counts)):
            frequency_list = []
            for offsets in summary_per_benchmark.values():
                frequency_list.append(offsets.get(ref_count, 0.0))
            stacked_axed[idx].bar(
                benchmarks,
                frequency_list,
                label=f"{ref_count}",
                bottom=bottom,
                align="center",
            )
            bottom += np.array(frequency_list)

        stacked_axed[idx].yaxis.set_major_formatter(
            mtick.PercentFormatter(xmax=1.0, decimals=0)
        )
        labels = stacked_axed[idx].get_xticklabels()
        for l in labels:
            l.set_rotation(90)

        labels = stacked_axed[idx].get_yticklabels()
        for l in labels:
            l.set_rotation(90)
            l.set_verticalalignment("center")

        violin_axes[idx].violinplot(
            data_per_benchmark.values(), showmeans=True, widths=0.9
        )
        violin_axes[idx].bar(len(benchmarks) + 1, sum(avg) / len(avg), width=0.8)
        benchmarks.append(f"{group.upper()} AVG")
        set_axis_style(violin_axes[idx], benchmarks)

    stacked_axed[-1].legend(
        title="", bbox_to_anchor=(0.95, 1), loc="upper left", ncol=4
    )

    violin_ax1.set_ylim(bottom=1.0, top=max_limit)
    violin_ax2.set_ylim(bottom=1.0, top=max_limit)
    violin_ax3.set_ylim(bottom=1.0, top=max_limit)
    violin_fig.savefig(os.path.join(graph_dir, f"{filename}_{offset_idx}.pdf"))
    stacked_fig.savefig(
        os.path.join(graph_dir, f"{filename}_{offset_idx}_rel_offset_freq.pdf")
    )
    plt.close(violin_fig)
    plt.close(stacked_fig)


def main(args):
    pool = mp.Pool(processes=7)
    results_by_application_by_config = defaultdict(lambda: defaultdict(lambda: 0))
    for benchmark in os.listdir(args.logdir):
        if benchmark not in [
            "ipc1_server",
            "ipc1_client",
            "ipc1_spec",
            "google_whiskey",
            "google_delta",
            "google_merced",
            "google_charlie",
        ]:
            continue
        benchmark_path = os.path.join(args.logdir, benchmark)
        for config in os.listdir(benchmark_path):
            if not config.endswith("_btb"):
                continue
            config_path = os.path.join(benchmark_path, config)
            results_by_application = {}
            for application in os.listdir(config_path):
                app_path = os.path.join(config_path, application)
                # result = pool.apply_async(extract_single_workload, args=(app_path,))
                result = extract_single_workload(app_path)
                results_by_application_by_config[config][application] = result

    print("COMPLETED DATA GATHERING")
    graph_dir = os.path.join(args.logdir, "graphs")
    os.makedirs(graph_dir, exist_ok=True)
    for rv_idx, name in enumerate(result_names):
        for offset_btb_idx in range(2):
            for (
                config,
                results,
            ) in results_by_application_by_config.items():
                plot_config(
                    results, graph_dir, f"{config}_{name}", offset_btb_idx, rv_idx
                )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse offset json files and analyse it per offset btb"
    )
    parser.add_argument("logdir")
    args = parser.parse_args()
    main(args)
