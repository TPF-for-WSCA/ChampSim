"""
Plot offset BTBs fill level over time and the amount of sharing.
"""

import argparse
import os

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


def main(args):
    for i in range(4):
        file_path = os.path.join(
            args.logdir, f"cpu0_offset_btb_{i}_sharing_over_time.json"
        )
        offset_information = pd.read_json(file_path)
        num_offset_entries_by_time = {}
        num_idx_entries_by_time = {}
        max_share_by_time = {}
        compression_factor_by_time = {}
        j = 0
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
            num_offset_entries_by_time[j] = offset_entries
            num_idx_entries_by_time[j] = idx_entries
            compression_factor_by_time[j] = idx_entries / offset_entries
            max_share_by_time[j] = max_shared_offset
            j += 1
        num_offset_entries_by_time = np.array(list(num_offset_entries_by_time.items()))[
            :, 1
        ]
        num_idx_entries_by_time = np.array(list(num_idx_entries_by_time.items()))[:, 1]
        max_share_by_time = np.array(list(max_share_by_time.items()))[:, 1]
        compression_factor_by_time = np.array(list(compression_factor_by_time.items()))[
            :, 1
        ]
        plt.figure(figsize=(10, 20))
        plt.subplot(212)
        plt.plot(max_share_by_time, label="Max Shared Offset")
        plt.legend(loc="upper right")

        plt.subplot(211)
        plt.plot(num_idx_entries_by_time, "r--", label="Index BTB entries")
        plt.plot(num_offset_entries_by_time, "b-", label="Offset BTB entries")
        plt.legend(loc="lower right")
        ax2 = plt.gca().twinx()
        ax2.plot(compression_factor_by_time, "g-", label="Compression Factor")
        ax2.legend(loc="upper right")
        plt.savefig(os.path.join(args.logdir, f"btb_{i}_offset_stats.pdf"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse offset json files and analyse it per offset btb"
    )
    parser.add_argument("logdir")
    args = parser.parse_args()
    main(args)
