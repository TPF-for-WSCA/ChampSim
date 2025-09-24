import json
import math
import os
import sys

import plotly.express as px
import pandas as pd

from collections import defaultdict
import re

ignore_directories = ["graphs", "raw_data"]
def parse_config_value(config_name):
    match = re.search(r'(\d+)([kmg]?)', config_name, re.IGNORECASE)
    if not match:
        return 0
    value = int(match.group(1))
    suffix = match.group(2).lower()
    if suffix == 'k':
        value *= 2 ** 10
    elif suffix == 'm':
        value *= 2 ** 20
    elif suffix == 'g':
        value *= 2 ** 30
    return value

grouped_plot_data = []
benchmarks = sorted(os.listdir(sys.argv[1]))
for benchmark in benchmarks:
    full_path = os.path.join(sys.argv[1], benchmark)
    if not os.path.isdir(full_path) or benchmark in ignore_directories:
        continue
    for config in os.listdir(full_path):
        subdir_path = os.path.join(full_path, config)
        if not os.path.isdir(subdir_path):
            continue
        config_name = config.split("_")[1]
        for app in os.listdir(subdir_path):
            if not os.path.isdir(os.path.join(subdir_path, app)):
                continue
            try:
                with open(os.path.join(subdir_path, app, "stats.json")) as f:
                    json_data = json.load(f)
            except FileNotFoundError:
                print(f"ignoring {app} -- stats file does not exist")
                continue
            region_count_list = json_data[0]["roi"]["cores"][0]["region_count_samples"]
            
            for count in region_count_list:
                grouped_plot_data.append({
                    "Config": config_name,
                    "Region Count": count
                })

plot_data = grouped_plot_data

df = pd.DataFrame(plot_data)
df["Region Count"] = df.apply(lambda row: row["Region Count"] / parse_config_value(row["Config"]), axis=1)
import plotly.graph_objects as go

fig = go.Figure()
fig.update_layout(showlegend=False)
fig.update_yaxes(tickformat=".0%")
fig.update_yaxes(range=[0, df["Region Count"].max()])
for config in df["Config"].unique():
    config_data = df[df["Config"] == config]["Region Count"]
    fig.add_trace(go.Violin(
        y=config_data,
        x=[config] * len(config_data),
        box_visible=False,
        points=False,
        width=0.75,
        line_width=4,
        jitter=True,
        meanline_visible=True,
        line_color='rgba(0,0,155,0.5)',
    ))

fig.update_layout(
    title="Region Count Distribution per Config",
    violingap=0,
    xaxis_title="Config",
    yaxis_title="Region Count"
)

"""
fig = px.violin(
    df,
    x="Config",
    y="Region Count",
    box=False,
    points=False,
    title="Region Count Distribution per Config"
).update_layout(violingap=0)
"""


unique_configs = df["Config"].dropna().unique()
sorted_configs = sorted(unique_configs, key=parse_config_value)
fig.update_xaxes(type='category', categoryorder='array', categoryarray=sorted_configs)

output_dir = os.path.join(sys.argv[1], "graphs")
os.makedirs(output_dir, exist_ok=True)
fig.write_image(os.path.join(output_dir, "region_violin.pdf"))
fig.show()