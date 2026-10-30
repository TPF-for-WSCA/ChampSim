#!/usr/bin/env bash
# Submit a build and a dependent job that discovers binaries and submits runs.
set -euo pipefail

repo_root=${CHAMPSIM_REPO_ROOT:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)}

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
  echo "Usage: $0 <config-directory> [binary-directory]"
  echo "Example: $0 fdip_icache_sizes fdip_icache"
  echo "Directories are relative to IDUN_CONFIGS/ and bin/, respectively."
  exit 0
fi
if (( $# < 1 || $# > 2 )); then
  echo "Usage: $0 <config-directory> [binary-directory]" >&2
  exit 2
fi

config_name=$1
binary_name=${2:-$1}
for name in "${config_name}" "${binary_name}"; do
  if [[ ! ${name} =~ ^[A-Za-z0-9._-]+$ || ${name} == . || ${name} == .. ]]; then
    echo "Invalid experiment directory name: ${name}" >&2
    exit 2
  fi
done
config_dir="${repo_root}/IDUN_CONFIGS/${config_name}"
if [[ ! -d ${config_dir} ]]; then
  echo "Config directory does not exist: ${config_dir}" >&2
  exit 1
fi
mapfile -d '' configs < <(find "${config_dir}" -maxdepth 1 -type f -name '*.json' -print0)
if (( ${#configs[@]} == 0 )); then
  echo "No JSON configs found in ${config_dir}" >&2
  exit 1
fi

# Use the existing build resource requests. Builds in this worktree must not
# overlap; this invocation submits one build for the entire configuration set.
build_id=$(sbatch --parsable \
  --job-name="build-${config_name}" \
  --chdir="${repo_root}" \
  --export="ALL,CHAMPSIM_REPO_ROOT=${repo_root}" \
  "${repo_root}/idun_build.sh" "${config_name}")
build_id=${build_id%%;*}
echo "Submitted build job ${build_id}."

# The array size is only known after the binaries exist. This short job submits
# the array using idun_runner.sh, which retains its own simulation resources.
printf -v run_command 'exec bash %q --submit %q' "${repo_root}/idun_runner.sh" "${binary_name}"
submit_id=$(sbatch --parsable \
  --dependency="afterok:${build_id}" --kill-on-invalid-dep=yes \
  --job-name="submit-runs-${binary_name}" \
  --account=share-ie-idi --nodes=1 --ntasks=1 --cpus-per-task=1 \
  --mem=1G --time=00:10:00 \
  --output=/cluster/work/romankb/slurm-submit-runs-%j.out \
  --error=/cluster/work/romankb/slurm-submit-runs-%j.err \
  --chdir="${repo_root}" \
  --export="ALL,CHAMPSIM_REPO_ROOT=${repo_root}" \
  --wrap="${run_command}")
submit_id=${submit_id%%;*}
echo "Submitted run-submission job ${submit_id}, waiting for build ${build_id} to succeed."
echo "The run array job ID will appear in /cluster/work/romankb/slurm-submit-runs-${submit_id}.out."
