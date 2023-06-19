import os
import sys

from matplotlib import pyplot
import numpy as np
import pandas as pd

colors = ["g", "r", "b", "g", "r", "b"]


def draw_invalid_entry(path, fix, ax, color="r"):
    dt = np.dtype([("Invalid Entries", "<u4")])
    data = np.fromfile(path, dtype=dt)
    df = pd.DataFrame(data, columns=data.dtype.names)
    ax.plot(df, color)
    mean = df.loc[:, "Invalid Entries"].mean()
    print(f"{path}:\t{mean}")
    ax.axhline(y=mean, label=f"Avg Invalid Entries [{mean}]", color=color)


fix, ax = pyplot.subplots()
if len(sys.argv) > 2 and sys.argv[2] == "multi":
    nc = 0
    for workload in os.listdir(sys.argv[1]):
        if not os.path.isdir(os.path.join(sys.argv[1], workload)):
            continue  # if we do this for a single single run
        if workload in ["graphs", "raw_data"]:
            continue
        draw_invalid_entry(
            f"{sys.argv[1]}/{workload}/cpu0_L1I_cl_num_invalid_blocks.bin",
            fix,
            ax,
            colors[nc],
        )
        nc = (nc + 1) % 6
        if nc == 0:
            break
else:
    draw_invalid_entry(
        f"{sys.argv[1]}/cpu0_L1I_cl_num_invalid_blocks.bin", fix, ax
    )

max_entries = 18 * 64
ax.axhline(y=max_entries)
pyplot.show()
