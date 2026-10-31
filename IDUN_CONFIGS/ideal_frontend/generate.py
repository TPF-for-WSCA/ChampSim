#!/usr/bin/env python3
"""Generate all 32 ideal-front-end subsets from the storage-study baseline."""
import argparse
import itertools
import json
from pathlib import Path


def configurations(wrong_path=True):
    baseline = Path(__file__).resolve().parents[1] / 'storage_dse/baseline_i32k_b8k.json'
    for bits in itertools.product((False, True), repeat=5):
        config = json.loads(baseline.read_text())
        names = [name for name, enabled in zip(('icache', 'cbp', 'ibp', 'btb', 'itlb'), bits) if enabled]
        name = 'ideal_' + '_'.join(names) if names else 'baseline'
        config['executable_name'] = 'ideal_frontend/' + name
        config['ooo_cpu'][0].update({f'ideal_{key}': value for key, value in zip(('cbp', 'ibp', 'btb'), bits[1:])})
        config['ooo_cpu'][0]['wrong_path'] = wrong_path
        config['L1I']['ideal'] = bits[0]
        config['ITLB'] = {'ideal_translation': bits[4]}
        yield name, config


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-wrong-path', action='store_false', dest='wrong_path', default=True,
                        help='disable TI-WP for every configuration, including baseline')
    args = parser.parse_args()
    for name, config in configurations(args.wrong_path):
        Path(__file__).with_name(name + '.json').write_text(json.dumps(config, indent=2) + '\n')
