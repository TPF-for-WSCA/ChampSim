#!/usr/bin/env bash
# Run after the complete ideal-frontend simulation array succeeds.
set -euo pipefail
if (( $# != 2 )); then
  echo "Usage: $0 <result-root> <output-prefix>" >&2
  exit 2
fi
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "${repo_root}"
python3 aggregation_scripts/plot_ideal_frontend.py "$1" --output "$2"
