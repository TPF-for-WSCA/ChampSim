# ChampSim Workflows

## Repository Map

- `config.sh`: Python entry point that reads JSON configs and emits generated make/build files.
- `champsim_config.json` and `champsim_*_config.json`: root example configs.
- `SAGA_CONFIGS/` and `IDUN_CONFIGS/`: experiment configuration sets for BTB/frontend studies.
- `branch/<name>/`: branch direction predictor modules.
- `btb/<name>/`: branch target predictor modules, including custom hashed/two-level variants in this fork.
- `prefetcher/<name>/`: data and instruction prefetcher modules.
- `replacement/<name>/`: cache replacement policy modules.
- `inc/`: simulator interfaces and shared types. Check here before guessing a module signature.
- `src/`: simulator core implementation.
- `test/python/`: configuration and generation tests.
- `test/cpp/`: compiled C++ test harness created by `config.sh`.
- `tracer/`: trace-generation utilities.

## Configure And Build

Use the standard flow:

```bash
./config.sh <config.json>
make
```

Useful `config.sh` options:

```bash
./config.sh --bindir <dir> <config.json>
./config.sh --prefix <dir> <config.json>
./config.sh --module-dir <dir> <config.json>
./config.sh --branch-dir <dir> --btb-dir <dir> --prefetcher-dir <dir> --replacement-dir <dir> <config.json>
./config.sh --compile-all-modules <config.json>
```

Notes:

- The last JSON file has highest priority when multiple configs are passed.
- If no config is passed, ChampSim builds a default no-prefetching simulator.
- Generated files live under `.csconfig/`; binaries normally land in `bin/`.
- `config.sh` also emits the C++ test target under `test/bin`.

## Run Simulations

Typical full run:

```bash
bin/champsim --warmup_instructions 200000000 --simulation_instructions 500000000 /path/to/trace.champsimtrace.xz
```

Smoke-test run:

```bash
bin/champsim --warmup_instructions 1000000 --simulation_instructions 5000000 /path/to/trace.champsimtrace.xz
```

Report whether the executed binary name differs from `bin/champsim`; configs can set custom executable names.

## Module Editing

When adding a module:

1. Copy the closest simple baseline into a new directory.
2. Rename the `.cc` file to match the module directory when local convention does so.
3. Update a JSON config to select the new module.
4. Run `./config.sh <config.json>` before `make` so generated sources include the module.

Useful baselines:

- Branch predictor: `branch/bimodal/`, `branch/gshare/`, `branch/perceptron/`.
- BTB: `btb/basic_btb/`, `btb/tagging_btb/`, `btb/twolevel_basic_btb/`, `btb/xbtbx/`.
- Prefetcher: `prefetcher/no/`, `prefetcher/next_line/`, `prefetcher/ip_stride/`.
- Replacement: `replacement/lru/`, `replacement/srrip/`, `replacement/drrip/`.

Check `inc/ooo_cpu.h`, `inc/cache.h`, `inc/module_impl.h`, and nearby modules for method names and callback signatures. ChampSim module APIs vary by module type and fork vintage; do not rely on memory alone.

## Config Editing

Read an existing config with the same experiment family first. Preserve custom keys such as `executable_name`, branch/BTB names, cache hierarchy knobs, and SAGA/IDUN naming conventions.

Validate JSON syntax before configuring:

```bash
python3 -m json.tool <config.json>
```

For many configs:

```bash
find <config-dir> -name '*.json' -print0 | xargs -0 -n1 python3 -m json.tool
```

## Validation

Use the least expensive validation that proves the changed surface:

- Config-only change: `python3 -m json.tool <config.json>` and `./config.sh <config.json>`.
- Module compile change: `./config.sh <config.json>` and `make`.
- Config-system change: run relevant `test/python` tests if available, then configure/build.
- Runtime behavior change: run a short trace simulation and inspect the final IPC/stat section.
- Research-scale experiment scripts: dry-run or inspect generated commands first when possible; avoid launching large grids unless requested.

If dependencies are missing, first report the exact failing command and error. ChampSim dependencies are normally installed with:

```bash
git submodule update --init
vcpkg/bootstrap-vcpkg.sh
vcpkg/vcpkg install
```

These can be slow or require network access, so ask before running dependency installation when it is not already set up.

## Result Analysis

ChampSim reports IPC and additional final statistics for the simulation phase only. When comparing configurations:

- Confirm identical traces, warmup instructions, and simulation instructions.
- Distinguish warmup behavior from simulation statistics.
- Prefer normalized speedup/IPC ratios only after matching instruction counts and trace sets.
- Keep per-benchmark data visible before aggregating; geometric mean is usually more appropriate for ratios.
