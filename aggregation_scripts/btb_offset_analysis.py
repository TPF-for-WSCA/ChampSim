"""
Plot offset BTBs fill level over time and the amount of sharing.
"""

import argparse
import os

import pandas as pd
import numpy as np
import holoviews as hv


from holoviews import opts

hv.extension("bokeh")


def main(args):
    for i in range(4):
        file_path = os.path.join(
            args.logdir, f"cpu0_offset_btb_{i}_sharing_over_time.json"
        )
        offset_information = pd.read_json(file_path)
        num_offset_entries_by_time = {}
        num_idx_entries_by_time = {}
        max_share_by_time = {}
        i = 0
        for single_record in offset_information.ref_frequencies:
            offset_entries = 0
            idx_entries = 0
            max_shared_offset = 0
            for entry in single_record:
                if entry["ref_count"] == 0:
                    continue
                idx_entries += entry["ref_count"] * entry["frequency"]
                offset_entries += entry["frequency"]
                if max_shared_offset < entry["ref_count"]:
                    max_shared_offset = entry["ref_count"]
            num_offset_entries_by_time[i] = offset_entries
            num_idx_entries_by_time[i] = idx_entries
            max_share_by_time[i] = max_shared_offset
            i += 1
        scatter = hv.Scatter(num_idx_entries_by_time)
        hv.save(scatter, os.path.join(args.logdir, "idx_entries_over_time.html"))
        scatter = hv.Scatter(num_offset_entries_by_time)
        hv.save(scatter, os.path.join(args.logdir, "offset_entries_over_time.html"))
        scatter = hv.Scatter(max_share_by_time)
        hv.save(scatter, os.path.join(args.logdir, "max_share_over_time.html"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse offset json files and analyse it per offset btb"
    )
    parser.add_argument("logdir")
    args = parser.parse_args()
    main(args)
