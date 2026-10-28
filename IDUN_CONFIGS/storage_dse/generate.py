#!/usr/bin/env python3
"""Generate the four-design, 80-point instruction front-end sweep."""
import json
from pathlib import Path

ICACHE_KIB = (8, 16, 32, 64)
BTB_KENTRIES = (2, 4, 8, 16, 32)
UBS_WAYS = [4, 4, 8, 8, 8, 12, 12, 16, 24, 32, 36, 36, 52, 64, 64, 64]
# Eight ways permit exact power-of-two entry counts; the nine-way artifact
# profile is reduced by omitting its 25-bit way.
TARGET_BITS = [0, 4, 5, 7, 9, 11, 19, 64]
DESIGNS = {"Baseline": (False, False), "UBS": (False, True),
           "LiteBTB": (True, False), "Combined": (True, True)}


def configurations():
    for design, (lite, ubs) in DESIGNS.items():
        for icache in ICACHE_KIB:
            for btb in BTB_KENTRIES:
                name = f"{design.lower()}_i{icache}k_b{btb}k"
                cache = {"sets": icache * 1024 // (8 * 64), "ways": 8,
                         "ubs": ubs, "prefetcher": "fdip", "replacement": "lru",
                         "prefetch_activate": "LOAD,PREFETCH", "virtual_prefetch": True}
                if ubs:
                    cache.pop("ways")
                    cache.update(ubs_way_sizes=UBS_WAYS, ubs_candidate_ways=4)
                cpu = {"btb": "xbtbx" if lite else "basic_btb",
                       "btb_sets": btb * 1024 // 8, "btb_ways": 8}
                if lite:
                    cpu.update(btb_target_sizes=TARGET_BITS, btb_clipped_tag=True,
                               btb_tag_size=13, btb_tag_regions=0,
                               btb_perfect_mapping=False, btb_invalidate_entry=False,
                               btb_invalidate_region=False)
                yield name, {"executable_name": f"storage_dse/{name}",
                             "block_size": 64, "ooo_cpu": [cpu], "L1I": cache}


if __name__ == "__main__":
    root = Path(__file__).resolve().parent
    for name, config in configurations():
        (root / f"{name}.json").write_text(json.dumps(config, indent=2) + "\n")
    print(f"Wrote {len(list(configurations()))} configurations to {root}")
