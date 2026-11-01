#!/usr/bin/env python3
"""Plot geometric mean per-trace ROI IPC speedup over the SOTA baseline."""
import argparse
import itertools
import json
import math
import statistics
import sys
from pathlib import Path

from plot_ipc import load_result
from plot_storage_dse import write_csv

STRUCTURES = ('icache', 'cbp', 'ibp', 'btb', 'itlb')


def configurations():
    yield 'baseline', 'Baseline'
    for count in range(1, len(STRUCTURES) + 1):
        for combination in itertools.combinations(STRUCTURES, count):
            name = 'ideal_' + '_'.join(combination)
            label = ' + '.join({'icache': 'I-cache', 'cbp': 'CBP', 'ibp': 'IBP', 'btb': 'BTB', 'itlb': 'ITLB'}[x] for x in combination)
            if combination == ('cbp', 'ibp', 'btb'):
                label = 'Perfect BPU'
            if combination == ('icache', 'cbp', 'ibp', 'btb'):
                label = 'I-cache + perfect BPU'
            if combination == ('cbp', 'ibp', 'btb', 'itlb'):
                label = 'Perfect BPU + ITLB'
            if count == len(STRUCTURES):
                label = 'I-cache + perfect BPU + ITLB'
            yield name, label


def collect(results, allow_incomplete=False):
    records = {}
    for name, label in configurations():
        paths = sorted((results / name).rglob('*.json'))
        if not paths:
            if allow_incomplete and name != 'baseline':
                print(f'Skipping missing configuration: {name}', file=sys.stderr)
                continue
            raise ValueError(f'no results for {name}')
        traces = {}
        for path in paths:
            trace, group, ipc = load_result(path, 0)
            if not math.isfinite(ipc) or ipc <= 0:
                raise ValueError(f'invalid IPC: {path}')
            if trace in traces:
                raise ValueError(f'duplicate trace {trace} for {name}')
            log = path.with_suffix('.log')
            if not log.exists() or 'Simulation complete' not in log.read_text():
                raise ValueError(f'missing or incomplete simulation log: {log}')
            traces[trace] = (group, ipc)
        records[name] = (label, traces)
    baseline = records['baseline'][1]
    summary, individual = [], []
    for name, (label, traces) in records.items():
        if set(traces) != set(baseline):
            raise ValueError(f'unbalanced trace coverage for {name}; use identical trace sets')
        ratios = []
        for trace in sorted(traces):
            group, ipc = traces[trace]
            base_ipc = baseline[trace][1]
            speedup = ipc / base_ipc
            ratios.append(speedup)
            individual.append(dict(configuration=name, trace=trace, group=group, ipc=ipc,
                                   baseline_ipc=base_ipc, speedup=speedup))
        speedup = statistics.geometric_mean(ratios)
        summary.append(dict(configuration=name, label=label, trace_count=len(traces),
                            speedup=speedup, improvement_percent=100 * (speedup - 1)))
    return summary, individual


def plot(summary, output, group=None):
    import os
    import tempfile
    os.environ.setdefault('MPLCONFIGDIR', str(Path(tempfile.gettempdir()) / 'champsim-ideal-matplotlib'))
    try:
        import importlib_resources
    except ModuleNotFoundError:
        from importlib import resources
        sys.modules['importlib_resources'] = resources
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(12, max(6, len(summary) * 0.36)), layout='constrained')
    colors = ['#777777' if r['configuration'] == 'baseline' else
              '#009E73' if r['configuration'] in ('ideal_icache_cbp_ibp_btb', 'ideal_icache_cbp_ibp_btb_itlb') else
              '#D55E00' if r['configuration'] == 'ideal_cbp_ibp_btb' else '#0072B2' for r in summary]
    bars = ax.barh(range(len(summary)), [r['speedup'] for r in summary], color=colors)
    ax.axvline(1, color='black', linewidth=1, linestyle='--')
    ax.set_yticks(range(len(summary)), [r['label'] for r in summary])
    ax.invert_yaxis()
    ax.bar_label(bars, labels=[f"{r['speedup']:.3f}×" for r in summary], padding=3, fontsize=8)
    ax.set_xlabel('Geometric mean ROI IPC speedup over baseline')
    ax.set_title(f"Ideal front-end study · {group or 'all workloads'} · {summary[0]['trace_count']} traces · equal weight per trace")
    ax.set_xlim(0, max(r['speedup'] for r in summary) * 1.15)
    ax.grid(axis='x', alpha=0.2)
    ax.set_axisbelow(True)
    for suffix in ('.png', '.pdf'):
        fig.savefig(output.with_suffix(suffix), dpi=180)
    plt.close(fig)


def summarize_groups(summary, individual):
    """Paired speedups, normalized and averaged independently within each group."""
    result = []
    for group in sorted({row['group'] for row in individual}):
        for item in summary:
            ratios = [row['speedup'] for row in individual
                      if row['group'] == group and row['configuration'] == item['configuration']]
            speedup = statistics.geometric_mean(ratios)
            result.append(dict(group=group, configuration=item['configuration'], label=item['label'],
                               trace_count=len(ratios), speedup=speedup, improvement_percent=100 * (speedup - 1)))
    return result


def group_diagnostics(results, summary):
    """Aggregate raw ROI counters per workload group; do not infer missing counters."""
    rows = []
    for item in summary:
        grouped = {}
        for path in (results / item['configuration']).rglob('*.json'):
            _, group, _ = load_result(path, 0)
            grouped.setdefault(group, []).extend(phase['roi']['cores'][0] for phase in json.loads(path.read_text()))
        for group, cores in sorted(grouped.items()):
            instructions = sum(core['instructions'] for core in cores)
            cycles = sum(core['cycles'] for core in cores)
            def total(key):
                return sum(core[key] for core in cores) if all(key in core for core in cores) else None
            def ratio(key, denominator, multiplier=1):
                value = total(key)
                return multiplier * value / denominator if value is not None and denominator else None
            btb_accesses = sum(sum(core.get('btb accesses', {}).values()) for core in cores)
            btb_misses = sum(sum(core.get('btb misses', {}).values()) for core in cores)
            redirects = total('branch_redirects')
            rows.append(dict(configuration=item['configuration'], group=group,
                             branch_mpki=1000 * sum(sum(core['mispredict'].values()) for core in cores) / instructions
                                 if all('mispredict' in core for core in cores) else None,
                             btb_mpki=1000 * btb_misses / instructions if btb_accesses else None,
                             btb_miss_rate=btb_misses / btb_accesses if btb_accesses else None,
                             mean_redirect_cycles=ratio('branch_redirect_cycles', redirects),
                             recovery_cycle_fraction=ratio('fetch_recovery_cycles', cycles),
                             frontend_empty_fraction=ratio('frontend_empty_cycles', cycles),
                             rob_empty_fraction=ratio('rob_empty_cycles', cycles),
                             fdip_discontinuities=total('fdip_discontinuities'),
                             fdip_blocks=total('fdip_fetch_blocks'),
                             fdip_ftq_full_cycles=total('fdip_ftq_full_cycles'),
                             wrong_path_fetched=total('wrong_path_fetched'),
                             wrong_path_unknown=total('wrong_path_unknown'),
                             wrong_path_unknown_percent=ratio('wrong_path_unknown', total('wrong_path_fetched'), 100),
                             wrong_path_unknown_per_ki=ratio('wrong_path_unknown', instructions, 1000)))
    return rows


def plot_wrong_path_unknown(diagnostics, summary, output):
    """Plot pooled unknown/fetched ratios; undefined shares are marked N/A."""
    import matplotlib.pyplot as plt
    labels = {item['configuration']: item['label'] for item in summary}
    groups = sorted({row['group'] for row in diagnostics})
    fig, axes = plt.subplots(1, len(groups), figsize=(6 * len(groups), max(6, len(summary) * 0.36)),
                             squeeze=False, layout='constrained', sharey=True)
    for ax, group in zip(axes[0], groups):
        rows = [row for row in diagnostics if row['group'] == group]
        shares = [row['wrong_path_unknown_percent'] for row in rows]
        bars = ax.barh(range(len(rows)), [value if value is not None else 0 for value in shares], color='#0072B2')
        ax.bar_label(bars, labels=[f'{value:.1f}%' if value is not None else 'N/A' for value in shares], padding=3, fontsize=8)
        ax.set_yticks(range(len(rows)), [labels[row['configuration']] for row in rows])
        if not ax.yaxis_inverted():
            ax.invert_yaxis()
        ax.set_xlim(0, 115)
        ax.set_xticks(range(0, 101, 25))
        ax.set_xlabel('Unknown / fetched wrong-path instructions (%)')
        ax.set_title(group)
        ax.grid(axis='x', alpha=0.2)
        ax.set_axisbelow(True)
    fig.suptitle('Wrong-path instructions missing from replay history · pooled ROI counts\nN/A: no wrong-path instructions or unavailable counters')
    for suffix in ('.png', '.pdf'):
        fig.savefig(output.with_suffix(suffix), dpi=180)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', type=Path, help='runner result directory containing baseline/ and ideal_*/')
    parser.add_argument('--output', type=Path, default=Path('ideal_frontend'))
    parser.add_argument('--allow-incomplete', action='store_true', help='skip missing variants; retain identical trace coverage')
    args = parser.parse_args()
    try:
        summary, individual = collect(args.results, args.allow_incomplete)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write_csv(args.output.with_suffix('.csv'), summary)
        write_csv(args.output.with_name(args.output.name + '_per_trace.csv'), individual)
        plot(summary, args.output)
        groups = summarize_groups(summary, individual)
        write_csv(args.output.with_name(args.output.name + '_by_group.csv'), groups)
        diagnostics = group_diagnostics(args.results, summary)
        write_csv(args.output.with_name(args.output.name + '_diagnostics_by_group.csv'), diagnostics)
        plot_wrong_path_unknown(diagnostics, summary, args.output.with_name(args.output.name + '_wrong_path_unknown'))
        for group in sorted({row['group'] for row in groups}):
            plot([row for row in groups if row['group'] == group], args.output.with_name(args.output.name + '_' + group), group)
    except (ValueError, OSError, KeyError, TypeError, ImportError) as error:
        parser.exit(1, f'{error}\n')
    print(f'Wrote {args.output}.[csv,png,pdf] and per-trace CSV')


if __name__ == '__main__':
    main()
