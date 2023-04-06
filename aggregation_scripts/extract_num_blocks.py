import argparse
import os
import pathlib


def main(args):
    for workload in os.listdir(args.trace_dir):
        if (
            not os.path.isdir(f"{args.trace_dir}/{workload}")
            or workload == graphs
        ):
            continue
        print(f"Handling {workload}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("trace_dir", type=str)
    parser.add_argument(
        "type", type=str, help="Possible types are bin and tsv"
    )
    parser.add_argument("filename", type=str)
    args = parser.parse_args()
    main(args)
