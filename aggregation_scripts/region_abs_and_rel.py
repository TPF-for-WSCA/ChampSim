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

# Create two columns: normalized and not normalized
df["Region Count Normalized"] = df.apply(lambda row: row["Region Count"] / parse_config_value(row["Config"]), axis=1)
df["Region Count Absolute"] = df["Region Count"]

import plotly.graph_objects as go
#garbage graph to get rid of the loading bullshit
fig = px.scatter(x=[0, 1, 2, 3, 4], y=[0, 1, 4, 9, 16])
fig.show()
fig.write_image(os.path.join(output_dir,"random.pdf"))

fig = go.Figure()
fig.update_layout(
    legend=dict(
        x=-0.025,
        y=1.025,
        xanchor="left",
        yanchor="top",
        bgcolor="rgba(255,0,0,0.0)",
        bordercolor="rgba(255,0,0,0.0)",
        borderwidth=0.0
    )
)
fig.update_layout(showlegend=True)

violincolor_norm = "rgba(173,216,230,0.5)"
violincolor_raw = "rgba(255,140,0,0.4)"
whiskerscolor_norm = "#096BA6"
whiskerscolor_raw = "#cc7000"
line_size = 2

marker_norm = dict(symbol='line-ew', color=whiskerscolor_norm, size=2*line_size, line=dict(color=whiskerscolor_norm, width=line_size))
marker_raw = dict(symbol='line-ew', color=whiskerscolor_raw, size=2*line_size, line=dict(color=whiskerscolor_raw, width=line_size))

def draw_whiskers(config_data, marker,y):
    median = config_data.median()
    min_val = config_data.min()
    max_val = config_data.max()
    fig.add_trace(go.Scatter(
        x=[config],
        y=[median],
        mode='markers',
        marker=marker,
        showlegend=False,
        yaxis=y,
        hoverinfo='skip'
    ))
    fig.add_trace(go.Scatter(
        x=[config],
        y=[max_val],
        mode='markers',
        marker=marker,
        showlegend=False,
        yaxis=y,
        hoverinfo='skip'
    ))
    fig.add_trace(go.Scatter(
        x=[config],
        y=[min_val],
        mode='markers',
        marker=marker,
        showlegend=False,
        yaxis=y,
        hoverinfo='skip'
    ))
    fig.add_trace(go.Scatter(
        x=[config, config],
        y=[min_val, max_val],
        mode='lines',
        line=marker.get('line'),
        showlegend=False,
        yaxis=y,
        hoverinfo='skip'
    ))

for config in df["Config"].unique():
    config_data_norm = df[df["Config"] == config]["Region Count Normalized"]
    config_data_raw = df[df["Config"] == config]["Region Count Absolute"]
    # Normalized violin
    fig.add_trace(go.Violin(
        y=config_data_norm,
        x=[config] * len(config_data_norm),
        name="Normalized",
        box_visible=False,
        points=False,
        width=1.0,
        line_width=0,
        jitter=False,
        meanline_visible=False,
        line_color=violincolor_norm,
        fillcolor=violincolor_norm,
        opacity=1,
        legendgroup="norm",
        offsetgroup="norm",
        yaxis="y2",
        showlegend=(config == df["Config"].unique()[0]),
        side="positive",
    ))
    # Raw violin
    fig.add_trace(go.Violin(
        y=config_data_raw,
        x=[config] * len(config_data_raw),
        name="Absolute",
        box_visible=False,
        points=False,
        width=1.0,
        line_width=0,
        jitter=False,
        meanline_visible=False,
        line_color=violincolor_raw,
        fillcolor=violincolor_raw,
        opacity=1,
        offsetgroup="raw",
        legendgroup="raw",
        yaxis="y1",
        showlegend=(config == df["Config"].unique()[0]),
        side="negative"
    ))

    draw_whiskers(config_data_norm, marker_norm, "y2")
    draw_whiskers(config_data_raw, marker_raw, "y1")

# Update figure for paper

fig.update_layout(
    violinmode='group'
)
fig.update_layout(
    yaxis=dict(
        title="Number of Unique Tags",
        showgrid=True,
        gridcolor='rgba(0,0,0,0.2)',
        zeroline=True,
        tickformat=None,
        range=[0, 1200],
        dtick=150,
        zerolinecolor='black',
        zerolinewidth=2,
    ),
    yaxis2=dict(
        title="% of Entries Occupied by Unique Tags",
        overlaying='y',
        side='right',
        tickformat=".0%",
        range=[0, 0.40],
        dtick=0.05,
    ),
    title="",
    violingap=1.0,
    xaxis_title="BTB Size",
    font=dict(size=10),
    width=340,
    height=450, # adjust as needed for clarity
    margin=dict(l=0, r=0, t=0, b=0),
    template="plotly_white"
)
unique_configs = df["Config"].dropna().unique()
sorted_configs = sorted(unique_configs, key=parse_config_value)
fig.update_xaxes(type='category', categoryorder='array', categoryarray=sorted_configs)

fig.write_image(os.path.join(output_dir, "region_violin.pdf"))
fig.show()