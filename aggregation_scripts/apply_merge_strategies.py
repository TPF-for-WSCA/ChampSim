import argparse
import csv
import os
import struct
import sys

from collections import defaultdict, deque
from functools import partial

# Statistics

TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY = defaultdict(int)


class Strategy:

    name = ""
    split_fn = lambda line: (1)

    def split(self, line):
        if len(line) == 0:
            return 1, [line]
        return self.split_fn(line)

    def __init__(self, name, split_fn):
        self.name = name
        self.split_fn = split_fn


""" For all split functions, we know that we only look at the part of the line that containse something.
That said it starts and ends with a True except it was empty on eviction.
"""


def split_n(line, n):
    prev = True
    holes = []
    blocks = []
    for elem in line:
        if prev and not elem:
            holes.append(1)
        if not prev and not elem:
            holes[-1] += 1
        prev = elem
    splits = [hole for hole in holes if hole >= n]
    return len(splits) + 1


def split_np(line, n):
    hole_size = max(int(len(line) / (100 / n)), 1)
    return split_n(line, hole_size)


strategies = [
    Strategy("no split", lambda line: (1, [line])),
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
        if bit == 0 and not started:
            last_bit_pos += 1
            continue
        if bit == 1:
            last_bit_pos += num_zero_bits
            last_bit_pos += 1
            num_zero_bits = 0
            started = True
        elif bit == 0:
            num_zero_bits += 1
        trimmed_mask.append(bit)
    return trimmed_mask[: -(len(mask) - last_bit_pos)]


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


def apply_splits_for_workload(workload_name, tracefile_path):
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
            trimmed_mask = trim_mask(array_line)
            if all(trimmed_mask):
                # no hole case, single block
                # we need to add one to each splitting strategy, as no holes appear there as well
                for strategy in strategies:
                    TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY[strategy.name] += 1
                continue
            for strategy in strategies:
                total_blocks = strategy.split(trimmed_mask)
                TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY[
                    strategy.name
                ] += total_blocks


def main(args):
    trace_directory = args.trace_dir
    global TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY
    for workload in os.listdir(trace_directory):
        if workload.endswith(".txt") or workload == "graphs":
            continue
        print(f"Handling {workload}...")
        apply_splits_for_workload(
            workload,
            os.path.join(trace_directory, workload, "cl_access_masks.bin"),
        )

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

        # reset counters
        TOTAL_LINES_AFTER_SPLIT_BY_STRATEGY = defaultdict(int)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse cacheline access masks and calculate number of cache"
        " blocks under given strategies"
    )
    parser.add_argument("trace_dir", type=str)
    args = parser.parse_args()
    main(args)
