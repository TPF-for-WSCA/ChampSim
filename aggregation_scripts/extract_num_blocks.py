import argparse
import math
import os
import struct
from collections import defaultdict, OrderedDict


def get_count_from_tracefile(tracefile_path):
    with open(tracefile_path, "rb") as tracefile:
        while True:
            line = tracefile.read(4)
            if not line:
                break
            intline = struct.unpack("i", line)
            yield intline


def main(args):
    blocks_histo_by_workload = defaultdict(lambda: defaultdict(int))
    for workload in os.listdir(args.trace_dir):
        if (
            not os.path.isdir(f"{args.trace_dir}/{workload}")
            or workload == "graphs"
            or workload == "raw_data"
        ):
            continue
        print(f"Handling {workload}")
        for num_blocks in get_count_from_tracefile(
            os.path.join(
                args.trace_dir, workload, "cpu0_L1I_cl_num_blocks.bin"
            )
        ):
            bucket = math.floor(num_blocks / 10) * 10
            blocks_histo_by_workload[workload][bucket] += 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("trace_dir", type=str)
    parser.add_argument(
        "type", type=str, help="Possible types are bin and tsv"
    )
    parser.add_argument("filename", type=str)
    args = parser.parse_args()
    main(args)
