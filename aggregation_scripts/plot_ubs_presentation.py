#!/usr/bin/env python3
"""Presentation copy of the relative IPC plot, excluding 2 and 256 KiB groups."""
import argparse
import csv
from pathlib import Path

from plot_ipc import configuration_key, configuration_storage, plot


def presentation_rows(rows):
    # Recompute labels from configuration identity even for older input CSVs
    # whose storage_kib column used the obsolete UBS-halving convention.
    corrected = []
    for row in rows:
        capacity = configuration_storage(row['configuration'])
        if capacity is None:
            raise ValueError(f"cannot determine nominal capacity: {row['configuration']}")
        if capacity not in (2, 256):
            corrected.append({**row, 'storage_kib': capacity})
    return corrected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=Path('ubs_ipc_relative_plot.csv'))
    parser.add_argument('--output', type=Path, default=Path('presentation/ubs_ipc_relative'))
    args = parser.parse_args()
    with args.input.open(newline='') as stream:
        rows = presentation_rows(csv.DictReader(stream))
    if not rows:
        parser.error('no plot values remain after filtering')
    categories = [c for c in ('overall', 'client', 'server', 'spec') if any(r['category'] == c for r in rows)]
    configs = sorted({r['configuration'] for r in rows}, key=configuration_key)
    summary = {name: {} for name in configs}
    values = {}
    for row in rows:
        config, category = row['configuration'], row['category']
        value = float(row['value'])
        summary[config][category] = dict(mean=value, count=int(row['trace_count']))
        variant = 'UBS' if '_ubs_' in config else 'Conventional'
        key = (float(row['storage_kib']), category, variant)
        if key in values:
            raise ValueError(f'duplicate plot value: {key}')
        values[key] = value
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # One row per storage group and one column per plotted series: directly
    # usable as an Excel clustered-column chart, with no pivot/aggregation.
    excel_path = args.output.with_name(args.output.name + '_excel.csv')
    series = [(category, variant) for category in categories for variant in ('Conventional', 'UBS')]
    with excel_path.open('w', encoding='utf-8-sig', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['Nominal capacity (KiB)'] + [f'{c.title()} - {v}' for c, v in series])
        for storage in sorted({float(r['storage_kib']) for r in rows}):
            writer.writerow([f'{storage:g} KiB'] + [values.get((storage, c, v), '') for c, v in series])
    for suffix in ('.pdf', '.png', '.svg'):
        plot(summary, configs, categories, args.output.with_suffix(suffix),
             'Instruction-cache performance', 'Harmonic mean speedup vs 16 KiB baseline (%)',
             reference_line=0.0, center_on_reference=True)
    print(f'Figures: {args.output}.[pdf,png,svg]')
    print(f'Excel chart data: {excel_path}')
    print(f'Exact long-form data: {args.output.with_name(args.output.name + "_plot.csv")}')


if __name__ == '__main__':
    main()
