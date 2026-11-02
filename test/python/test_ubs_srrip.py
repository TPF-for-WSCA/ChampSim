import copy
import json
from pathlib import Path
import unittest

import config.instantiation_file
import config.parse


class UbsSrripConfigurationTest(unittest.TestCase):
    root = Path(__file__).resolve().parents[2]

    def assert_cache_pair(self, baseline, uneven):
        baseline, uneven = copy.deepcopy(baseline), copy.deepcopy(uneven)
        self.assertEqual(baseline['L1I'].pop('ways'), 8)
        self.assertTrue(uneven['L1I'].pop('ubs'))
        self.assertFalse(baseline['L1I'].pop('ubs', False))
        self.assertEqual(uneven['L1I'].pop('ubs_candidate_ways'), 4)
        sizes = uneven['L1I'].pop('ubs_way_sizes')
        self.assertEqual(sizes, [4, 4, 8, 8, 8, 12, 12, 16, 24, 32, 36, 36, 52, 64, 64, 64])
        baseline.pop('executable_name')
        uneven.pop('executable_name')
        self.assertEqual(baseline, uneven)

    def test_baseline_and_ubs_differ_only_in_cache_organization(self):
        pairs = []
        for directory in ('fdip_icache_sizes', 'fdip_icache_sizes_wrongpath'):
            for path in sorted((self.root / 'IDUN_CONFIGS' / directory).glob('*_ubs_*.json')):
                pairs.append((path.with_name(path.name.replace('_ubs_', '_')), path))
        for path in sorted((self.root / 'IDUN_CONFIGS/storage_dse').glob('ubs_*.json')):
            pairs.append((path.with_name(path.name.replace('ubs_', 'baseline_', 1)), path))
        self.assertEqual(len(pairs), 32)
        for baseline_path, uneven_path in pairs:
            with self.subTest(config=str(uneven_path)):
                baseline = json.loads(baseline_path.read_text())
                uneven = json.loads(uneven_path.read_text())
                self.assert_cache_pair(baseline, uneven)
                # Also compare resolved cores, channels, translations and modules.
                resolved = []
                for configuration in (baseline, uneven):
                    elements = copy.deepcopy(config.parse.parse_config(configuration)[1])
                    l1i = next(c for c in elements['caches'] if c['name'] == elements['cores'][0]['L1I'])
                    for key in ('ways', 'ubs', 'ubs_way_sizes', 'ubs_candidate_ways'):
                        l1i.pop(key, None)
                    resolved.append(elements)
                self.assertEqual(*resolved)

    def test_wrongpath_sweep_changes_only_wrongpath_enablement(self):
        for path in sorted((self.root / 'IDUN_CONFIGS/fdip_icache_sizes').glob('*.json')):
            with self.subTest(config=path.name):
                baseline = json.loads(path.read_text())
                wrongpath = json.loads((self.root / 'IDUN_CONFIGS/fdip_icache_sizes_wrongpath' / path.name).read_text())
                self.assertFalse(baseline['ooo_cpu'][0].get('wrong_path', False))
                self.assertTrue(wrongpath['ooo_cpu'][0].pop('wrong_path'))
                baseline.pop('executable_name')
                wrongpath.pop('executable_name')
                self.assertEqual(baseline, wrongpath)

    def test_experimental_sweeps_generate_ubs_with_srrip(self):
        root = Path(__file__).resolve().parents[2]
        for directory in ('fdip_icache_sizes', 'fdip_icache_sizes_wrongpath'):
            for path in sorted((root / 'IDUN_CONFIGS' / directory).glob('*.json')):
                with self.subTest(config=str(path)):
                    _, elements, _, _, _ = config.parse.parse_config(json.loads(path.read_text()))
                    cache = next(c for c in elements['caches'] if c['name'] == elements['cores'][0]['L1I'])
                    self.assertEqual([r['class'] for r in cache['_replacement_data']], ['srrip'])
                    channels = [(cache['lower_level'], cache['name']), (cache['lower_translate'], cache['name'])]
                    generated = '\n'.join(config.instantiation_file.get_cache_builder(cache, channels))
                    self.assertIn('.replacement<class srrip>()', generated)
                    if '_ubs_' in path.stem:
                        self.assertTrue(cache['ubs'])
                        self.assertIn('.set_ubs()', generated)
                        self.assertIn('.ubs_candidate_ways(4)', generated)
                        self.assertEqual(len(cache['ubs_way_sizes']), 16)
                    else:
                        self.assertFalse(cache.get('ubs', False))
