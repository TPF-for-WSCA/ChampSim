import matplotlib
import numpy as np
import pandas as pd
import os

matplotlib.use("Agg")

import matplotlib.pyplot as plt

import matplotlib.ticker as mtick

path = "/Users/robrunne/micro_new_1_accumulated"
output_path = "/Users/robrunne/micro_new_1_accumulated"
# TODO: Combine spec, client, CVP-1 FP and INT traces into one graph
files_to_plot = [
    "accumulated_all_applications_cvp1_new_compute.tsv",
    "accumulated_all_applications_ipc1_spec.tsv",
    "accumulated_all_applications_ipc1_client.tsv",
]

x = [i for i in range(2, 65)]
frames = []
for file in files_to_plot:
    new_frame = pd.read_csv(os.path.join(path, file), sep="\t")
    frames.append(new_frame)

frame = pd.concat(frames, axis=1)

fig, ax = plt.subplots()
for (
    workload,
    data,
) in frame.items():
    if workload == "1":
        continue
    ax.plot(x, data, marker="+", markevery=(6, 8), label=workload)

ax.set_ylabel("Accumulated Number of Cachelines")
ax.set_xlabel("Useful Bytes in Cacheline")
ax.yaxis.set_major_formatter(mtick.PercentFormatter(1.0))
plt.grid(
    visible=True,
    which="both",
)
plt.savefig(os.path.join(output_path, f"accumulated_client.pdf"))
plt.savefig(os.path.join(output_path, f"accumulated_client.pgf"))
plt.close()
