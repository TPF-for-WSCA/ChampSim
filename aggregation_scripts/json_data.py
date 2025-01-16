import json
import os
import sys

regions = {100: {}, 99.5: {}, 99: {}, 95: {}, 90: {}}

assert(len(sys.argv) == 2)
for dir in os.listdir(sys.argv[1]):
    if not os.path.isdir(os.path.join(sys.argv[1], dir)):
        continue
    if dir.startswith(".") or ".tsv" in dir or ".txt" in dir:
        continue
    json_data = None
    with open(os.path.join(sys.argv[1], dir, "stats.json")) as f:
        json_data = json.load(f)
    regions_data = json_data[0]['roi']['cores'][0]['regions_covered']
    regions[100][dir] = regions_data['100%']
    regions[99.5][dir] = regions_data['99.5%']
    regions[99][dir] = regions_data['99%']
    regions[95][dir] = regions_data['95%']
    regions[90][dir] = regions_data['90%']

for percentage in [100, 99.5, 99, 95, 90]:
    outname = os.path.join(sys.argv[1], f"{percentage}_region_sampling.tsv")
    max_idx = 0
    with open(outname, "w+") as outfile:
        headers = list(regions[percentage].keys())
        for header in headers:
            outfile.write(f"\t{header}")
            max_idx = max(len(regions[percentage][header]), max_idx)
        outfile.write("\n")
        # TODO: loop over single entries in regions, one at a time for all headers and add them
        idx = 0
        while(1):
            for header in headers:
                if (len(regions[percentage][header]) <= idx):
                    outfile.write("\t''")
                    continue
                outfile.write(f"\t'{regions[percentage][header][idx]}'")
            outfile.write("\n")
            idx += 1
            if idx == max_idx:
                break

