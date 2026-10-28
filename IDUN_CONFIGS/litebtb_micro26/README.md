# MICRO'26 LiteBTB configurations

These configurations reproduce the BTB geometry from commit `12894621`
(`MICRO'26`) using the current ChampSim configuration and module APIs.

- `litebtb_4k.json` is the artifact's `xbtbx` configuration.
- `rabin_litebtb_4k.json` is the final Rabin-tag configuration.

`xbtbx` is the historical artifact name for LiteBTB.  The remaining hash
variants retain their artifact module names so existing experiment scripts can
select them without translation.

For short runs across all implementations, see [smoke/README.md](smoke/README.md).
All cache implementations (including UBS) and current BTB modules now print
modeled storage at simulator startup.
