#!/usr/bin/python
import os
import re
import sys
from collections import defaultdict
from enum import Enum


class STATS(Enum):
    IPC = 1
    MPKI = 2
    PARTIAL = 3
    BUFFER_DURATION = 4
    USELESS = 5


type = STATS.IPC

buffer = False


def extract_ipc(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile("CPU 0 cumulative IPC\: (\d*\.?\d+)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[0]


def extract_l1i_partial(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(
        "cpu0\_L1I TOTAL.*PARTIAL MISS\:\s+(\d*) \( (\d*\.?\d+)\%\)"
    )
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[1]
    return 0


def extrace_useless_percentage(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile("cpu0\_L1I EVICTIONS\:\s+(\d*)\s+USELESS:\s+(\d*)")
    if buffer:
        regex = re.compile(
            "cpu0\_L1I_buffer EVICTIONS\:\s+(\d*)\s+USELESS:\s+(\d*)"
        )

    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return int(matches.groups()[1]) / int(matches.groups()[0])
    return -1


def extract_l1i_mpki(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile("cpu0\_L1I MPKI\: (\d*\.?\d+)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[0]


def single_run(path):
    stat_by_workload = {}
    for workload in os.listdir(path):
        if not os.path.isdir(os.path.join(path, workload)):
            continue  # if we do this for a single single run
        if workload in ["graphs", "raw_data"]:
            continue
        for logfile in os.listdir(f"{path}/{workload}"):
            if not ".txt" in logfile or logfile.startswith("."):
                continue
            if type == STATS.MPKI:
                stat_by_workload[workload] = extract_l1i_mpki(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.PARTIAL:
                stat_by_workload[workload] = extract_l1i_partial(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.USELESS:
                stat_by_workload[workload] = extrace_useless_percentage(
                    f"{path}/{workload}/{logfile}"
                )
            else:
                stat_by_workload[workload] = extract_ipc(
                    f"{path}/{workload}/{logfile}"
                )
    return stat_by_workload


def mutliple_sizes_run(out_dir=None):
    ipc_by_cachesize_and_workload = {}
    if not out_dir:
        out_dir = sys.argv[1]
    for subdir in os.listdir(out_dir):
        if not os.path.isdir(os.path.join(out_dir, subdir)):
            continue
        lip_matches = re.search(r"(\d*)lip", subdir)
        matches = re.search(r"(\d+)([km])+", subdir)
        if not matches:
            ipc_by_cachesize_and_workload[subdir] = single_run(
                f"{out_dir}/{subdir}"
            )
            continue
        matches = matches.groups()
        lip_matches = matches.groups()
        name = ""
        if lip_matches:
            name = f"lip@{lip_matches[0]} "
        if matches[1]:
            factor = 1024 if matches[1] == "k" else 1024 * 1024
            size_bytes = int(matches[0]) * factor
            name += f"{size_bytes}"
        else:
            name = subdir
        ipc_by_cachesize_and_workload[name] = single_run(
            f"{out_dir}/{subdir}"
        )
    return ipc_by_cachesize_and_workload


def write_tsv(data, out_path=None):
    filename = "ipc"
    if type == STATS.MPKI:
        filename = "mpki"
    elif type == STATS.PARTIAL:
        filename = "partial"
    elif type == STATS.BUFFER_DURATION:
        filename = "avg_buffer"
    elif type == STATS.USELESS:
        filename = "useless"
    if buffer:
        filename += "_buffer"
    filename += ".tsv"
    if out_path:
        out_path = os.path.join(out_path, filename)
    else:
        out_path = os.path.join("./", filename)
    with open(out_path, "w+") as outfile:
        max_csize = 0
        abs_max = 0
        for csize, values in data.items():
            if len(values) > abs_max:
                abs_max = len(values)
                max_csize = csize
        headers = []
        for workload, _ in data[max_csize].items():
            outfile.write(f"\t{workload}")
            headers.append(workload)
        outfile.write("\n")
        for csize, values in data.items():
            outfile.write(f"{csize}")
            for header in headers:
                outfile.write(f"\t{data[csize].get(header, None)}")
            outfile.write("\n")


def multiple_benchmarks_run():
    curr_dir = sys.argv[1]
    for benchmark in os.listdir(curr_dir):
        benchpath = os.path.join(curr_dir, benchmark)
        if not os.path.isdir(benchpath):
            continue
        data = mutliple_sizes_run(out_dir=benchpath)
        write_tsv(data, out_path=benchpath)


data = {}
if sys.argv[3] == "MPKI":
    type = STATS.MPKI
elif sys.argv[3] == "PARTIAL":
    type = STATS.PARTIAL
elif sys.argv[3] == "BUFFER_DURATION":
    type = STATS.BUFFER_DURATION
elif sys.argv[3] == "USELESS_LINES":
    type = STATS.USELESS
if len(sys.argv) == 5 and sys.argv[4]:
    buffer = True
if sys.argv[2] == "single":
    data["const"] = single_run(sys.argv[1])
elif sys.argv[2] == "multibench":
    multiple_benchmarks_run()
    exit(0)
else:
    data = mutliple_sizes_run()

write_tsv(data)
