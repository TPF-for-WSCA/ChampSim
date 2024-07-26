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
    dframes = []
    for i in range(1, 16):
        file_path = os.path.join(path, f"cpu0_partition_{i}_type_count.tsv")
        if not os.path.isfile(file_path):
            continue
        print(f"\tPloting {file_path}")
        try:
            dframes.append(pd.read_csv(file_path, sep="\t", header=None))
            dframes[-1].columns = ["Branch Type", i]
        except:
            print(f"\tERROR: Could not open {file_path}")
            return None
    result = dframes.pop(0)
    for df in dframes:
        result = result.merge(
            df,
            how="outer",
            on=["Branch Type"],
            sort=True,
        )
    result = result.fillna(0)
    result = result.set_index("Branch Type")
    result = result.transpose()
    ax = result.plot.bar(stacked=True)
    ax.legend(loc=2)
    plt.savefig(f"{path}/branch_type_by_offset_category.pdf")
    return result


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
            for group, value in value.items():
                value = value.get()
                continue
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
        continue

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

    return
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
    results_by_application_by_config = defaultdict(
        lambda: defaultdict(lambda: defaultdict(lambda: 0))
    )
    groups = ["google", "ipc1"]
    # TODO: Plot per group or per application or per trace
    # TODO: in any case: first get all of the workloads parsed
    # TODO: for nameless but id-ful workloads, replace ID by name
    for benchmark in os.listdir(args.result_dir):
        if not benchmark.startswith(("google", "ipc1")):
            continue
        benchmark_path = os.path.join(args.result_dir, benchmark)
        group = benchmark.split("_")[0]
        for config in os.listdir(benchmark_path):
            if not config.endswith("offset_btbx"):
                continue
            config_path = os.path.join(benchmark_path, config)
            results_by_application = {}
            for application in os.listdir(config_path):
                app_path = os.path.join(config_path, application)
                result = pool.apply_async(
                    extract_single_workload, args=(app_path,)
                )  # TODO: pass which value should be extracted
                results_by_application_by_config[group][config][application] = result

    print("COMPLETED DATA GATHERING")
    graph_dir = os.path.join(args.result_dir, "graphs")
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
        description="Parse TSV files per workload or per group and generate (stacked) bar charts"
    )
    parser.add_argument("result_dir")
    args = parser.parse_args()
    main(args)
