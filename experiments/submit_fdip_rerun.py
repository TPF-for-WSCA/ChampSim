#!/usr/bin/env python3
"""Submit a prepared FDIP rerun with success dependencies and record job IDs."""
import json
import subprocess
import sys
from pathlib import Path

run = Path(sys.argv[1]).resolve()
manifest = json.loads((run / 'manifest.json').read_text())
state = run / 'jobs.json'
jobs = json.loads(state.read_text()) if state.exists() else {}
common = ['sbatch', '--parsable', '--account=share-ie-idi', '--nodes=1', '--ntasks=1',
          '--chdir=' + str(run / 'source'), '--kill-on-invalid-dep=yes']


def submit(stage, options):
    if stage not in jobs:
        result = subprocess.run(common + options + ['--job-name=fdip-rerun-' + stage,
            '--output=' + str(run / 'logs' / (stage + '-%A_%a.out')),
            '--error=' + str(run / 'logs' / (stage + '-%A_%a.err')),
            str(run / 'job.sh'), stage, str(run)], check=True, capture_output=True, text=True)
        job_id = result.stdout.strip().split(';')[0]
        if not job_id.isdigit():
            raise ValueError(f'unexpected sbatch response: {result.stdout}')
        jobs[stage] = job_id
        state.write_text(json.dumps(jobs, indent=2) + '\n')
    print(stage + ': ' + jobs[stage], flush=True)
    return jobs[stage]


build = submit('build', ['--cpus-per-task=8', '--mem-per-cpu=2G', '--time=04:00:00'])
count = len(manifest['configurations']) * len(manifest['traces'])
array = submit('run', ['--cpus-per-task=1', '--mem-per-cpu=5G', '--time=1-12:00:00',
                      f'--array=0-{count-1}%32', '--dependency=afterok:' + build])
submit('plot', ['--cpus-per-task=1', '--mem=4G', '--time=00:30:00', '--dependency=afterok:' + array])
