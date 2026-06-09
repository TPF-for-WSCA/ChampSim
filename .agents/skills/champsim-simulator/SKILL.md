---
name: champsim-simulator
description: Work with the ChampSim trace-based microarchitecture simulator in this repository. Use when Codex needs to configure, build, run, debug, or validate ChampSim; edit branch predictors, BTBs, cache prefetchers, replacement policies, simulator core code, JSON experiment configs, trace workflows, SAGA/IDUN experiment scripts, or ChampSim result parsing; or reason about microarchitecture experiments implemented in ChampSim.
---

# ChampSim Simulator

## Workflow

Start by reading the local repository shape before changing code. Prefer `rg --files`, `rg`, and nearby examples in `branch/`, `btb/`, `prefetcher/`, `replacement/`, `inc/`, `src/`, `config/`, `test/`, `SAGA_CONFIGS/`, and `IDUN_CONFIGS/`.

For concrete commands, paths, module contracts, and validation guidance, read `references/champsim-workflows.md` when the task involves implementation, running experiments, or interpreting simulator output.

## Operating Rules

- Treat ChampSim as a generated-build simulator: JSON config plus `./config.sh` produce build files under `.csconfig/` and binaries under `bin/`.
- Preserve the user's experiment artifacts. Do not delete traces, `results_*`, generated configs, `.csconfig/`, or binaries unless explicitly asked.
- Keep changes scoped to the relevant module or config. Avoid broad simulator refactors when implementing one branch predictor, BTB, prefetcher, or replacement-policy experiment.
- Prefer adding a new module directory over rewriting a baseline module when the user asks for a new design.
- Validate JSON configs structurally before building, and keep names stable because scripts often key off config and executable names.
- For simulation commands, use short warmup/simulation instruction counts for smoke tests unless the user explicitly requests research-scale runs.
- When traces are required but absent, validate configuration/build first and report the missing trace as the remaining blocker.

## Common Task Flow

1. Identify the target surface: config-only change, module algorithm, simulator core, trace generation, or result analysis.
2. Inspect the closest existing example and the active JSON config.
3. Make the smallest code/config change that fits the experiment.
4. Run `./config.sh <config.json>` and `make` when build validation is appropriate.
5. Run a short simulation only if a trace path is available or provided.
6. Summarize the changed experiment knobs, binary/config names, validation commands, and any trace/result limitations.
