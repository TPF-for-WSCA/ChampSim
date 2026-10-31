import json
from pathlib import Path
import unittest

import config.instantiation_file
import config.parse


class UbsSrripConfigurationTest(unittest.TestCase):
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
