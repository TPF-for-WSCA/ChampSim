import json
import math
import os
import sys

import plotly.express as px
import pandas as pd

from collections import defaultdict
import re

output_dir = os.path.join(sys.argv[1], "graphs")
os.makedirs(output_dir, exist_ok=True)

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
#garbage graph to get rid of the loading bullshit
fig = px.scatter(x=[0, 1, 2, 3, 4], y=[0, 1, 4, 9, 16])
fig.show()
fig.write_image(os.path.join(output_dir,"random.pdf"))

fig = go.Figure()
fig.update_layout(
    # width=400,  # typical single-column width in points (~3.3in)
    # height=300, # adjust as needed for aspect ratio
    template="plotly_white"
)
fig.update_layout(showlegend=False)
fig.update_yaxes(tickformat=".0%")
fig.update_yaxes(minor=dict(ticks="", showgrid=True))
fig.update_yaxes(range=[0, 0.48], dtick=0.08)
violincolor="rgba(173,216,230,0.5)"
whiskerscolor="#096BA6"
line_size=2
marker=dict(symbol='line-ew', color=whiskerscolor, size=2*line_size, line=dict(color=whiskerscolor, width=line_size))
for config in df["Config"].unique():
    config_data = df[df["Config"] == config]["Region Count"]
    median = config_data.median()
    min_val = config_data.min()
    max_val = config_data.max()
    fig.add_trace(go.Scatter(
        x=[config],
        y=[median],
        mode='markers',
        marker=marker,
        showlegend=False,
        hoverinfo='skip'
    ))
    fig.add_trace(go.Scatter(
        x=[config],
        y=[max_val],
        mode='markers',
        marker=marker,
        showlegend=False,
        hoverinfo='skip'
    ))
    fig.add_trace(go.Scatter(
        x=[config],
        y=[min_val],
        mode='markers',
        marker=marker,
        showlegend=False,
        hoverinfo='skip'
    ))
    fig.add_trace(go.Scatter(
        x=[config, config],
        y=[min_val, max_val],
        mode='lines',
        line=dict(color=whiskerscolor, width=line_size),
        showlegend=False,
        hoverinfo='skip'
    ))
    fig.add_trace(go.Violin(
        y=config_data,
        x=[config] * len(config_data),
        box_visible=False,
        points=False,
        width=0.75,
        line_width=0,
        jitter=False,
        meanline_visible=False,
        line_color=violincolor,
    ))

# Update figure for paper
fig.update_yaxes(showgrid=True, gridcolor='rgba(0,0,0,0.2)', zeroline=True, zerolinecolor='black', zerolinewidth=2)
fig.update_layout(
    title="",
    violingap=0,
    xaxis_title="BTB Size",
    yaxis_title="% Unique Tags of all BTB Tags",
    font=dict(size=12),
    width=340,
    height=500, # adjust as needed for clarity
    margin=dict(l=0, r=0, t=0, b=0)
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

fig.write_image(os.path.join(output_dir, "region_violin.pdf"))
fig.show()