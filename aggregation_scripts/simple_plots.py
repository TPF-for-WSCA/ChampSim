import matplotlib
import numpy as np
import pandas as pd
import os

matplotlib.use("Agg")

import matplotlib.pyplot as plt

import matplotlib.ticker as mtick

path = "/home/robrunne/OneDrive/Research/results/fa_v11/ipc1_8w_comp/raw_data"
output_path = (
    "/home/robrunne/OneDrive/Research/results/fa_v11/ipc1_8w_comp/graphs"
)
files_to_plot = [
    "accumulated_all_applications_ipc1_server.tsv",
    "accumulated_all_applications_ipc1_spec.tsv",
    "accumulated_all_applications_ipc1_client.tsv",
]

x = [i for i in range(2, 65)]
for file in files_to_plot:
    frame = pd.read_csv(os.path.join(path, file), sep="\t")
    fig, ax = plt.subplots()
    for (
        workload,
        data,
    ) in frame.iteritems():
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
    plt.savefig(os.path.join(output_path, f"{file}.pdf"))
    plt.savefig(os.path.join(output_path, f"{file}.pgf"))
    plt.close()
