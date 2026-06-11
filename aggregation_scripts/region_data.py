import json
import os
import sys

import plotly.express as px
import pandas as pd

import re

import plotly.graph_objects as go

output_dir = os.path.join(sys.argv[1], "graphs")
os.makedirs(output_dir, exist_ok=True)

ignore_directories = ["graphs", "raw_data", "ignored"]
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

def parse_result_config(config_name):
    match = re.fullmatch(r"sizes?_(?P<size>\d+[kmg]?)(?:_(?P<hash_function>.+))?", config_name, re.IGNORECASE)
    if match:
        return match.group("hash_function") or "default", parse_config_value(match.group("size"))
    return config_name, parse_config_value(config_name)

def add_whiskers(fig, df, y_column, hash_functions):
    whiskerscolor="#096BA6"
    line_size=2
    marker=dict(symbol='line-ew', color=whiskerscolor, size=2*line_size, line=dict(color=whiskerscolor, width=line_size))
    for hash_function in hash_functions:
        config_data = df[df["Hash Function"] == hash_function][y_column]
        median = config_data.median()
        min_val = config_data.min()
        max_val = config_data.max()
        fig.add_trace(go.Scatter(
            x=[hash_function],
            y=[median],
            mode='markers',
            marker=marker,
            showlegend=False,
            hoverinfo='skip'
        ))
        fig.add_trace(go.Scatter(
            x=[hash_function],
            y=[max_val],
            mode='markers',
            marker=marker,
            showlegend=False,
            hoverinfo='skip'
        ))
        fig.add_trace(go.Scatter(
            x=[hash_function],
            y=[min_val],
            mode='markers',
            marker=marker,
            showlegend=False,
            hoverinfo='skip'
        ))
        fig.add_trace(go.Scatter(
            x=[hash_function, hash_function],
            y=[min_val, max_val],
            mode='lines',
            line=dict(color=whiskerscolor, width=line_size),
            showlegend=False,
            hoverinfo='skip'
        ))

def build_violin(df, y_column, y_axis_title, hash_functions):
    violincolor="rgba(173,216,230,0.5)"
    fig = px.violin(
        df,
        x="Hash Function",
        y=y_column,
        box=False,
        points=False,
        category_orders={"Hash Function": hash_functions},
    )
    fig.update_layout(showlegend=False)
    fig.update_traces(
        width=0.75,
        line_width=0,
        jitter=False,
        meanline_visible=False,
        line_color=violincolor,
        fillcolor=violincolor,
    )
    add_whiskers(fig, df, y_column, hash_functions)
    fig.update_yaxes(showgrid=True, gridcolor='rgba(0,0,0,0.2)', zeroline=True, zerolinecolor='black', zerolinewidth=2)
    fig.update_layout(
        title="",
        violingap=0,
        xaxis_title="Tag Hash Function",
        yaxis_title=y_axis_title,
        font=dict(size=9),
        width=340,
        height=200, # adjust as needed for clarity
        template="plotly_white",
        margin=dict(l=0, r=0, t=0, b=0)
    )
    fig.update_xaxes(type='category', categoryorder='array', categoryarray=hash_functions)
    return fig

def write_plot(fig, output_basename):
    html_path = os.path.join(output_dir, f"{output_basename}.html")
    pdf_path = os.path.join(output_dir, f"{output_basename}.pdf")
    fig.write_html(html_path)
    try:
        fig.write_image(pdf_path)
    except RuntimeError as err:
        print(f"could not write {pdf_path}: {err}", file=sys.stderr)

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
        hash_function, btb_capacity = parse_result_config(config)
        if btb_capacity == 0:
            print(f"ignoring {config} -- could not parse BTB capacity", file=sys.stderr)
            continue
        for app in os.listdir(subdir_path):
            if not os.path.isdir(os.path.join(subdir_path, app)):
                continue
            try:
                with open(os.path.join(subdir_path, app, "stats.json")) as f:
                    json_data = json.load(f)
                print(f"\tprocessing {config}/{app}")
            except FileNotFoundError:
                print(f"ignoring {app} -- stats file does not exist", file=sys.stderr)
                continue
            region_count_list = json_data[0]["roi"]["cores"][0]["region_count_samples"]
            
            for count in region_count_list:
                grouped_plot_data.append({
                    "Hash Function": hash_function,
                    "BTB Capacity": btb_capacity,
                    "Region Count": count
                })

plot_data = grouped_plot_data

df = pd.DataFrame(plot_data)
if df.empty:
    sys.exit("No region count samples found")
hash_functions = sorted(df["Hash Function"].dropna().unique())
print("Collected region samples per hash function:")
for hash_function, count in df.groupby("Hash Function")["Region Count"].count().reindex(hash_functions).items():
    print(f"\t{hash_function}: {count}")
#garbage graph to get rid of the loading bullshit
fig = px.scatter(x=[0, 1, 2, 3, 4], y=[0, 1, 4, 9, 16])
fig.show()
write_plot(fig, "random")

fig = build_violin(df, "Region Count", "Region Count", hash_functions)
write_plot(fig, "region_violin_absolute")
fig.show()

df["Region Count / BTB Capacity"] = df.apply(lambda row: row["Region Count"] / row["BTB Capacity"], axis=1)
fig = build_violin(df, "Region Count / BTB Capacity", "Region Count / BTB Capacity", hash_functions)
fig.update_yaxes(tickformat="0%")
fig.update_yaxes(minor=dict(ticks="", showgrid=False))
fig.update_yaxes(range=[0, 0.35], dtick=0.05)
write_plot(fig, "region_violin_relative")
fig.show()
