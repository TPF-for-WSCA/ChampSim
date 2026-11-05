# LiteBTB UTB size and replacement sweep

Ten configurations use a fixed 4K-entry BTB (512 sets × 8 ways) and an
32 KiB conventional L1I cache. Other settings match
`../storage_dse/litebtb_i32k_b4k.json`, including wrong-path execution.

Each UTB size has both `reference_count` and `lru` replacement configurations.
UTB sets remain fixed at 64; capacity varies through associativity:

| UTB entries | UTB ways | UTB sets |
| --- | --- | --- |
| 128 | 2 | 64 |
| 256 | 4 | 64 |
| 512 | 8 | 64 |
| 768 | 12 | 64 |
| 1024 | 16 | 64 |

Filenames and executable names include the UTB entry count and policy.
To build and run through the existing Slurm scripts, from the repository root:

```bash
./idun_build.sh utb_sweep
# After the build job completes successfully:
./idun_runner.sh utb_sweep
```
