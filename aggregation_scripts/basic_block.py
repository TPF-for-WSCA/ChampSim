from collections import defaultdict

import itertools
import os
import re
import sys

basic_block_counts = defaultdict(int)

for benchmark in os.listdir(sys.argv[1]):
    if benchmark in ["graphs", "raw_data", "archive"]:
        continue
    bench = os.path.join(sys.argv[1], benchmark)
    if os.path.isfile(bench):
        continue
    bench = os.path.join(sys.argv[1], benchmark, "sizes_8k_abtb_tag_15b")
    for workload in os.listdir(bench):
        workdir = os.path.isfile(os.path.join(bench, workload))
        if workload.endswith(".tsv") or os.path.isfile(workdir):
            continue
        txt_file = next(f for f in os.listdir(os.path.join(bench, workload)) if f.endswith('.txt'))
        logfile = os.path.join(bench, workload, txt_file)
        logs = []
        with open(logfile) as f:
            logs = f.readlines()
        regex = re.compile(r"BASIC BLOCK SIZE\tCOUNT")
        ilogs = iter(logs)
        for line in ilogs:
            matches = regex.search(line)
            if matches:
                break
        
        for line in ilogs:
            if line.startswith("SQUASHED") or not line.strip():
                break
            block_size = int(line.split("\t")[0].strip())
            count = int(line.split("\t")[1].strip())
            if block_size > 256:
                continue
            if block_size > 128:
                block_size = 128
            basic_block_counts[block_size] += count


with open(os.path.join(sys.argv[1], "raw_data", "basic_block_counts.tsv"), "w+") as f:
    for (block_size, count) in basic_block_counts.items():
        f.write(f"{block_size}\t{count}\n")
