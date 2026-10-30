import json
from pathlib import Path
import unittest

import config.parse


class TageScLConfigurationTests(unittest.TestCase):
    def test_default_predictor_and_explicit_override(self):
        for settings, expected in [({}, 'tage_sc_l'),
                                 ({'ooo_cpu': [{'branch_predictor': 'bimodal'}]}, 'bimodal')]:
            with self.subTest(expected=expected):
                _, elements, _, _, _ = config.parse.parse_config(settings)
                self.assertEqual([p['class'] for p in elements['cores'][0]['_branch_predictor_data']],
                                 [expected])

    def test_all_experiment_cores_select_tage_sc_l(self):
        root = Path(__file__).resolve().parents[2]
        paths = sorted((root / 'IDUN_CONFIGS').rglob('*.json')) + [root / 'champsim_config.json']
        self.assertGreater(len(paths), 1)
        for path in paths:
            with self.subTest(path=path.relative_to(root)):
                data = json.loads(path.read_text())
                configs = data if isinstance(data, list) else [data]
                for config in configs:
                    self.assertTrue(config['ooo_cpu'])
                    for core in config['ooo_cpu']:
                        self.assertEqual(core['branch_predictor'], 'tage_sc_l')
