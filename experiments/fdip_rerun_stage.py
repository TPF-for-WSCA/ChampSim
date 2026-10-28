#!/usr/bin/env python3
"""Execute a frozen FDIP rerun stage; called by the dependent Slurm jobs."""
import csv
import hashlib
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

stage, run_arg = sys.argv[1:]
run = Path(run_arg).resolve()
manifest = json.loads((run / 'manifest.json').read_text())
source = Path(manifest['source'])
repo = run.parents[1]
names = manifest['configurations']
traces = manifest['traces']


def execute(args):
    subprocess.run([str(v) for v in args], cwd=source, check=True)


if stage == 'build':
    execute([sys.executable, 'config.sh', '--join', 'chain', *sorted((run / 'configs').glob('*.json'))])
    execute(['make', '-j', os.environ.get('SLURM_CPUS_PER_TASK', '8'), *['bin/fdip_icache/' + n for n in names]])
    hashes = {name: hashlib.sha256((source / 'bin/fdip_icache' / name).read_bytes()).hexdigest() for name in names}
    (run / 'binary_sha256.json').write_text(json.dumps(hashes, indent=2) + '\n')
    print(f'Built and hashed {len(names)} binaries', flush=True)
elif stage == 'run':
    index = int(os.environ['SLURM_ARRAY_TASK_ID'])
    if index < 0 or index >= len(names) * len(traces):
        raise ValueError(f'invalid array index {index}')
    name, trace = names[index // len(traces)], Path(traces[index % len(traces)])
    out = run / 'results' / name / trace.parent.name
    out.mkdir(parents=True, exist_ok=True)
    stem = trace.name.removesuffix('.champsimtrace.xz')
    with (out / (stem + '.log')).open('w') as log, (out / (stem + '.err')).open('w') as err:
        subprocess.run([str(source / 'bin/fdip_icache' / name), '--warmup-instructions', str(manifest['warmup_instructions']),
                        '--simulation-instructions', str(manifest['simulation_instructions']),
                        '--json', str(out / (stem + '.json')), str(trace)], stdout=log, stderr=err, check=True)
    if 'Simulation complete' not in (out / (stem + '.log')).read_text():
        raise ValueError(f'incomplete run: {name}/{stem}')
    print(f'Completed {index}: {name}/{stem}', flush=True)
elif stage == 'plot':
    # Never publish presentation figures from partial/mixed result sets.
    diagnostics = []
    for name in names:
        values = []
        expected = set()
        for trace_str in traces:
            trace = Path(trace_str)
            path = run / 'results' / name / trace.parent.name / (trace.name.removesuffix('.champsimtrace.xz') + '.json')
            expected.add(path)
            phases = json.loads(path.read_text())
            if len(phases) != 1 or phases[0]['traces'] != [str(trace)]:
                raise ValueError(f'unexpected trace/phase: {path}')
            roi = phases[0]['roi']
            core, cache = roi['cores'][0], roi['cpu0_L1I']
            if core['instructions'] < manifest['simulation_instructions'] or core['cycles'] <= 0:
                raise ValueError(f'incomplete ROI: {path}')
            text = path.with_suffix('.log').read_text()
            if 'Simulation complete' not in text or not re.search(r'^cpu0_L1I .*STORAGE', text, re.M):
                raise ValueError(f'missing completion/storage report: {path}')
            n = core['instructions']
            values.append(dict(ipc=n/core['cycles'], demand_mpki=1000*sum(cache['LOAD']['miss'])/n,
                               partial_mpki=1000*sum(cache['LOAD']['partial_miss'])/n,
                               miss_latency=cache['miss latency'], useful_prefetch_pki=1000*cache['useful prefetch']/n))
        actual = set((run / 'results' / name).rglob('*.json'))
        if actual != expected:
            raise ValueError(f'unexpected results for {name}')
        diagnostics.append(dict(configuration=name, traces=len(values), harmonic_ipc=statistics.harmonic_mean(v['ipc'] for v in values),
                                **{key: statistics.mean(v[key] for v in values) for key in values[0] if key != 'ipc'}))
    with (run / 'diagnostics.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(diagnostics[0]))
        writer.writeheader()
        writer.writerows(diagnostics)
    plots = run / 'plots'
    plots.mkdir(exist_ok=True)
    execute([sys.executable, 'aggregation_scripts/plot_ipc.py', run / 'results', '--output', plots / 'ubs_ipc.pdf',
             '--baseline', 'fdip_icache_16k', '--title', 'Instruction-cache performance (fresh rerun)'])
    execute([sys.executable, 'aggregation_scripts/plot_ubs_presentation.py', '--input', plots / 'ubs_ipc_relative_plot.csv',
             '--output', run / 'presentation/ubs_ipc_relative'])
    # Publish only once the entire experiment and both plotting steps pass.
    for path in plots.iterdir():
        shutil.copy2(path, repo / path.name)
        shutil.copy2(path, repo / path.name.replace('ubs_ipc', 'fdip_icache_ipc', 1))
    for path in (run / 'presentation').iterdir():
        shutil.copy2(path, repo / 'presentation' / path.name)
    (run / 'COMPLETE').write_text('All 650 simulations validated; presentation plots and CSVs published.\n')
    print((run / 'COMPLETE').read_text(), flush=True)
else:
    raise ValueError(stage)
