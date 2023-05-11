import argparse
import csv
import os
import struct
import sys

from collections import defaultdict
from collections.abc import MutableMapping
from functools import partial

# Statistics

TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY = defaultdict(int)
TOTAL_LINES_CROSSING_BY_BOUNDARY_BY_STRATEGY = defaultdict(
    lambda: defaultdict(lambda: [0, 0])
)
BLOCK_SIZES_HISTOGRAM = defaultdict(lambda: [0 for i in range(64)])
WAY_SIZES_BY_WORKLOAD = defaultdict(list)


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
    Strategy("no split", lambda line: (1, [[0, len(line) - 1]])),
    Strategy("Split One Byte Hole", partial(split_n, n=1)),
    Strategy("Split Two Bytes Hole", partial(split_n, n=2)),
    Strategy("Split Four Bytes Hole", partial(split_n, n=4)),
    Strategy("Split 50% Hole", partial(split_np, n=50)),
    Strategy("Split 25% Hole", partial(split_np, n=25)),
    Strategy("Split 12.5% Hole", partial(split_np, n=12.5)),
    Strategy("Split 6.25% Hole", partial(split_np, n=6.25)),
    Strategy("Split 3.125% Hole", partial(split_np, n=3.125)),
]


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


def get_mask_from_tracefile(tracefile_path):
    with open(tracefile_path, "rb") as tracefile:
        while True:
            line = tracefile.read(8)
            if not line:
                break
            intline = struct.unpack("ii", line)
            array_line = int_t_to_boolean_list(intline)
            if not array_line:
                print(f"Decoding of line {line} failed", file=sys.stderr)
                break
            yield array_line


def merge_single_mask(first_byte, trimmed_mask):
    if all(trimmed_mask):
        # no hole case, single block
        # we need to add one to each splitting strategy, as no holes appear there as well
        for strategy in strategies:
            TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY[strategy.name] += 1
            BLOCK_SIZES_HISTOGRAM[strategy.name][len(trimmed_mask) - 1] += 1
        return
    for strategy in strategies:
        total_blocks, blocks = strategy.split(trimmed_mask)
        TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY[strategy.name] += total_blocks
        for block in blocks:
            count_overlap_alignment(block, first_byte, str(strategy))
            BLOCK_SIZES_HISTOGRAM[strategy.name][block[1] - block[0]] += 1


def create_uniform_buckets_of_size(num_buckets):
    normalised_histogram = [
        b / sum(BLOCK_SIZES_HISTOGRAM["no split"])
        for b in BLOCK_SIZES_HISTOGRAM["no split"]
    ]
    target = 1.0 / num_buckets
    bucket_sizes = []
    bucket_percentages = []
    counter = 1
    sumup = 0
    prev_bucket = 0.0
    bucket_idx = 1
    for percentage in normalised_histogram:
        if percentage > target:
            split = int(percentage / target)
            value = percentage / split
        else:
            value = percentage
        if value == 0:
            counter += 1
            continue
        single_val = value
        counter_increment = 0
        if counter < 64:  # dont increase if we already hit the ceiling
            counter_increment = 1
        while value <= percentage:
            comp_value = sumup + single_val
            diff_with = abs(comp_value - (target * bucket_idx))
            diff_without = abs((target * bucket_idx) - sumup)
            if comp_value > (target * bucket_idx) and diff_with < diff_without:
                if counter < 64:  # dont increase if we already hit the ceiling
                    counter += counter_increment
                bucket_percentages.append(comp_value - prev_bucket)
                bucket_sizes.append(counter)
                prev_bucket = comp_value
                sumup += single_val
                counter_increment = 0
                bucket_idx += 1
                value += single_val
                continue
            elif (
                comp_value > (target * bucket_idx) and diff_without < diff_with
            ):
                bucket_sizes.append(counter)
                bucket_percentages.append(sumup - prev_bucket)
                prev_bucket = sumup
                bucket_idx += 1
            if counter < 64:  # dont increase if we already hit the ceiling
                counter += counter_increment
            counter_increment = 0
            sumup += single_val
            value += single_val
    while len(bucket_sizes) < num_buckets:
        bucket_sizes.append(bucket_sizes[-1])
    # print(f"target bucket size: {target}")
    # print(f"Actual buckets: {bucket_percentages}")
    return bucket_sizes, bucket_percentages


def apply_way_analysis(workload_name, tracefile_path):
    for mask in get_mask_from_tracefile(tracefile_path):
        trimmed_mask, first_byte = trim_mask(mask)
        merge_single_mask(first_byte, trimmed_mask)

    target_size = 512
    error = 512
    selected_waysizes = []
    for i in range(8, 32):
        bucket_sizes, _ = create_uniform_buckets_of_size(i)
        if abs(target_size - sum(bucket_sizes)) < error:
            error = abs(target_size - sum(bucket_sizes))
            selected_waysizes = bucket_sizes
    print(f"Optimal waysizes with error: {error}")
    print(selected_waysizes)
    WAY_SIZES_BY_WORKLOAD[workload_name] = selected_waysizes


def apply_splits_for_workload(workload_name, tracefile_path):
    for array_line in get_mask_from_tracefile(tracefile_path):
        trimmed_mask, first_byte = trim_mask(array_line)
        merge_single_mask(first_byte, trimmed_mask)


def main(args):
    trace_directory = args.trace_dir
    global TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY
    for workload in os.listdir(trace_directory):
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
                        trace_directory, workload, "cl_access_masks.bin"
                    ),
                )
            elif args.action == "optimal_way":
                apply_way_analysis(
                    workload,
                    os.path.join(
                        trace_directory, workload, "cl_access_masks.bin"
                    ),
                )
            else:
                exit(-1)

            # print out results
            print(TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY)

            result_file_path = os.path.join(
                trace_directory, workload, "cl_splits_overhead.tsv"
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
                total_lines = TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY["no split"]
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
            print(f"Unknown exception occured {ex}")
            continue  # Ignore this workload / log written to stderr

    way_file_path = result_file_path = os.path.join(
        trace_directory, "way_sizes.tsv"
    )
    with open(way_file_path, "w", encoding="utf-8", newline="") as wayfile:
        writer = csv.writer(wayfile, dialect="excel-tab")
        for workload, waysizes in WAY_SIZES_BY_WORKLOAD.items():
            entry = [workload, *waysizes]
            writer.writerow(entry)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse cacheline access masks and calculate number of cache"
        " blocks under given strategies"
    )
    parser.add_argument("trace_dir", type=str)
    parser.add_argument(
        "action",
        type=str,
        default="merge_strategy",
        choices=["merge_strategy", "optimal_way"],
    )
    args = parser.parse_args()
    main(args)
