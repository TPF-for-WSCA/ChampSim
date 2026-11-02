"""Shared L1I LOAD MPKI extraction, paired reductions, plots, and CSV exports."""
import csv
import json
import math
import statistics
from collections import defaultdict

from plot_ipc import natural_key, plot, suffixed_path, write_plot_csv


def load_mpki(path, core_index=0):
    """Pool phases before dividing; absent cache counters are unavailable, not zero.

    LOAD misses include merged misses and speculative fetches, matching the
    simulator's miss counter. The denominator is retired ROI instructions.
    """
    phases = json.loads(path.read_text())
    if isinstance(phases, dict):
        phases = [phases]
    instructions = 0
    misses = 0
    for phase in phases:
        roi = phase['roi']
        instructions += roi['cores'][core_index]['instructions']
        counts = roi.get(f'cpu{core_index}_L1I', {}).get('LOAD', {}).get('miss')
        if counts is None or len(counts) <= core_index:
            return None
        misses += counts[core_index]
    if instructions <= 0:
        return None
    if not math.isfinite(misses) or misses < 0:
        raise ValueError(f'invalid L1I LOAD miss count: {path}')
    return 1000 * misses / instructions


def summarize_mpki(records, configurations, selected_traces, baselines):
    """Reduction is the ratio of arithmetic mean MPKIs over paired traces.

    A category is N/A if any selected trace lacks counters, or its baseline
    mean MPKI is zero. Union-mode reductions use only matched trace pairs.
    """
    categories = ['overall', *sorted({entry['group'] for config in configurations
                  for trace, entry in records[config].items() if trace in selected_traces}, key=natural_key)]
    absolute, reduction = defaultdict(dict), defaultdict(dict)
    for config in configurations:
        for category in categories:
            entries = {trace: entry for trace, entry in records[config].items()
                       if trace in selected_traces and (category == 'overall' or entry['group'] == category)}
            values = [entry['mpki'] for entry in entries.values()]
            absolute[config][category] = dict(
                mean=statistics.mean(values) if values and all(v is not None for v in values) else None,
                count=len(values))
            base = baselines.get(config)
            pairs = [(entry['mpki'], records[base][trace]['mpki']) for trace, entry in entries.items()
                     if base in records and trace in records[base]]
            value = None
            if pairs and all(a is not None and b is not None for a, b in pairs):
                baseline_mean = statistics.mean(b for _, b in pairs)
                if baseline_mean > 0:
                    value = 100 * (1 - statistics.mean(a for a, _ in pairs) / baseline_mean)
            reduction[config][category] = dict(mean=value, count=len(pairs))
    return categories, absolute, reduction


def plot_horizontal(summary, configurations, categories, output, labels, ylabel):
    """Keep long design names readable in storage and ideal-front-end studies."""
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(12, max(5, .38 * len(configurations))), constrained_layout=True)
    width = .8 / len(categories)
    for index, category in enumerate(categories):
        values = [summary[config][category]['mean'] for config in configurations]
        positions = [i + (index - (len(categories) - 1) / 2) * width for i in range(len(configurations))]
        ax.barh(positions, [v if v is not None else float('nan') for v in values], height=width, label=category)
        for position, value in zip(positions, values):
            if value is None:
                ax.text(0, position, 'N/A', fontsize=6, va='center')
    ax.set_yticks(range(len(configurations)), [labels.get(c, c) for c in configurations])
    ax.invert_yaxis()
    ax.axvline(0, color='black', linewidth=.8)
    ax.set_xlabel(ylabel)
    ax.set_title('L1I load misses · equal weight per trace · N/A if unavailable or undefined')
    ax.grid(axis='x', alpha=.2)
    ax.set_axisbelow(True)
    ax.legend()
    fig.savefig(output, dpi=180)
    plt.close(fig)
    write_plot_csv(summary, configurations, categories, suffixed_path(output, '_plot').with_suffix('.csv'), ylabel)


def write_mpki_outputs(records, configurations, selected_traces, baselines, output, *, labels=None, title=None):
    """Write absolute/reduction figures and exact aggregate/per-trace CSVs.

    Call after the caller's existing plot has initialized Matplotlib.
    """
    measured = {config: {trace: {**entry, 'mpki': load_mpki(entry['path'], entry.get('core', 0))}
                         for trace, entry in records[config].items() if trace in selected_traces}
                for config in configurations}
    categories, absolute, reduction = summarize_mpki(measured, configurations, selected_traces, baselines)
    for suffix, summary, metric in [('_mpki', absolute, 'Mean L1I LOAD MPKI'),
                                    ('_mpki_reduction', reduction, 'L1I LOAD MPKI reduction (%)')]:
        destination = suffixed_path(output, suffix)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if labels is None:
            plot(summary, configurations, categories, destination,
                 title or 'L1I load misses · equal weight per trace · N/A if unavailable or undefined', metric,
                 reference_line=0 if suffix.endswith('reduction') else None,
                 center_on_reference=suffix.endswith('reduction'))
        else:
            plot_horizontal(summary, configurations, categories, destination, labels, metric)
        print(f'MPKI plot: {destination}')
    with suffixed_path(output, '_mpki').with_suffix('.csv').open('w', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['configuration', 'trace_group', 'trace', 'mpki', 'baseline', 'baseline_mpki',
                         'mpki_reduction_percent', 'source'])
        for config in configurations:
            base = baselines.get(config)
            for trace, entry in sorted(measured[config].items(), key=lambda item: natural_key(item[0])):
                baseline_mpki = measured.get(base, {}).get(trace, {}).get('mpki')
                value = entry['mpki']
                reduction_value = 100 * (1 - value / baseline_mpki) if value is not None and baseline_mpki else None
                writer.writerow([config, entry['group'], trace, value, base, baseline_mpki, reduction_value, entry['path']])
    with suffixed_path(output, '_mpki_baselines').with_suffix('.csv').open('w', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['configuration', 'baseline'])
        writer.writerows((config, baselines.get(config)) for config in configurations)


def experiment_mpki(results, individual, baselines, output, labels):
    """Adapt strict experiment collectors without changing their IPC exports."""
    from plot_ipc import collect_results
    records, _ = collect_results(results.resolve(), 0)
    configs = list(dict.fromkeys(row['configuration'] for row in individual))
    traces = {row['trace'] for row in individual}
    # These callers already enforce identical trace coverage and complete logs.
    for extension in ('.png', '.pdf'):
        write_mpki_outputs(records, configs, traces, baselines, output.with_suffix(extension), labels=labels)
