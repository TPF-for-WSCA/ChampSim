#!/usr/bin/env bash
# Submit only when explicitly invoked with build or run.
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
experiment=storage_dse_ubs_fixed4

case ${1:-} in
  build)
    exec "${repo_root}/idun_build.sh" "${experiment}"
    ;;
  run)
    # Keep original results intact; expose unchanged designs to the plotter.
    original=${ORIGINAL_RESULT_ROOT:-/cluster/work/romankb/results/ipc1/storage_dse}
    destination=${RESULT_ROOT:-/cluster/work/romankb/results/ipc1/${experiment}}
    original=$(realpath -e -- "${original}")
    destination=$(realpath -m -- "${destination}")
    if [[ ${original} == "${destination}" ]]; then
      echo "RESULT_ROOT must differ from ORIGINAL_RESULT_ROOT" >&2
      exit 1
    fi
    # Validate all reused results before creating links or submitting work.
    for config in "${repo_root}"/IDUN_CONFIGS/storage_dse/{baseline,litebtb}_*.json; do
      name=$(basename -- "${config}" .json)
      if [[ ! -d ${original}/${name} ]]; then
        echo "Missing original results: ${original}/${name}" >&2
        exit 1
      fi
      if [[ -e ${destination}/${name} || -L ${destination}/${name} ]]; then
        if [[ ! -L ${destination}/${name} ]] || [[ $(readlink -f -- "${destination}/${name}") != "${original}/${name}" ]]; then
          echo "Refusing to replace existing results: ${destination}/${name}" >&2
          exit 1
        fi
      fi
    done
    mkdir -p -- "${destination}"
    for config in "${repo_root}"/IDUN_CONFIGS/storage_dse/{baseline,litebtb}_*.json; do
      name=$(basename -- "${config}" .json)
      if [[ ! -L ${destination}/${name} ]]; then
        ln -s -- "${original}/${name}" "${destination}/${name}"
      fi
    done
    export RESULT_ROOT=${destination}
    exec "${repo_root}/idun_runner.sh" "${experiment}"
    ;;
  *)
    echo "Usage: $0 {build|run}" >&2
    echo "Invoke build first; invoke run after the build job succeeds." >&2
    exit 2
    ;;
esac
