#!/usr/bin/env python3
"""Freeze sources/configurations/traces and prepare an isolated Slurm rerun."""
import hashlib
import json
import shutil
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

repo = Path(__file__).resolve().parents[1]
run = repo / 'experiments' / ('fdip_icache_sizes_' + datetime.now(ZoneInfo('Europe/Oslo')).strftime('%Y%m%d_%H%M%S'))
source = run / 'source'
source.mkdir(parents=True)
ignore = shutil.ignore_patterns('*.o', '*.d', '__pycache__', 'compile_commands.json')
for name in ('inc', 'src', 'config', 'btb', 'branch', 'prefetcher', 'replacement', 'test', 'aggregation_scripts'):
    shutil.copytree(repo / name, source / name, ignore=ignore)
for name in ('Makefile', 'config.sh', 'global.options', 'module.options'):
    shutil.copy2(repo / name, source / name)
(source / 'vcpkg_installed').symlink_to(repo / 'vcpkg_installed', target_is_directory=True)
configs = run / 'configs'
configs.mkdir()
names = []
for path in sorted((repo / 'IDUN_CONFIGS/fdip_icache_sizes').glob('*.json')):
    shutil.copy2(path, configs / path.name)
    names.append(Path(json.loads(path.read_text())['executable_name']).name)
trace_root = Path('/cluster/work/romankb/dataset/IPC1_new_translated')
traces = sorted(str(p) for group in ('spec', 'server', 'client') for p in (trace_root / group).glob('*.champsimtrace.xz'))
assert len(names) == 13 and len(traces) == 50, (len(names), len(traces))
manifest = dict(run=str(run), source=str(source), configurations=names, traces=traces,
                warmup_instructions=50000000, simulation_instructions=50000000)
(run / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
hashes = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in source.rglob('*') if p.is_file() and 'vcpkg_installed' not in p.parts}
(run / 'source_sha256.json').write_text(json.dumps(hashes, indent=2) + '\n')
(run / 'logs').mkdir()
print(run)
