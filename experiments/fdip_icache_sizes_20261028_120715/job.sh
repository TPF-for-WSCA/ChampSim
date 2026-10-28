#!/usr/bin/env bash
set -euo pipefail
stage=$1
run_dir=$2
if command -v module >/dev/null 2>&1; then
  module load GCC/12.2.0
  module load Python/3.10.8-GCCcore-12.2.0
fi
exec python3 "${run_dir}/stage.py" "${stage}" "${run_dir}"
