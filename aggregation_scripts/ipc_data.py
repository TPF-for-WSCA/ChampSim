#!/usr/bin/python
import os
import re
import sys
from collections import defaultdict
from enum import Enum


class STATS(Enum):
    IPC = 1
    MPKI = 2


type = STATS.IPC


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
        for logfile in os.listdir(f"{path}/{workload}"):
            if not ".txt" in logfile:
                continue
            if type == STATS.MPKI:
                stat_by_workload[workload] = extract_l1i_mpki(
                    f"{path}/{workload}/{logfile}"
                )
            else:
                stat_by_workload[workload] = extract_ipc(
                    f"{path}/{workload}/{logfile}"
                )
    return stat_by_workload


def mutliple_sizes_run():
    ipc_by_cachesize_and_workload = {}
    out_dir = sys.argv[1]
    for subdir in os.listdir(out_dir):
        if not os.path.isdir(os.path.join(out_dir, subdir)):
            continue
        matches = re.search(r"(\d+)([km])?", subdir)
        matches = matches.groups()
        if matches[1]:
            factor = 1024 if matches[1] == "k" else 1024 * 1024
            size_bytes = int(matches[0]) * factor
            ipc_by_cachesize_and_workload[size_bytes] = single_run(
                f"{out_dir}/{subdir}"
            )
        elif matches[0]:
            ipc_by_cachesize_and_workload[matches[0]] = single_run(
                f"{out_dir}/{subdir}"
            )
        else:
            ipc_by_cachesize_and_workload[subdir] = single_run(f"{out_dir}/{subdir}")
    return ipc_by_cachesize_and_workload


def write_tsv(data):
    header = False
    filename = "./ipc.tsv" if type == STATS.IPC else "./mpki.tsv"
    with open(filename, "w+") as outfile:
        for csize, values in data.items():
            if not header:
                header = True
                for workload, ipc in data[csize].items():
                    outfile.write(f"\t{workload}")
                outfile.write("\n")

            outfile.write(f"{csize}")
            for workload, ipc in data[csize].items():
                outfile.write(f"\t{ipc}")
            outfile.write("\n")


data = {}
if sys.argv[3] == "MPKI":
    type = STATS.MPKI
if sys.argv[2] == "single":
    data["const"] = single_run(sys.argv[1])
else:
    data = mutliple_sizes_run()

write_tsv(data)
