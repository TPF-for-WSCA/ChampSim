import argparse
import os
import struct


def split_1(line):
    pass


def split_2(line):
    pass


def split_4(line):
    pass


def split_50p(line):
    pass


def split_25p(line):
    pass


def split_12_5p(line):
    pass


def split_6_25p(line):
    pass


def split_3_125p(line):
    pass


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
    return mask_array


def apply_splits_for_workload(workload_name, tracefile_path, output_file_path):
    with open(tracefile_path, "rb") as tracefile:
        while True:
            line = tracefile.read(8)
            print(line)
            intline = struct.unpack("ii", line)
            array_line = int_t_to_boolean_list(intline)
            print(array_line)
            if not line:
                break


def main(args):
    trace_directory = args.trace_dir

    for workload in os.listdir(trace_directory):
        apply_splits_for_workload(
            workload,
            os.path.join(trace_directory, workload, "cl_access_masks.bin"),
            os.path.join(trace_directory, workload, "cl_splits_overhead.tsv"),
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse cacheline access masks and calculate number of cache"
        " blocks under given strategies"
    )
    parser.add_argument("trace_dir", type=str)
    args = parser.parse_args()
    main(args)
