import argparse
import csv
import math
import matplotlib
import numpy as np
import os
import struct
import sys
import traceback

import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
from argparse import RawTextHelpFormatter
from collections import defaultdict
from collections.abc import MutableMapping
from functools import partial

plt.style.use("tableau-colorblind10")

# Statistics

CHOSEN_STRATEGY = 1

TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY = defaultdict(int)
TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY = defaultdict(
    lambda: defaultdict(lambda: [0, 0])
)
BLOCK_SIZES_HISTOGRAM = defaultdict(lambda: [0 for i in range(64)])
WAY_SIZES_BY_WORKLOAD = defaultdict(list)
BLOCK_STARTS_BY_WORKLOAD = defaultdict(lambda: [0 for i in range(64)])
arm = False


def flatten(d, parent_key="", sep="_"):
    items = []
    for k, v in d.items():
        new_key = f"{parent_key if parent_key else str()}{sep if parent_key else str()}{k if parent_key else k}"
        if isinstance(v, MutableMapping):
            items.extend(flatten(v, new_key, sep=sep).items())
        else:
            items.append((new_key, v))
    return dict(items)


class Strategy:
    name = ""
    split_fn = lambda line: (1)

    def split(self, line):
        if len(line) == 0:
            return 1, [[0, len(line) - 1]]
        return self.split_fn(line)

    def __init__(self, name, split_fn):
        self.name = name
        self.split_fn = split_fn

    def __str__(self) -> str:
        return self.name


""" For all split functions, we know that we only look at the part of the line that containse something.
That said it starts and ends with a True except it was empty on eviction.
"""


def split_n(line, n):
    prev = True
    holes = []
    blocks = []
    current = [0, 0]
    for i, elem in enumerate(line):
        if prev and not elem:
            holes.append(1)
            blocks.append(current)
            current = [0, 0]
        if not prev and elem:
            current = [i, 0]
        if not prev and not elem:
            holes[-1] += 1
        prev = elem
        if elem:
            current[1] = i
    if current[0] <= current[1] and current[1] != 0:
        # we have one left over
        blocks.append(current)
    # we might need to merge blocks
    actual_blocks = []
    splits = []
    for i, hole in enumerate(holes):
        if hole >= n:
            splits.append(hole)
            actual_blocks.append(blocks[i])
        else:
            actual_blocks.append([blocks[i][0], blocks[i + 1][1]])
    if len(splits) == len(actual_blocks):
        # we did split but not add the last block...
        actual_blocks.append(blocks[-1])
    return (len(splits) + 1, actual_blocks)


def split_np(line, n):
    hole_size = max(int(len(line) / (100 / n)), 1)
    return split_n(line, hole_size)


strategies = [
    Strategy("NoSplit", lambda line: (1, [[0, len(line) - 1]])),
    Strategy("SplitOneByteHole", partial(split_n, n=1)),
    Strategy("SplitTwoBytesHole", partial(split_n, n=2)),
    Strategy("SplitFourBytesHole", partial(split_n, n=4)),
]
#    Strategy("Split Four Bytes Hole", partial(split_n, n=4)),
#    Strategy("Split 50% Hole", partial(split_np, n=50)),
#    Strategy("Split 25% Hole", partial(split_np, n=25)),
#    Strategy("Split 12.5% Hole", partial(split_np, n=12.5)),
#    Strategy("Split 6.25% Hole", partial(split_np, n=6.25)),
#    Strategy("Split 3.125% Hole", partial(split_np, n=3.125)),


def trim_mask(mask):
    started = False
    last_bit_pos = 0
    num_zero_bits = 0
    trimmed_mask = []
    for bit in mask:
        if not bit and not started:
            last_bit_pos += 1
            continue
        if bit:
            last_bit_pos += num_zero_bits
            last_bit_pos += 1
            num_zero_bits = 0
            started = True
        elif not bit:
            num_zero_bits += 1
        trimmed_mask.append(bit)
    return (
        trimmed_mask[: -(len(mask) - last_bit_pos)]
        if (len(mask) - last_bit_pos) != 0
        else trimmed_mask,
        len(mask) - len(trimmed_mask),
    )


def int_t_to_boolean_list(line, bits=64):
    mask_array = []
    for i, integer in enumerate(line):
        for bit in range(
            int(bits / len(line)) * i, int(bits / len(line)) * (i + 1)
        ):
            if (integer >> bit) & 1:
                mask_array.append(True)
            else:
                mask_array.append(False)
    mask_array.reverse()
    return mask_array


def count_overlap_alignment(block, offset, strategy):
    global TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY
    if block[0] + offset <= 31 and block[1] + offset > 31:
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][8][1] += 1
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][16][1] += 1
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][32][1] += 1
        return
    if (block[0] + offset) // 16 != (block[1] + offset) // 16:
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][8][1] += 1
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][16][1] += 1
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][32][0] += 1
        return
    if (block[0] + offset) // 8 != (block[1] + offset) // 8:
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][8][1] += 1
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][16][0] += 1
        TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][32][0] += 1
        return
    TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][8][0] += 1
    TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][16][0] += 1
    TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY[strategy][32][0] += 1


def get_mask_from_tracefile(tracefile_path, sample_distance=1, start_offset=0):
    with open(tracefile_path, "rb") as tracefile:
        tracefile.seek(start_offset, 0)
        while True:
            line = tracefile.read(8)
            if not line:
                print(f"Handled {tracefile.tell()/8} cachelines")
                break
            intline = struct.unpack("ii", line)
            array_line = int_t_to_boolean_list(intline)
            if not array_line:
                print(f"Decoding of line {line} failed", file=sys.stderr)
                break
            if sample_distance > 1:
                tracefile.seek(sample_distance - 1 * 8, 1)
            yield array_line


def get_uint64_t_from_tracefile(
    tracefile_path, sample_distance=1, start_offset=0
):
    with open(tracefile_path, "rb") as tracefile:
        tracefile.seek(start_offset, 0)
        while True:
            line = tracefile.read(8)
            if not line:
                break
            uint64_t = struct.unpack("Q", line)
            if sample_distance > 1:
                tracefile.seek(sample_distance - 1 * 8, 1)
            yield uint64_t[0]


def merge_single_mask(first_byte, trimmed_mask, only_chosen=False):
    local_strategies = (
        [strategies[CHOSEN_STRATEGY]] if only_chosen else strategies
    )
    if not strategies[0] in local_strategies:
        local_strategies.append(strategies[0])
    if all(trimmed_mask):
        # no hole case, single block
        # we need to add one to each splitting strategy, as no holes appear there as well
        for strategy in local_strategies:
            TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY[strategy.name] += 1
            BLOCK_SIZES_HISTOGRAM[strategy.name][len(trimmed_mask) - 1] += 1
        return
    for strategy in local_strategies:
        total_blocks, blocks = strategy.split(trimmed_mask)
        TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY[strategy.name] += total_blocks
        for block in blocks:
            count_overlap_alignment(block, first_byte, str(strategy))
            BLOCK_SIZES_HISTOGRAM[strategy.name][block[1] - block[0]] += 1


def create_uniform_buckets_of_size(num_buckets):
    normalised_histogram = [
        b / sum(BLOCK_SIZES_HISTOGRAM[strategies[CHOSEN_STRATEGY].name])
        for b in BLOCK_SIZES_HISTOGRAM[strategies[CHOSEN_STRATEGY].name]
    ]
    target = 1.0 / num_buckets
    bucket_sizes = []
    bucket_percentages = []
    counter = 1
    sumup = 0
    prev_bucket = 0.0
    bucket_idx = 1
    increment = 1
    if arm:
        increment = 4
        counter = 0
    for idx, percentage in enumerate(normalised_histogram):
        if arm and (idx + 1) % 4 != 0:
            assert percentage == 0
            continue
        if percentage > target:
            split = int(percentage / target)
            value = percentage / split
        else:
            value = percentage
        if value == 0:
            if counter < 64:
                counter += increment
            continue
        single_val = value
        inc_counter = True
        while value <= percentage:
            comp_value = sumup + single_val
            diff_with = abs(comp_value - (target * bucket_idx))
            diff_without = abs((target * bucket_idx) - sumup)
            if comp_value > (target * bucket_idx) and diff_with < diff_without:
                if inc_counter and counter < 64:
                    counter += increment
                bucket_percentages.append(comp_value - prev_bucket)
                bucket_sizes.append(counter)
                prev_bucket = comp_value
                sumup += single_val
                bucket_idx += 1
                value += single_val
                inc_counter = False
                continue
            elif (
                comp_value > (target * bucket_idx) and diff_without < diff_with
            ):
                bucket_sizes.append(counter)
                bucket_percentages.append(sumup - prev_bucket)
                prev_bucket = sumup
                bucket_idx += 1
            sumup += single_val
            value += single_val
        if counter < 64:  # dont increase if we already hit the ceiling
            counter += increment
    while len(bucket_sizes) < num_buckets:
        bucket_sizes.append(bucket_sizes[-1])
    # print(f"target bucket size: {target}")
    # print(f"Actual buckets: {bucket_percentages}")
    bucket_percentages.append(1 - sum(bucket_percentages))
    return bucket_sizes, bucket_percentages


def apply_way_analysis(
    workload_name, tracefile_path, num_buffer_entries=64, num_sets=64
):
    # count = 0
    for mask in get_mask_from_tracefile(tracefile_path):
        trimmed_mask, first_byte = trim_mask(mask)
        merge_single_mask(first_byte, trimmed_mask, only_chosen=True)
        # count += 1
        # if count > 100000:
        #     break  # debug solution

    # 64 b (arm: 16) per entry + 6b counter per entry (only for non-directmapped required) + 128B merge registers overall (merge registers might be optimised away)
    if not arm:
        buffer_bytes_per_set = (
            num_buffer_entries + num_buffer_entries * 8 + 128
        ) / num_sets
    else:
        buffer_bytes_per_set = (
            num_buffer_entries + num_buffer_entries * 2 + 128
        ) / num_sets
    target_size = 512 - buffer_bytes_per_set
    error = target_size
    selected_waysizes = []
    local_overhead = 0
    overhead = 0
    for i in range(8, 64):
        if arm:
            overhead = math.ceil((4 * i) / 8)  # 6 bits per tag
        else:
            overhead = math.ceil((6 * i) / 8)  # 6 bits per tag
        local_target_size = target_size - overhead
        bucket_sizes, _ = create_uniform_buckets_of_size(i)
        total_size = sum(bucket_sizes)
        # if total_size > local_target_size:
        #     break
        if abs(local_target_size - sum(bucket_sizes)) < error:
            error = abs(local_target_size - sum(bucket_sizes))
            selected_waysizes = bucket_sizes
            local_overhead = overhead
    print(
        f"Optimal waysizes with error: {error}, overall size: {sum(selected_waysizes) + buffer_bytes_per_set + local_overhead}"
    )
    print(selected_waysizes)
    WAY_SIZES_BY_WORKLOAD[workload_name] = selected_waysizes


def apply_splits_for_workload(workload_name, tracefile_path):
    for array_line in get_mask_from_tracefile(tracefile_path):
        trimmed_mask, first_byte = trim_mask(array_line)
        merge_single_mask(first_byte, trimmed_mask)


def apply_offset_bucket_analysis(worklad_name, tracefile_path):
    for mask in get_mask_from_tracefile(tracefile_path):
        trimmed_mask, first_byte = trim_mask(mask)

        print(first_byte)
        BLOCK_STARTS_BY_WORKLOAD[worklad_name][first_byte] += 1
        prev_bit = True
        for bit in trimmed_mask:
            if bit and not prev_bit:
                print(first_byte)
                BLOCK_STARTS_BY_WORKLOAD[worklad_name][first_byte] += 1
            first_byte += 1
            prev_bit = bit


def apply_storage_efficiency_analysis(
    workload_name, tracedirectory_path, vcl_config=None
):
    # assuming 32k cache with S = 64
    tracefile_path = os.path.join(
        tracedirectory_path, "cpu0_L1I_c_bytes_used.bin"
    )
    plt.rcParams.update({"font.size": 7})

    count = 0
    max_num_blocks = 512
    cacheline_size = 64
    useful_insertions = []
    useful_bytes = 0
    total_cache_size = max_num_blocks * cacheline_size
    if vcl_config:
        total_cache_size = sum(vcl_config) * 64
        max_num_blocks = len(vcl_config) * 64
    storage_efficiency_timeseries = []
    max_efficiency = 0.0
    min_efficiency = 1.0
    for useful_bytes in get_uint64_t_from_tracefile(tracefile_path):
        assert useful_bytes <= total_cache_size

        if useful_bytes == 0:
            print("all useless...")
        efficiency = float(useful_bytes) / float(total_cache_size)
        if efficiency > max_efficiency:
            max_efficiency = efficiency
        if efficiency < min_efficiency:
            min_efficiency = efficiency

        storage_efficiency_timeseries.append(efficiency)
        if len(storage_efficiency_timeseries) > 1000:  # debug mode
            break
    if len(storage_efficiency_timeseries) == 0:
        print(
            f"WARNING: STORAGE EFFICIENCY TIME SERIES IS EMPTY AFTE RCONSUMING ENTIRE FILE @{workload_name}/@{vcl_config}",
            file=sys.stderr,
        )
        return
    average_storage_efficiency = sum(storage_efficiency_timeseries) / len(
        storage_efficiency_timeseries
    )

    cm = 1 / 2.54
    fig, ax1 = plt.subplots()
    fig.set_size_inches(18 * cm, 8 * cm)

    ax1.set_title(f"{workload_name.split('.')[0]}")
    ax1.yaxis.set_major_formatter(mtick.PercentFormatter(1.0))
    ax1.set_ylabel("Storage Efficiency")
    fig.subplots_adjust(bottom=0.2)
    ax1.plot(
        range(1, len(storage_efficiency_timeseries) + 1),
        storage_efficiency_timeseries,
    )

    ax1.axhline(y=average_storage_efficiency, color="#C85200")

    x = -35
    label = f"{(average_storage_efficiency*100):3.2f}%"
    ax1.text(
        x,
        average_storage_efficiency,
        label,
        fontsize=5,
        va="bottom",
        ha="left",
        fontstyle="italic",
        color="#C85200",
    )
    ax1.axhline(y=max_efficiency, color="#898989")
    label = f"{(max_efficiency*100):3.2f}%"
    ax1.text(
        x,
        max_efficiency,
        label,
        fontsize=5,
        va="bottom",
        ha="left",
        fontstyle="italic",
        color="#898989",
    )
    ax1.axhline(y=min_efficiency, color="#FFBC79")
    label = f"{(min_efficiency*100):3.2f}%"
    ax1.text(
        x,
        min_efficiency,
        label,
        fontsize=5,
        va="bottom",
        ha="left",
        fontstyle="italic",
        color="#FFBC79",
    )

    # plt.savefig(
    #     os.path.join(
    #         tracedirectory_path, f"{workload_name}_storage_efficiency.pgf"
    #     )
    # )
    tracedirectory_path = tracedirectory_path.rsplit("/", 1)[0]
    os.makedirs(os.path.join(tracedirectory_path, "graphs"), exist_ok=True)

    plt.savefig(
        os.path.join(
            tracedirectory_path,
            "graphs",
            f"{workload_name}_storage_efficiency.pdf",
        )
    )
    plt.close()
    return storage_efficiency_timeseries


def print_starting_offsets(trace_directory, workload):
    result_file_path = os.path.join(
        trace_directory, workload, "cpu0_L1I_cl_block_starts.tsv"
    )
    with open(
        result_file_path, "w", encoding="utf-8", newline=""
    ) as result_file:
        writer = csv.DictWriter(
            result_file,
            BLOCK_STARTS_BY_WORKLOAD.keys(),
            dialect="excel-tab",
        )
        writer.writeheader()
        writer.writerow(BLOCK_STARTS_BY_WORKLOAD)


def main(args):
    trace_directory = args.trace_dir
    global TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY, arm, CHOSEN_STRATEGY
    CHOSEN_STRATEGY = args.strategy
    if args.architecture == "arm":
        arm = True
    data_per_workload = {}
    for workload in os.listdir(trace_directory):
        global BLOCK_SIZES_HISTOGRAM  # We need t reset it for every workload
        # BLOCK_SIZES_HISTOGRAM = defaultdict(lambda: [0 for i in range(64)]) # we do not reset it so we have in the end the average of all workloads
        if not os.path.isdir(os.path.join(trace_directory, workload)):
            continue
        if (
            workload.endswith(".txt")
            or workload == "graphs"
            or workload == "raw_data"
        ):
            continue
        print(f"Handling {workload}...")
        try:
            if args.action == "merge_strategy":
                apply_splits_for_workload(
                    workload,
                    os.path.join(
                        trace_directory,
                        workload,
                        "cpu0_L1I_cl_access_masks.bin",
                    ),
                )
            elif args.action == "optimal_way":
                apply_way_analysis(
                    workload,
                    os.path.join(
                        trace_directory,
                        workload,
                        "cpu0_L1I_cl_access_masks.bin",  # TODO: Switch to precalculated relative
                    ),
                )
            elif args.action == "starting_offsets":
                apply_offset_bucket_analysis(
                    workload,
                    os.path.join(
                        trace_directory,
                        workload,
                        "cpu0_L1I_cl_access_masks.bin",
                    ),
                )
                print_starting_offsets(trace_directory, workload)
                continue
            elif args.action == "storage_efficiency":
                results = apply_storage_efficiency_analysis(
                    workload,
                    os.path.join(trace_directory, workload),
                    args.vcl_config,
                )
                if not results:
                    continue
                label = workload.split(".")[0]
                if not label:
                    print("label is none")
                    continue
                data_per_workload[label] = results
                continue
            else:
                exit(-1)

            # print out results
            print(TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY)

            result_file_path = os.path.join(
                trace_directory, workload, "cpu0_L1I_cl_splits_overhead.tsv"
            )
            with open(
                result_file_path, "w", encoding="utf-8", newline=""
            ) as result_file:
                writer = csv.DictWriter(
                    result_file,
                    TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY.keys(),
                    dialect="excel-tab",
                )
                writer.writeheader()
                writer.writerow(TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY)
                total_lines = TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY["NoSplit"]
                percentage_overhead = {}
                for key, value in TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY.items():
                    percentage_overhead[key] = (value / total_lines) - 1.0
                writer.writerow(percentage_overhead)

            result_file_path = os.path.join(
                trace_directory, workload, "cl_splits_crossings.tsv"
            )

            with open(
                result_file_path, "w", encoding="utf-8", newline=""
            ) as result_file:
                flattened_data = flatten(
                    TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY, "", " @ "
                )
                writer = csv.DictWriter(
                    result_file,
                    flattened_data.keys(),
                    dialect="excel-tab",
                )
                writer.writeheader()
                writer.writerow(flattened_data)
                percentage_overhead = {}
                for (
                    key,
                    value,
                ) in flattened_data.items():
                    percentage_overhead[key] = value[1] / (value[0] + value[1])
                writer.writerow(percentage_overhead)

            # reset counters
            TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY = defaultdict(int)
        except Exception as ex:
            print(f"Unknown exception occured {ex}, {traceback.print_exc()}")
            continue  # Ignore this workload / log written to stderr

    def set_axis_style(ax, labels):
        ax.set_xticks(
            np.arange(1, len(labels) + 1), labels=labels, rotation=90
        )
        ax.set_xlim(0.25, len(labels) + 0.75)

    if args.action == "storage_efficiency":
        data_per_workload = dict(sorted(data_per_workload.items()))
        groups = set(
            [label.split("_")[0] for label in data_per_workload.keys()]
        )
        from mpl_toolkits.axes_grid1 import make_axes_locatable

        cm = 1 / 2.54
        fig = plt.figure(figsize=(18 * cm, 8 * cm))
        fig.subplots_adjust(bottom=0.39)
        ax1 = fig.add_subplot(1, 1, 1)
        ax1.set_ylabel("Storage Efficiency")
        ax1.yaxis.set_major_formatter(mtick.PercentFormatter(1.0))
        divider = make_axes_locatable(ax1)
        graphs_dir = os.path.join(trace_directory, "graphs")
        os.makedirs(os.path.join(trace_directory, "graphs"), exist_ok=True)
        ax2 = divider.append_axes("right", size="679%", pad=0.05)
        ax2.yaxis.set_tick_params(labelleft=False)
        fig.add_axes(ax2)
        ax3 = divider.append_axes("right", size="222%", pad=0.05)
        ax3.yaxis.set_tick_params(labelleft=False)
        fig.add_axes(ax3)
        axes = [ax1, ax2, ax3]

        for idx, group in enumerate(sorted(groups)):
            avg = []
            data_per_benchmark = {}
            for key, value in data_per_workload.items():
                if key.startswith(group):
                    data_per_benchmark[key] = value
                    avg.append(sum(value) / len(value))
            data_per_benchmark[f"{group.upper()} AVG"] = avg

            axes[idx].violinplot(
                data_per_benchmark.values(), showmeans=True, widths=0.9
            )
            set_axis_style(axes[idx], data_per_benchmark.keys())

        plt.savefig(
            os.path.join(
                graphs_dir,
                f"combined_storage_efficiency.pdf",
            )
        )
        plt.close()
        exit(0)
    way_file_path = result_file_path = os.path.join(
        trace_directory, f"{strategies[CHOSEN_STRATEGY]}_way_sizes.tsv"
    )
    with open(way_file_path, "w", encoding="utf-8", newline="") as wayfile:
        writer = csv.writer(wayfile, dialect="excel-tab")
        for workload, waysizes in WAY_SIZES_BY_WORKLOAD.items():
            entry = [workload, *waysizes]
            writer.writerow(entry)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse cacheline access masks and calculate number of cache"
        " blocks under given strategies",
        formatter_class=RawTextHelpFormatter,
    )
    parser.add_argument("trace_dir", type=str)
    parser.add_argument(
        "action",
        type=str,
        default="merge_strategy",
        choices=[
            "merge_strategy",
            "optimal_way",
            "starting_offsets",
            "storage_efficiency",
        ],
    )

    parser.add_argument(
        "architecture", type=str, default="x86", choices=["x86", "arm"]
    )
    strategies_list = "\n".join(
        [f"\t{i}:\t{strategy}" for i, strategy in enumerate(strategies)]
    )
    parser.add_argument(
        "--strategy",
        type=int,
        default=1,
        help=f"Available strategies:\n\tIdx:\tStrategy Name:\n{strategies_list}",
    )

    parser.add_argument(
        "--vcl-configuration", dest="vcl_config", type=int, nargs="*"
    )

    args = parser.parse_args()
    main(args)
