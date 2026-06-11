from datetime import datetime
from enum import Enum
from functools import partial
from multiprocessing import Pool, cpu_count
from os import path
from pathlib import Path

import argparse
import os
import random
import subprocess
import sys

executable = "/cluster/work/romankb/dynamorio/build/clients/bin64/drcachesim"

warmup_instructions = 1000000
evaluation_instructions = 10000000
bit_ordering = "~/data/bit_ordering.txt"


class Color(Enum):
    RED = "\033[31m"
    ENDC = "\033[m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"


def cprint(string, color: Color):
    print(color.value + string + Color.ENDC.value)


def is_trace_file(filename):
    return filename.endswith(".gz") or filename.endswith(".xz")


def collect_trace_files(traces_directory, include_subdirs):
    workloads = sorted(os.listdir(traces_directory))
    trace_files = []
    for workload in workloads:
        workload_path = path.join(traces_directory, workload)
        if include_subdirs:
            if not path.isdir(workload_path):
                continue
            filenames = filter(is_trace_file, sorted(os.listdir(workload_path)))
            trace_files.extend(map(partial(path.join, workload_path), filenames))
        elif is_trace_file(workload):
            trace_files.append(workload_path)
    return trace_files


def parse_trace_set(trace_set):
    if "=" in trace_set:
        name, directory = trace_set.split("=", 1)
        name = name.strip()
        directory = directory.strip()
    else:
        directory = trace_set.strip()
        name = path.basename(path.normpath(directory))
    if not name:
        raise ValueError(f"Trace set '{trace_set}' has an empty name")
    if not directory:
        raise ValueError(f"Trace set '{trace_set}' has an empty directory")
    return name, directory


def random_ordered_trace_pairs(trace_files, count, rng):
    if len(trace_files) < 2 or count <= 0:
        return []

    max_unique_pairs = len(trace_files) * (len(trace_files) - 1)
    target_count = min(count, max_unique_pairs)
    pairs = []
    seen = set()
    attempts = 0
    max_attempts = max(target_count * 20, 100)

    while len(pairs) < target_count and attempts < max_attempts:
        attempts += 1
        warmup_trace, context_switch_trace = rng.sample(trace_files, 2)
        pair = (warmup_trace, context_switch_trace)
        if pair in seen:
            continue
        seen.add(pair)
        pairs.append(pair)

    if len(pairs) < target_count:
        for warmup_trace in trace_files:
            for context_switch_trace in trace_files:
                if warmup_trace == context_switch_trace:
                    continue
                pair = (warmup_trace, context_switch_trace)
                if pair in seen:
                    continue
                seen.add(pair)
                pairs.append(pair)
                if len(pairs) >= target_count:
                    return pairs

    return pairs


def make_random_context_switch_experiments(args):
    trace_sets = [parse_trace_set(trace_set) for trace_set in args.trace_set_dirs]
    rng = random.Random(args.random_seed)
    per_set_pairs = []

    for trace_set_name, trace_set_dir in trace_sets:
        trace_files = collect_trace_files(trace_set_dir, args.subdir)
        if len(trace_files) < 2:
            cprint(
                f"Skipping {trace_set_name}: found {len(trace_files)} trace(s), need at least 2",
                Color.YELLOW,
            )
            per_set_pairs.append((trace_set_name, []))
            continue

        pairs = random_ordered_trace_pairs(
            trace_files, args.random_context_switch_combinations, rng
        )
        rng.shuffle(pairs)
        per_set_pairs.append((trace_set_name, pairs))

    experiments = []
    set_index = 0
    while len(experiments) < args.random_context_switch_combinations:
        added_pair = False
        for _ in range(len(per_set_pairs)):
            trace_set_name, pairs = per_set_pairs[set_index]
            set_index = (set_index + 1) % len(per_set_pairs)
            if not pairs:
                continue
            warmup_trace, context_switch_trace = pairs.pop()
            experiments.append((trace_set_name, warmup_trace, context_switch_trace))
            added_pair = True
            break
        if not added_pair:
            break

    return experiments


def run_experiment(
    trace_file_path,
    output_dir,
    vcl_perfect_predictor=None,
    context_switch_trace_path=None,
):
    cmd = [
        executable,
        "--warmup_instructions",
        str(warmup_instructions),
        # "-result_dir",  # TODO: Re-add when we have more files written
        # str(output_dir),
    ]
    if evaluation_instructions > 0:
        cmd.extend(
            [
                "--simulation_instructions",
                str(evaluation_instructions),
            ]
        )

    cmd.extend(
        [
            "--json",
            f"{output_dir}/stats.json",
        ]
    )
    if context_switch_trace_path:
        cmd.extend(
            [
                "--context-switch-trace",
                str(context_switch_trace_path),
            ]
        )
    if args.btb_tag_hash:
        cmd.append("--btb-tag-hash")
        cmd.append(args.btb_tag_hash)
    if args.intel:
        cmd.append("--intel")
    if args.som:
        cmd.append("--stallonmiss")
    if args.trace_format == "c":
        cmd.append("--c")
    if args.trace_format == "p":
        cmd.append("--ptrace")
    cmd.extend([str(trace_file_path)])
    print(f"EXECUTE {' '.join(cmd)}", flush=True)
    os.makedirs(output_dir, exist_ok=True)
    success = True
    completed_experiment = subprocess.run(cmd, -1, capture_output=True, check=False)
    if completed_experiment.returncode != 0:
        print(
            f"WARNING: EXPERIMENT {' '.join(cmd)} returned non-zero code",
            flush=True,
        )
        print(f"STDERR: {completed_experiment.stderr}\n", flush=True)
        success = False
    else:
        print(f"Experiment {' '.join(cmd)} completed successfully\n", flush=True)

    result_trace_path = context_switch_trace_path or trace_file_path
    config_file_name = trace_stem(result_trace_path)
    with open(path.join(output_dir, f"{config_file_name}_log.txt"), mode="ab+") as f:
        now = datetime.now()
        datetimestring = now.strftime("%d.%m.%Y %H:%M")
        f.write(
            f"####################################################################################################\n#                                                                                                  #\n#                                                                                                  #\n#                                    NEW RUN - {datetimestring}                                    #\n#                                                                                                  #\n#                                                                                                  #\n####################################################################################################\n".encode()
        )
        f.write(b"==================== CMD ====================\n")
        f.write(" ".join(cmd).encode())
        f.write(b"\n==================== STDOUT ====================\n")
        f.write(completed_experiment.stdout)

    # TODO: Filter Stderr if there are too many of the same message, only print the first n and then a line saying how many more there were of the same
    with open(path.join(output_dir, f"stderr_{config_file_name}.err"), mode="ab+") as f:
        now = datetime.now()
        datetimestring = now.strftime("%d.%m.%Y %H:%M")
        f.write(
            f"####################################################################################################\n#                                                                                                  #\n#                                                                                                  #\n#                                    NEW RUN - {datetimestring}                                    #\n#                                                                                                  #\n#                                                                                                  #\n####################################################################################################\n".encode()
        )
        f.write(b"==================== CMD ====================\n")
        f.write(" ".join(cmd).encode())
        f.write(b"==================== STDERR ====================\n")
        f.write(completed_experiment.stderr)
        f.flush()
    sys.stdout.flush()
    sys.stderr.flush()
    return success


def trace_stem(trace_path):
    return path.split(trace_path)[1].rsplit(".", 1)[0]


def get_perfect_predictor_file(base_path, trace_dir):
    pass


def main(args):
    import psutil

    global warmup_instructions, evaluation_instructions
    warmup_instructions = args.warmup
    evaluation_instructions = args.eval

    output_dir = args.output_dir[0]
    if args.exec:
        global executable
        executable = args.exec

    pid = os.getpid()
    proc = psutil.Process(pid)

    pool = Pool(processes=len(proc.cpu_affinity()))
    print(f"RUNNING POOL ON {len(proc.cpu_affinity())}")
    pending_experiments = []

    if args.trace_set_dirs:
        scheduled_experiments = make_random_context_switch_experiments(args)
        print(
            f"Scheduled {len(scheduled_experiments)} random context-switch combinations",
            flush=True,
        )
        for trace_set_name, warmup_trace, context_switch_trace in scheduled_experiments:
            result_trace_name = (
                f"{trace_set_name}/{trace_stem(warmup_trace)}_to_"
                f"{trace_stem(context_switch_trace)}"
            )
            output_subdir = path.join(output_dir, result_trace_name)

            if path.exists(output_subdir):
                files = os.listdir(output_subdir)
                if any(f.endswith(".txt") for f in files):
                    cprint(f"{output_subdir} already computed", Color.YELLOW)
                    continue

            print(f"Run {result_trace_name} experiment", flush=True)
            pending_experiments.append(
                (
                    pool.apply_async(
                        run_experiment,
                        [
                            warmup_trace,
                            output_subdir,
                            None,
                            context_switch_trace,
                        ],
                    ),
                    result_trace_name,
                )
            )
    else:
        traces_directory = args.traces_directory[0]
        trace_files = collect_trace_files(traces_directory, args.subdir)

        for trace in trace_files:
            trace_name = trace_stem(trace)
            if args.subdir:
                trace_name = path.split(trace)[0].split("/")[-1]

            result_trace_name = trace_name
            if args.context_switch_trace:
                result_trace_name = trace_stem(args.context_switch_trace)

            output_subdir = path.join(output_dir, result_trace_name)

            # TEST ONLY
            # run_experiment(trace, output_subdir)
            if path.exists(output_subdir):
                files = os.listdir(output_subdir)
                if any(f.endswith(".txt") for f in files):
                    cprint(f"{output_subdir} already computed", Color.YELLOW)

                    continue
            print(f"Run {result_trace_name} experiment", flush=True)
            pending_experiments.append(
                (
                    pool.apply_async(
                        run_experiment,
                        [
                            trace,
                            output_subdir,
                            None,
                            args.context_switch_trace,
                        ],
                    ),
                    result_trace_name,
                )
            )

    # To prevent subprocesses to be killed
    experiments = [
        (experiment[0].get(), experiment[1]) for experiment in pending_experiments
    ]

    for exp in experiments:
        if exp[0]:
            cprint(f"{exp[1]} finished successfully", Color.GREEN)
        else:
            cprint(f"{exp[1]} finished with errors", Color.RED)

    pool.close()
    pool.join()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run an experiment on a set of traces")
    parser.add_argument(
        "--experiment_executable",
        metavar="EXECUTABLE PATH",
        dest="exec",
        type=str,
        nargs="?",
        default=None,
        help="Specify the location of the executable to run the experiment",
    )
    parser.add_argument(
        "--traces_directory",
        dest="traces_directory",
        metavar="TRACE DIRECTORY",
        type=str,
        nargs=1,
        help="Directory containing all traces in named subfolders",
    )
    parser.add_argument(
        "--trace-set-dirs",
        dest="trace_set_dirs",
        metavar="NAME=TRACE_DIRECTORY",
        type=str,
        nargs="+",
        default=None,
        help=(
            "Trace sets to sample for random context-switch experiments. "
            "Each value can be NAME=DIR or just DIR; when set, --traces_directory "
            "is ignored."
        ),
    )
    parser.add_argument(
        "--random-context-switch-combinations",
        type=int,
        default=50,
        help="Maximum number of random warmup/context-switch trace pairs to run.",
    )
    parser.add_argument(
        "--random-seed",
        type=int,
        default=None,
        help="Optional seed for reproducible random trace-pair selection.",
    )
    parser.add_argument(
        "--trace_format",
        type=str,
        nargs="?",
        default="champsim",
        choices=["p", "c"],
    )
    parser.add_argument(
        "--output_dir",
        dest="output_dir",
        metavar="OUTPUT DIR",
        type=str,
        nargs=1,
        help="Directory where output should be stored",
    )

    parser.add_argument(
        "--warmup",
        dest="warmup",
        type=int,
        help=f"Optional: Number of instructions to warmup. Default is {warmup_instructions}",
    )
    parser.add_argument(
        "--evaluation",
        dest="eval",
        type=int,
        help=f"Optional: Number of instructions to simulate. Default is {evaluation_instructions}",
    )

    parser.add_argument(
        "--nosub",
        dest="subdir",
        action="store_false",
    )
    parser.set_defaults(subdir=True)

    parser.add_argument(
        "--intel",
        action="store_true",
    )
    parser.add_argument("--btb_tag_hash", type=str, default=None)
    parser.set_defaults(intel=False)

    parser.add_argument(
        "--som",
        action="store_true",
    )
    parser.set_defaults(som=False)

    parser.add_argument(
        "--context-switch-trace",
        dest="context_switch_trace",
        type=str,
        default=None,
        help="Path to trace file to switch to after warmup (simulates context switch)",
    )

    args = parser.parse_args()
    main(args)
