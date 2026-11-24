#!/usr/bin/env bash
set -euo pipefail

if [[ ${1:-} == --build ]]; then
  cd -- "$2"
  if command -v module >/dev/null 2>&1; then
    module load "${GCC_MODULE:-GCCcore/14.2.0}"
  fi
  ./config.sh --no-compile-all-modules --join chain IDUN_CONFIGS/harcom_btb_ipc1/*.json
  make -j "${SLURM_CPUS_PER_TASK:-8}" CXX=g++ DRAMPOWER_CMAKE="${DRAMPOWER_CMAKE:-/usr/bin/cmake}"
  exit
fi

if (( $# < 1 || $# > 2 )); then
  echo "Usage: $0 [<DRAMPower-memspec.json>] <shared-results-root>" >&2
  exit 2
fi

config_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(git -C "$config_dir" rev-parse --show-toplevel)
if (( $# == 1 )); then
  memspec="$repo/third_party/DRAMSys/configs/memspec/MICRON_4Gb_DDR4-2400_8bit_A.json"
  test -f "$memspec" || { echo "Run git submodule update --init third_party/DRAMSys before using the default memspec." >&2; exit 1; }
  results_root=$1
else
  memspec=$1
  results_root=$2
fi
test -f "$memspec" || { echo "Missing DRAMPower memspec: $memspec" >&2; exit 1; }
trace_root=$(cd -- "${TRACE_ROOT:-/cluster/work/romankb/dataset/IPC1_new_translated}" && pwd)
concurrency=${ARRAY_MAX_CONCURRENT:-128}
if ! [[ $concurrency =~ ^[1-9][0-9]*$ ]]; then
  echo "ARRAY_MAX_CONCURRENT must be a positive integer." >&2
  exit 2
fi
dram_submodules=(third_party/DRAMPower third_party/DRAMUtils)
for submodule in "${dram_submodules[@]}"; do
  test -f "$repo/$submodule/CMakeLists.txt" || { echo "Run git submodule update --init $submodule before submitting." >&2; exit 1; }
done
test -d "$repo/vcpkg_installed" || { echo "Install ChampSim's vcpkg dependencies before submitting." >&2; exit 1; }
mkdir -p -- "$results_root"
experiment=$(mktemp -d "$(cd -- "$results_root" && pwd)/ipc1-btb.XXXXXX")
build="$experiment/build"
mkdir -p -- "$build/third_party"
manifest="$experiment/traces.txt"
find "$trace_root" -type f -name '*.champsimtrace.xz' -printf '%P\n' | LC_ALL=C sort > "$manifest"
trace_count=$(wc -l < "$manifest")
if (( trace_count == 0 )); then
  echo "No IPC1 traces found under $trace_root" >&2
  exit 1
fi

# Capture tracked working-tree sources, including local edits. Use the same
# snapshot for both Slurm jobs.
git -C "$repo" ls-files --cached --exclude-standard -z |
  tar -C "$repo" --exclude=vcpkg --exclude=third_party/DRAMPower --exclude=third_party/DRAMUtils \
    --exclude=third_party/DRAMSys --no-recursion --null -T - -cf - |
  tar -C "$build" -xf -
cp -a -- "$config_dir" "$build/IDUN_CONFIGS/"
ln -sT -- "$repo/vcpkg_installed" "$build/vcpkg_installed"
for submodule in "${dram_submodules[@]}"; do
  ln -sT -- "$repo/$submodule" "$build/$submodule"
done
cp -- "$memspec" "$experiment/memspec.json"

scripts="$build/IDUN_CONFIGS/harcom_btb_ipc1"
configs=("$scripts"/*.json)
tasks=$(( ${#configs[@]} * trace_count ))
common=(--account="${SLURM_ACCOUNT:-share-ie-idi}" --nodes=1 --ntasks=1 --chdir="$experiment" --export=ALL)
build_job=$(sbatch --parsable "${common[@]}" --job-name=build-harcom-btb-ipc1 \
  --cpus-per-task=8 --mem=16G --time=02:00:00 --output="$experiment/build-%j.log" \
  "$scripts/submit.sh" --build "$build")
build_id=${build_job%%;*}
printf 'Build job %s\nOutput directory %s\nTraces %d, configurations %d, runs %d\n' \
  "$build_id" "$experiment" "$trace_count" "${#configs[@]}" "$tasks"

run_job=$(sbatch --parsable "${common[@]}" --dependency="afterok:$build_id" --kill-on-invalid-dep=yes \
  --array="0-$((tasks - 1))%$concurrency" --job-name=run-harcom-btb-ipc1 \
  --cpus-per-task=1 --mem=5G --time=12:00:00 --output="$experiment/run-%A_%a.log" \
  "$scripts/run_sample.sh" "$build" "$experiment/memspec.json" "$experiment/results" "$manifest" "$trace_root")
printf 'Run array %s\n' "${run_job%%;*}"
