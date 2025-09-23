import json
import math
import os
import sys

import plotly.express as px
import pandas as pd

from collections import defaultdict

regions = {
    100: defaultdict(dict),
    99.5: defaultdict(dict),
    99: defaultdict(dict),
    95: defaultdict(dict),
    90: defaultdict(dict),
}

regions_per_way = {
    0: [],
    4: [],
    5: [],
    7: [],
    9: [],
    11: [],
    19: [],
    25: []
}
class workload_stats():
    def __init__(self, name):
        self.name = name
        self.stdev_per_way = {}
        self.mean_per_way = {}
        self.max_per_way = {}
        self.min_per_way = {}

workloads = []

all_ways = {}

regions_per_way_data = {}

assert len(sys.argv) == 2
for dir in os.listdir(sys.argv[1]):
    if not os.path.isdir(os.path.join(sys.argv[1], dir)):
        continue
    if dir.startswith(".") or ".tsv" in dir or ".txt" in dir:
        continue
    json_data = None
    try:
        with open(os.path.join(sys.argv[1], dir, "stats.json")) as f:
            json_data = json.load(f)
    except FileNotFoundError as e:
        print(f"File not found, ignoring {dir}")
        continue
    regions_data = json_data[0]["roi"]["cores"][0]["regions_covered"]
    overall_regions = json_data[0]["roi"]["cores"][0]["btb_regions"]["max"]
    regions_per_way_data = json_data[0]["roi"]["cores"][0]["region_samples_per_way"]
    all_ways[dir] = overall_regions
    for way in regions_data.keys():
        regions[100][int(way)][dir] = regions_data[way]["100%"]
        regions[99.5][int(way)][dir] = regions_data[way]["99.5%"]
        regions[99][int(way)][dir] = regions_data[way]["99%"]
        regions[95][int(way)][dir] = regions_data[way]["95%"]
        regions[90][int(way)][dir] = regions_data[way]["90%"]

    for measurement in regions_per_way_data:
        for size, reg_cnt in measurement:
            regions_per_way[size].append(reg_cnt)

    workload = workload_stats(name = dir.split(".")[0])
    with open(f"{sys.argv[1]}{dir.split('.')[0]}_regions_per_way.tsv", "w+") as outfile:
        headers = list(regions_per_way.keys())
        for header, counts in regions_per_way.items():
            outfile.write(f"{header}\t")
            mean = sum(counts)/len(counts)
            stdev = 0
            for count in counts:
                stdev += (count-mean)**2
                outfile.write(f"{count}\t")
            workload.mean_per_way[header] = mean
            workload.max_per_way[header] = max(counts)
            workload.min_per_way[header] = min(counts)
            workload.stdev_per_way[header] = math.sqrt(stdev / len(counts))
            outfile.write("\n")
    workloads.append(workload)


with open(f"{sys.argv[1]}summary_regions_per_way.tsv", "w+") as outfile:
    headers = list(regions_per_way.keys())
    for header in headers:
        outfile.write(f"\t{header}")
    outfile.write("\n")

    for workload in workloads:
        outfile.write(f"{workload.name}")  # TODO: Replace with propert stringify method
        for header in headers:
            outfile.write(f"\t{workload.mean_per_way[header]}")
        outfile.write("\n")
        for header in headers:
            outfile.write(f"\t{workload.max_per_way[header]}")
        outfile.write("\n")
        for header in headers:
            outfile.write(f"\t{workload.min_per_way[header]}")
        outfile.write("\n")
        for header in headers:
            outfile.write(f"\t{workload.stdev_per_way[header]}")
        outfile.write("\n")
        outfile.write("\n")

all_ways = dict(sorted(all_ways.items()))
outname = os.path.join(sys.argv[1], f"overall_max_region.tsv")
with open(outname, "w+") as outfile:
    headers = list(all_ways.keys())
    for header in headers:
        outfile.write(f"\t{header}")
    outfile.write("\n")
    # TODO: loop over single entries in regions, one at a time for all headers and add them
    for header in headers:
        outfile.write(f"\t{all_ways[header]}")
    outfile.write("\n")
for percentage in [100, 99.5, 99, 95, 90]:
    for way in regions[percentage].keys():
        outname = os.path.join(
            sys.argv[1], f"{percentage}_way_{way}_region_sampling.tsv"
        )
        max_idx = 0
        with open(outname, "w+") as outfile:
            headers = list(regions[percentage][way].keys())
            for header in headers:
                outfile.write(f"\t{header}")
                max_idx = max(len(regions[percentage][way][header]), max_idx)
            outfile.write("\n")
            # TODO: loop over single entries in regions, one at a time for all headers and add them
            idx = 0
            while 1:
                for header in headers:
                    if len(regions[percentage][way][header]) <= idx:
                        outfile.write("\t")
                        continue
                    outfile.write(f"\t{regions[percentage][way][header][idx]}")
                outfile.write("\n")
                idx += 1
                if idx == max_idx:
                    break
