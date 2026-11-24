#!/usr/bin/env bash
set -euo pipefail

if (( $# != 5 )); then
  echo "Usage: $0 <build-dir> <DRAMPower-memspec.json> <results-dir> <trace-manifest> <trace-root>" >&2
  exit 2
fi

export LC_ALL=C
configs=("$1"/IDUN_CONFIGS/harcom_btb_ipc1/*.json)
mapfile -t traces < "$4"
task=${SLURM_ARRAY_TASK_ID:?Set SLURM_ARRAY_TASK_ID}
trace_count=${#traces[@]}
if ! [[ $task =~ ^(0|[1-9][0-9]*)$ ]] || (( task >= ${#configs[@]} * trace_count )); then
  echo "Invalid IPC1 array task index: $task" >&2
  exit 2
fi

if command -v module >/dev/null 2>&1; then
  module load "${GCC_MODULE:-GCCcore/14.2.0}"
fi

config=${configs[task / trace_count]##*/}
name=${config%.json}
trace=${traces[task % trace_count]}
output="$3/$name/${trace%.champsimtrace.xz}"
mkdir -p -- "$(dirname -- "$output")"
temporary_json="$output.partial.json"
# Use an atomic move so the summary sees only completed result files.
"$1/bin/harcom_btb_ipc1/$name" -w 50000000 -i 50000000 \
  --drampower-memspec "$2" --json="$temporary_json" "$5/$trace" \
  > "$output.log" 2>&1
mv -- "$temporary_json" "$output.json"
