#!/usr/bin/python
import os
import re
import sys
from collections import defaultdict

ipc_by_cachesize_and_workload = defaultdict((lambda: defaultdict(int)))
out_dir=sys.argv[1]
for subdir in os.listdir(out_dir):
    if not os.path.isdir(
        os.path.join(out_dir, subdir)
    ):
        continue
    size = subdir.split("champsim")[2]
    matches = re.match(r"(\d+)([km])", size).groups()
    factor = 1024 if matches[1] == "k" else 1024*1024
    size_bytes = int(matches[0]) * factor
    for workload in os.listdir(
        f"{out_dir}/{subdir}"
    ):
        for logfile in os.listdir(
            f"{out_dir}/{subdir}/{workload}"
        ):
            if not ".txt" in logfile:
                continue

            logs = []
            with open(
                f"{out_dir}/{subdir}/{workload}/{logfile}"
            ) as f:
                logs = f.readlines()
            regex = re.compile("CPU 0 cumulative IPC\: ([+-]?\d*\.?\d+)")
            for line in logs:
                matches = regex.match(line)
                if matches:
                    ipc_by_cachesize_and_workload[size_bytes][
                        workload
                    ] = matches.groups()[0]

header = False
with open("./ipc.tsv", "w+") as outfile:
    for csize, values in ipc_by_cachesize_and_workload.items():
        if not header:
            header = True
            for workload, ipc in ipc_by_cachesize_and_workload[csize].items():
                outfile.write(f"\t{workload}")
            outfile.write("\n")

        outfile.write(f"{csize}")
        for workload, ipc in ipc_by_cachesize_and_workload[csize].items():
            outfile.write(f"\t{ipc}")
        outfile.write("\n")
