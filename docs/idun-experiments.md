# Build and run on IDUN

From the repository, submit the whole pipeline with:

```bash
./idun_experiment.sh storage_dse
./idun_experiment.sh fdip_icache_sizes fdip_icache
```

The first argument selects `IDUN_CONFIGS/<name>`. The optional second argument
selects `bin/<name>` and defaults to the first argument. It must match the
directory used by the configs' `executable_name` fields.

The script submits a build job and a short job with an `afterok` dependency on
that build. After the build succeeds, the short job discovers the binaries and
submits the simulation array through `idun_runner.sh`. A failed build prevents
the dependent job from running. The array ID is written to the submission job's
log, whose path the script prints. If the second submission fails, the printed
build job ID still identifies the already submitted build.

The existing build and simulation resource settings remain in `idun_build.sh`
and `idun_runner.sh`. Runner environment settings carry through the pipeline:

```bash
ARRAY_MAX_CONCURRENT=16 SIMULATION_INSTRUCTIONS=100000000 \
  ./idun_experiment.sh storage_dse
```

Other supported settings are `TRACE_ROOT`, `RESULT_ROOT`, and
`WARMUP_INSTRUCTIONS`. The runner uses all executable files in the selected
binary directory. Avoid overlapping build submissions in the same checkout,
since configuration and make outputs are shared.

Run this script directly on the login node. There is no need to wait for the
build or submit the simulation array manually.
