#!/bin/bash
# Sample call to ChampSim with context-switch simulation
# This demonstrates the context-switch simulation feature where:
# 1. Warmup phase runs with the first trace (provided in config)
# 2. Simulation phase starts with warmup trace for specified CPUs
# 3. After N instructions, switches to the context-switch trace

# Example with typical parameters:
./bin/champsim \
  --trace-file traces/spec2k6/401.bzip2-277B.trace.xz \
  --warmup-instructions 100000000 \
  --simulation-instructions 1000000000 \
  --context-switch-trace traces/spec2k6/429.mcf-184B.trace.xz \
  --context-switch-cpus 0 1 \
  champsim_mac_config.json

# Explanation:
#   --trace-file: Primary trace used for warmup phase
#   --warmup-instructions: Instructions to execute during warmup
#   --simulation-instructions: Instructions to execute during simulation (before context switch)
#   --context-switch-trace: Secondary trace to switch to for specified CPUs
#   --context-switch-cpus: CPU indices (0 and 1) that will switch to the new trace
#   
# The simulation will:
#   1. Load both traces
#   2. Run warmup with the primary trace
#   3. Run simulation phase, starting with primary trace for warmup
#   4. CPUs 0 and 1 will switch to the context-switch-trace mid-simulation
#   5. Report statistics for both phases

