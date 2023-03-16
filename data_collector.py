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

experiment_instructions=900000000

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
        "1000000",
        "-simulation_instructions",
        str(experiment_instructions),
        "-result_dir",
        output_dir,
        "-ptrace",
        "-traces",
        trace_file_path,
    ]
    print(f"EXECUTE {' '.join(str(cmd))}", flush=True)
    os.makedirs(output_dir, exist_ok=True)
    success = True
    completed_experiment = subprocess.run(cmd, -1, capture_output=True)
    if completed_experiment.returncode != 0:
        print(f"WARNING: EXPERIMENT {' '.join(cmd)} returned non-zero code", flush=True)
        print(f"STDERR: {completed_experiment.stderr}\n", flush=True)
        success = False
    else:
        print(f"Experiment {' '.join(cmd)} completed successfully\n", flush=True)

    config_file_name = path.split(trace_file_path)[1].split(".")[0]
    with open(
        path.join(output_dir, f"{config_file_name}_log.txt"), mode="ab+"
    ) as f:
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
    global experiment_instructions
    if args.instr:
        experiment_instructions = args.instr
    traces_directory = args.traces_directory[0]
    workloads = os.listdir(traces_directory)
    trace_files = []
    for workload in workloads:
        workload_dir = path.join(traces_directory, workload)
        filenames = filter(
            lambda trace: trace.endswith(".gz"), os.listdir(workload_dir)
        )
        trace_files.extend(map(partial(path.join, workload_dir), filenames))

    output_dir = args.output_dir[0]
    if args.exec:
        global executable
        executable = args.exec

    pool = Pool(processes=8)
    pending_experiments = []

    for trace in trace_files:
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
    parser = argparse.ArgumentParser(
        description="Run an experiment on a set of traces"
    )
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
        "--output_dir",
        dest="output_dir",
        metavar="OUTPUT DIR",
        type=str,
        nargs=1,
        help="Directory where output should be stored",
    )

    parser.add_argument(
        "--instructions",
        dest="instr",
        type=int,
        help=f"Optional: Number of instructions to simulate. Default is {experiment_instructions}"
    )

    args = parser.parse_args()
    main(args)
