#!/usr/bin/python
import argparse
import csv
import math
import os
import re
import sys
from collections import defaultdict
from enum import Enum

value_set_per_bit_size = defaultdict(set)
count_per_offset = defaultdict(int)
ways_per_set_per_idx_size_count = defaultdict(
    lambda: defaultdict(set)
)  # cardinality of the set is the number of ways

value_count_per_addr_bit = defaultdict(lambda: defaultdict(int))
total_lines_read = 0


def write_result_files(out_dir: str):
    with open(
        os.path.join(out_dir, "ordered_offset_counts.tsv"), "w+", encoding="utf-8"
    ) as ordered_offset_file:
        writer = csv.writer(ordered_offset_file)
        writer.writerow(["Offset Size", "Offset Counts"])
        for offset_size, offsets in value_set_per_bit_size.items():
            row = [offset_size]
            offset_counts = sorted(
                [count_per_offset[offset] for offset in offsets], reverse=True
            )
            row.extend(offset_counts)
            writer.writerow(row)
    with open(
        os.path.join(out_dir, "ordered_addr_offset_file.tsv"), "w+", encoding="utf-8"
    ) as ordered_addr_offset_file:
        writer = csv.writer(ordered_addr_offset_file)
        writer.writerow(["Final Addr Bits", "Offset Counts"])
        for addr_bits, offsets in value_count_per_addr_bit.items():
            row = [f"{addr_bits:b}"]
            offset_counts = sorted(offsets.values(), reverse=True)
            row.extend(offset_counts)
            writer.writerow(row)
    with open(
        os.path.join(out_dir, "num_ways_based_on_index.tsv"), "w+", encoding="utf-8"
    ) as ordered_addr_offset_file:
        writer = csv.writer(ordered_addr_offset_file)
        writer.writerow(["Num Sets", "Max Num Ways"])
        for num_sets, max_ways in ways_per_set_per_idx_size_count.items():
            num_ways = max([s.size() for s in max_ways.values()])
            row = [f"{num_sets}", f"{num_ways}"]
            writer.writerow(row)


def read_single_tsv(path: str):
    global total_lines_read
    with open(
        os.path.join(path, "cpu0_pc_offset_mapping.tsv"),
        "r",
        encoding="utf-8",
        newline="",
    ) as input_file:
        input_file.seek(0)
        reader = csv.reader(input_file, delimiter="\t")
        for line in reader:
            total_lines_read += 1
            addr = int(line[0])
            offset = int(line[1])
            bit_size = 0 if offset == 0 else math.ceil(math.log2(offset))
            value_set_per_bit_size[bit_size].add(offset)
            count_per_offset[offset] += 1
            idx = (addr >> 2) & 0b1111
            value_count_per_addr_bit[idx][offset] += 1
            for i in [1, 2, 4, 8, 16, 32, 64, 128, 256]:
                idx = (addr >> 2) & ((1 << i) - 1)
                ways_per_set_per_idx_size_count[i][idx].add(offset)


def main(args):
    for benchmark in os.listdir(args.result_dir[0]):
        print(f"Handling {benchmark}")
        if benchmark in ["graphs", "raw_data", "deactivated"]:
            continue
        benchmark_path = os.path.join(
            args.result_dir[0], benchmark, "sizes_champsim32k"
        )
        for workload in os.listdir(benchmark_path):
            print(f"\t{workload}: ...", end="", flush=True)
            read_single_tsv(os.path.join(benchmark_path, workload))
            print(f"\r\t{workload}:    \tCOMPLETED")
    print(f"TOTAL LINES READ: {total_lines_read}")
    out_dir = os.path.join(args.result_dir[0], "raw_data")
    os.makedirs(out_dir, exist_ok=True)
    write_result_files(out_dir)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Extract offset statistics from tsv and result subdirectories"
    )
    parser.add_argument(
        "--result_dir",
        metavar="REULT DIRECTORY",
        dest="result_dir",
        type=str,
        nargs=1,
        help="Directory that contains the subdirectory per benchmark.",
    )

    args = parser.parse_args()
    main(args)
