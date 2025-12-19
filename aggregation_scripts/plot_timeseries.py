import os
import sys

from matplotlib import pyplot, colors
import numpy as np
import pandas as pd

colors = list(colors.cnames.keys())


def draw_invalid_entry(path, fix, ax, workload, color="r"):
    dt = np.dtype([("Invalid Entries", "<u4")])
    data = np.fromfile(path, dtype=dt)
    df = pd.DataFrame(data, columns=data.dtype.names)
    (line,) = ax.plot(df, color)
    mean = df.loc[:, "Invalid Entries"].mean()
    line.set_label(f"{workload} [{mean:.2f}]")
    print(f"{path}:\t{mean}")
    ax.axhline(y=mean, color=color)


fix, ax = pyplot.subplots()
if len(sys.argv) > 2 and sys.argv[2] == "multi":
    nc = int(len(colors) / 2)
    for workload in os.listdir(sys.argv[1]):
        if not os.path.isdir(os.path.join(sys.argv[1], workload)):
            continue  # if we do this for a single single run
        if workload in ["graphs", "raw_data"]:
            continue
        draw_invalid_entry(
            f"{sys.argv[1]}/{workload}/cpu0_L1I_cl_num_invalid_blocks.bin",
            fix,
            ax,
            workload,
            colors[nc],
        )
        nc = (nc + 1) % len(colors)
else:
    draw_invalid_entry(
        f"{sys.argv[1]}/cpu0_L1I_cl_num_invalid_blocks.bin",
        fix,
        ax,
        sys.argv[1],
    )

max_entries = 18 * 64
ax.axhline(y=max_entries)
ax.legend()
pyplot.show()
