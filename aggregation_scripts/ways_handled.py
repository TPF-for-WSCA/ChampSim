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


def extract_linex_handled_by_way(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex_title = re.compile(
        "cpu0\_L1I LINES HANDLED PER WAY\n"
    )  # CHANGE T WHATEVER CACHE YOU WANT TO LOOK AT
    regex_other_title = re.compile(
        "cpu0\_[a-z0-9A-Z\_]+ LINES HANDLED PER WAY\n"
    )
    logs.reverse()
    lines_per_way = []
    collect_lines = False
    for line in logs:  # reverse to find last run first
        matches = regex_title.match(line)
        if matches:
            lines_per_way.reverse()
            return lines_per_way
        matches = regex_other_title.match(line)
        if not matches:
            matches = re.compile("Time Spent in Buffer by").match(line)
        if matches:
            lines_per_way = []
            collect_lines = True
            continue
        if collect_lines:
            num_cl = int(line.strip().split(":")[1].strip())
            lines_per_way.append(num_cl)


def single_run(path):
    stat_by_workload = {}
    for workload in os.listdir(path):
        if (
            not os.path.isdir(os.path.join(path, workload))
            or workload == "graphs"
            or workload == "raw_data"
        ):
            continue  # if we do this for a single single run
        for logfile in os.listdir(f"{path}/{workload}"):
            if not ".txt" in logfile:
                continue
            stat_by_workload[workload] = extract_linex_handled_by_way(
                f"{path}/{workload}/{logfile}"
            )
    return stat_by_workload


def write_tsv(data):
    filename = "./ways_handled.tsv"
    with open(filename, "w+") as outfile:
        for wload, values in data.items():
            if values is None:
                continue
            outfile.write(f"{wload}")
            for lines in values:
                outfile.write(f"\t{lines}")
            outfile.write("\n")


data = {}
data = single_run(sys.argv[1])
write_tsv(data)
