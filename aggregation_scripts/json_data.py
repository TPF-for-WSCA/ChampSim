import json
import os
import sys

regions = {}

assert(len(sys.argv) == 2)
for dir in os.listdir(sys.argv[1]):
    json_data = None
    with open(os.path.join(sys.argv[1], dir, stats.json)) as f:
        json_data = json.load(f)
    regions_data = json_data[0]['roi']['cores'][0]['regions_covered']
    regions[100][dir] = regions_data['100%']
    regions[99.5][dir] = regions_data['99.5%']
    regions[99][dir] = regions_data['99%']
    regions[95][dir] = regions_data['95%']
    regions[90][dir] = regions_data['90%']

for percentage in [100, 99.5, 99, 95, 90]:
    outname = os.path.join(sys.argv[1], f"{percentage}_region_sampling.tsv")
    with open(outname, "w+") as outfile:
        headers = regions[percentage].keys()
        for header in headers:
            outfile.write(f"\t{header}")
        outfile.write("\n")
        for header in headers:
            outfile.write(f"\t{regions[percentage][header]}")
        outfile.write("\n")
