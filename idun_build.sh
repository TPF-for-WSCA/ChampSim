#!/usr/bin/env bash
#SBATCH --job-name=build-experiment
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem-per-cpu=2G
#SBATCH --time=04:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=FAIL
#SBATCH --output=/cluster/work/romankb/slurm-build-experiment-%j.out
#SBATCH --error=/cluster/work/romankb/slurm-build-experiment-%j.err

set -euo pipefail

if [[ -n ${CHAMPSIM_REPO_ROOT:-} ]]; then
  repo_root=${CHAMPSIM_REPO_ROOT}
elif [[ -n ${SLURM_SUBMIT_DIR:-} ]]; then
  repo_root=${SLURM_SUBMIT_DIR}
else
  repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
fi
build_jobs=${SLURM_CPUS_PER_TASK:-8}

if (( $# != 1 )); then
  echo "Usage: $0 <config-directory>" >&2
  exit 2
fi

experiment_name=$1
if [[ ! ${experiment_name} =~ ^[A-Za-z0-9._-]+$ ]]; then
  echo "Invalid config directory name: ${experiment_name}" >&2
  exit 2
fi

config_dir="${repo_root}/IDUN_CONFIGS/${experiment_name}"
if [[ ! -d ${config_dir} ]]; then
  echo "Config directory does not exist: ${config_dir}" >&2
  exit 1
fi

mapfile -d '' configs < <(find "${config_dir}" -maxdepth 1 -type f -name '*.json' -print0 | sort -z)
if (( ${#configs[@]} == 0 )); then
  echo "No JSON configs found in ${config_dir}" >&2
  exit 1
fi

# Outside a Slurm allocation, this script acts as the submission frontend.
# All configurations are built sequentially within the submitted job because
# concurrent ChampSim builds would interfere with one another in the worktree.
if [[ -z ${SLURM_JOB_ID:-} ]]; then
  echo "Submitting one Slurm job to build ${#configs[@]} configuration(s) sequentially."
  exec sbatch \
    --job-name="build-${experiment_name}" \
    --chdir="${repo_root}" \
    --export="ALL,CHAMPSIM_REPO_ROOT=${repo_root}" \
    "${repo_root}/idun_build.sh" "${experiment_name}"
fi

if command -v module >/dev/null 2>&1; then
  module load GCC/12.2.0
  module load Python/3.10.8-GCCcore-12.2.0
fi

cd "${repo_root}"

for config in "${configs[@]}"; do
  echo "Configuring $(basename -- "${config}")"
  ./config.sh "${config}"

  echo "Building $(basename -- "${config}")"
  env -u MAKEFLAGS -u MFLAGS make -j "${build_jobs}"
done

echo "Built ${#configs[@]} configuration(s) from ${config_dir}."
