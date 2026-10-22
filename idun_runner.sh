#!/usr/bin/env bash
#SBATCH --job-name=run-experiment
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=5G
#SBATCH --time=1-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=FAIL
#SBATCH --output=/cluster/work/romankb/slurm-run-experiment-%A_%a.out
#SBATCH --error=/cluster/work/romankb/slurm-run-experiment-%A_%a.err

set -euo pipefail

if [[ -n ${CHAMPSIM_REPO_ROOT:-} ]]; then
  repo_root=${CHAMPSIM_REPO_ROOT}
elif [[ -n ${SLURM_SUBMIT_DIR:-} ]]; then
  repo_root=${SLURM_SUBMIT_DIR}
else
  repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
fi
trace_root=${TRACE_ROOT:-/cluster/work/romankb/dataset/IPC1_new_translated}
warmup_instructions=${WARMUP_INSTRUCTIONS:-50000000}
simulation_instructions=${SIMULATION_INSTRUCTIONS:-50000000}
max_parallel=${ARRAY_MAX_CONCURRENT:-16}

trace_groups=(spec server client)

if (( $# != 1 )); then
  echo "Usage: $0 <binary-directory>" >&2
  exit 2
fi

experiment_name=$1
if [[ ! ${experiment_name} =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "Invalid binary directory name: ${experiment_name}" >&2
  exit 2
fi

binary_dir="${repo_root}/bin/${experiment_name}"
result_root=${RESULT_ROOT:-/cluster/work/romankb/results/ipc1/${experiment_name}}

if command -v module >/dev/null 2>&1; then
  module load GCC/12.2.0
fi

if [[ ! -d "${trace_root}" ]]; then
  echo "IPC1 trace root does not exist: ${trace_root}" >&2
  exit 1
fi

if [[ ! -d ${binary_dir} ]]; then
  echo "Binary directory does not exist: ${binary_dir}" >&2
  exit 1
fi

mapfile -d '' binaries < <(find "${binary_dir}" -maxdepth 1 -type f -perm -u+x -print0 | sort -z)
if (( ${#binaries[@]} == 0 )); then
  echo "No executable binaries found in ${binary_dir}" >&2
  exit 1
fi

trace_dirs=()
for group in "${trace_groups[@]}"; do
  trace_dir="${trace_root}/${group}"
  if [[ ! -d ${trace_dir} ]]; then
    echo "Missing IPC1 trace group: ${trace_dir}" >&2
    exit 1
  fi
  trace_dirs+=("${trace_dir}")
done

mapfile -d '' traces < <(find "${trace_dirs[@]}" -maxdepth 1 -type f -name '*.champsimtrace.xz' -print0 | sort -z)
if (( ${#traces[@]} == 0 )); then
  echo "No IPC1 traces found below ${trace_root}" >&2
  exit 1
fi

total_simulations=$((${#binaries[@]} * ${#traces[@]}))

# Outside an array allocation, this script acts as the submission frontend.
# Each array element below re-enters the script with SLURM_ARRAY_TASK_ID set.
if [[ -z ${SLURM_ARRAY_TASK_ID:-} ]]; then
  if [[ -n ${SLURM_JOB_ID:-} ]]; then
    echo "Run this script directly (not through sbatch); it submits its own array job." >&2
    exit 2
  fi
  if [[ ! ${max_parallel} =~ ^[1-9][0-9]*$ ]]; then
    echo "ARRAY_MAX_CONCURRENT must be a positive integer: ${max_parallel}" >&2
    exit 2
  fi

  array_spec="0-$((total_simulations - 1))%${max_parallel}"
  echo "Submitting ${total_simulations} simulations as array ${array_spec} (${#binaries[@]} binaries x ${#traces[@]} traces)."
  exec sbatch \
    --array="${array_spec}" \
    --job-name="run-${experiment_name}" \
    --chdir="${repo_root}" \
    --export="ALL,CHAMPSIM_REPO_ROOT=${repo_root}" \
    "${repo_root}/idun_runner.sh" "${experiment_name}"
fi

if [[ ! ${SLURM_ARRAY_TASK_ID} =~ ^[0-9]+$ ]] || (( SLURM_ARRAY_TASK_ID >= total_simulations )); then
  echo "Invalid array task ID ${SLURM_ARRAY_TASK_ID}; expected 0-$((total_simulations - 1))." >&2
  exit 2
fi

binary_index=$((SLURM_ARRAY_TASK_ID / ${#traces[@]}))
trace_index=$((SLURM_ARRAY_TASK_ID % ${#traces[@]}))
binary=${binaries[${binary_index}]}
trace=${traces[${trace_index}]}

binary_name=$(basename -- "${binary}")
group=$(basename -- "$(dirname -- "${trace}")")
trace_file=$(basename -- "${trace}")
trace_name=${trace_file%.champsimtrace.xz}
output_dir="${result_root}/${binary_name}/${group}"
mkdir -p "${output_dir}"

echo "Array task ${SLURM_ARRAY_TASK_ID}/${total_simulations}: ${binary_name} ${group}/${trace_file}"
"${binary}" \
  --warmup-instructions "${warmup_instructions}" \
  --simulation-instructions "${simulation_instructions}" \
  --json "${output_dir}/${trace_name}.json" \
  "${trace}" \
  >"${output_dir}/${trace_name}.log" \
  2>"${output_dir}/${trace_name}.err"
echo "Completed ${binary_name} ${group}/${trace_file}."
