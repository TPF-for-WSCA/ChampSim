from datetime import datetime
from enum import Enum
from functools import partial
from multiprocessing import Pool, cpu_count
from os import path
from pathlib import Path

import argparse
import os
import subprocess
import sys

executable = "/cluster/work/romankb/dynamorio/build/clients/bin64/drcachesim"

warmup_instructions = 1000000
evaluation_instructions = 10000000


class Color(Enum):
    RED = "\033[31m"
    ENDC = "\033[m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"


def cprint(string, color: Color):
    print(color.value + string + Color.ENDC.value)


def run_experiment(
    trace_file_path,
    output_dir,
    vcl_perfect_predictor=None,
):
    cmd = [
        executable,
        "-warmup_instructions",
        str(warmup_instructions),
        "-simulation_instructions",
        str(evaluation_instructions),
        "-result_dir",
        str(output_dir),
    ]
    if args.intel:
        cmd.append("-intel")
    if args.som:
        cmd.append("-stallonmiss")
    if args.trace_format == "c":
        cmd.append("-c")
    if args.trace_format == "p":
        cmd.append("-ptrace")
    cmd.extend(["-traces", str(trace_file_path)])
    print(f"EXECUTE {' '.join(cmd)}", flush=True)
    os.makedirs(output_dir, exist_ok=True)
    success = True
    completed_experiment = subprocess.run(cmd, -1, capture_output=True)
    if completed_experiment.returncode != 0:
        print(
            f"WARNING: EXPERIMENT {' '.join(cmd)} returned non-zero code",
            flush=True,
        )
        print(f"STDERR: {completed_experiment.stderr}\n", flush=True)
        success = False
    else:
        print(f"Experiment {' '.join(cmd)} completed successfully\n", flush=True)

    config_file_name = path.split(trace_file_path)[1].split(".")[0]
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
        f.write(b"==================== STDERR ====================\n")
        f.write(completed_experiment.stderr)
        f.flush()
    sys.stdout.flush()
    sys.stderr.flush()
    return success


def get_perfect_predictor_file(base_path, trace_dir):
    pass


def main(args):
    global warmup_instructions, evaluation_instructions
    warmup_instructions = args.warmup
    evaluation_instructions = args.eval
    traces_directory = args.traces_directory[0]
    workloads = os.listdir(traces_directory)
    trace_files = []
    for workload in workloads:
        if args.subdir:
            workload_dir = path.join(traces_directory, workload)
            filenames = filter(
                lambda trace: trace.endswith(".gz") or trace.endswith(".xz"),
                os.listdir(workload_dir),
            )
            trace_files.extend(map(partial(path.join, workload_dir), filenames))
        else:
            if workload.endswith(".gz") or workload.endswith(".xz"):
                trace_files.append(path.join(traces_directory, workload))

    output_dir = args.output_dir[0]
    if args.exec:
        global executable
        executable = args.exec

    pool = Pool(processes=cpu_count())
    pending_experiments = []

    for trace in trace_files:
        trace_name = trace.split("/")[-1].rsplit(".", 1)[0]
        if args.subdir:
            trace_name = path.split(trace)[0].split("/")[-1]
        output_subdir = path.join(output_dir, trace_name)
        # TEST ONLY
        # run_experiment(trace, output_subdir)
        print(f"Run {trace_name} experiment", flush=True)
        pending_experiments.append(
            pool.apply_async(
                run_experiment,
                [
                    trace,
                    output_subdir,
                ],
            )
        )

    # To prevent subprocesses to be killed
    experiments = [experiment.get() for experiment in pending_experiments]

    for i in range(len(trace_files)):
        if experiments[i]:
            cprint(f"{trace_files[i]} finished successfully", Color.GREEN)
        else:
            cprint(f"{trace_files[i]} finished with errors", Color.RED)

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
    parser.set_defaults(intel=False)

    parser.add_argument(
        "--som",
        action="store_true",
    )
    parser.set_defaults(som=False)

    args = parser.parse_args()
    main(args)
