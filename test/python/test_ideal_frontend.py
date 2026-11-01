import importlib.util
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'aggregation_scripts'))
from plot_ideal_frontend import collect, configurations, group_diagnostics, summarize_groups

spec = importlib.util.spec_from_file_location('ideal_generate', ROOT / 'IDUN_CONFIGS/ideal_frontend/generate.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class IdealFrontendTests(unittest.TestCase):
    def fixture(self, root, name, trace, cycles):
        path = root / name / 'spec' / (trace + '.json')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps([{'traces': [trace + '.champsimtrace.xz'],
                                    'roi': {'cores': [{'instructions': 1000, 'cycles': cycles}]}}]))
        path.with_suffix('.log').write_text('Simulation complete\n')

    def test_sweep_retains_baseline_and_covers_every_subset(self):
        configs = dict(generator.configurations())
        self.assertEqual(set(configs), {name for name, _ in configurations()})
        self.assertEqual(len(configs), 32)
        base = json.loads((ROOT / 'IDUN_CONFIGS/storage_dse/baseline_i32k_b8k.json').read_text())
        baseline = configs['baseline']
        for name, config in configs.items():
            cpu = config['ooo_cpu'][0]
            self.assertEqual(cpu['branch_predictor'], 'tage_sc_l')
            self.assertEqual(cpu['btb'], 'basic_btb')
            self.assertEqual(cpu['btb_sets'] * cpu['btb_ways'], 4096)
            self.assertEqual(config['L1I']['sets'] * config['L1I']['ways'] * config['block_size'], 32768)
            for key in ('L1D', 'L2C', 'LLC'):
                self.assertEqual(config[key], base[key])
            committed = json.loads((ROOT / f'IDUN_CONFIGS/ideal_frontend/{name}.json').read_text())
            self.assertEqual(config, committed)
        baseline['ooo_cpu'][0]['btb_sets'] = base['ooo_cpu'][0]['btb_sets']
        self.assertEqual(baseline['ooo_cpu'][0].pop('mispredict_penalty'), 2)
        self.assertEqual(baseline['ooo_cpu'][0].pop('fdip_ftq_size'), 24)
        baseline['executable_name'] = base['executable_name']
        baseline['L1I'].pop('ideal')
        self.assertEqual(baseline.pop('ITLB'), {'ideal_translation': False})
        self.assertTrue(baseline['ooo_cpu'][0].pop('wrong_path'))
        for key in ('ideal_cbp', 'ideal_ibp', 'ideal_btb'):
            self.assertFalse(baseline['ooo_cpu'][0].pop(key))
        self.assertEqual(base, baseline)

    def test_ideal_cache_validation_and_generated_switches(self):
        import config.parse
        with self.assertRaisesRegex(ValueError, 'only for instruction caches'):
            config.parse.parse_config({'L1D': {'ideal': True}})
        study = dict(generator.configurations())['ideal_icache_cbp_ibp_btb_itlb']
        _, elements, _, _, _ = config.parse.parse_config(study)
        core = elements['cores'][0]
        for key in ('ideal_cbp', 'ideal_ibp', 'ideal_btb', 'wrong_path'):
            self.assertTrue(core[key])
        self.assertTrue(next(c for c in elements['caches'] if c['name'] == core['L1I'])['ideal'])
        self.assertTrue(next(c for c in elements['caches'] if c['name'] == core['ITLB'])['ideal_translation'])
        for level in ('L1D', 'DTLB', 'STLB', 'L1I'):
            with self.assertRaisesRegex(ValueError, 'only for instruction TLBs'):
                config.parse.parse_config({level: {'ideal_translation': True}})

    def test_geometric_mean_of_paired_speedups(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ('baseline', 'ideal_cbp'):
                self.fixture(root, name, 'a', 1000 if name == 'baseline' else 500)
                self.fixture(root, name, 'b', 200 if name == 'baseline' else 400)
            summary, individual = collect(root, True)
            self.assertAlmostEqual(summary[1]['speedup'], math.sqrt(2 * 0.5))
            self.assertEqual(len(individual), 4)

    def test_groups_are_normalized_and_averaged_separately(self):
        summary = [dict(configuration='baseline', label='Baseline'), dict(configuration='ideal_btb', label='BTB')]
        rows = [dict(configuration=name, group=group, speedup=ratio)
                for name, group, ratio in [('baseline', 'server', 1), ('baseline', 'server', 1),
                                           ('baseline', 'client', 1), ('ideal_btb', 'server', 2),
                                           ('ideal_btb', 'server', 8), ('ideal_btb', 'client', 0.5)]]
        groups = {(row['group'], row['configuration']): row for row in summarize_groups(summary, rows)}
        self.assertEqual(groups['server', 'ideal_btb']['speedup'], 4)
        self.assertEqual(groups['server', 'ideal_btb']['trace_count'], 2)
        self.assertEqual(groups['client', 'ideal_btb']['speedup'], 0.5)

    def test_unknown_wrong_path_share_pools_counts_and_handles_undefined(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for trace, fetched, unknown in [('spec_a', 10, 10), ('spec_b', 90, 0)]:
                self.fixture(root, 'baseline', trace, 100)
                path = root / 'baseline/spec' / (trace + '.json')
                data = json.loads(path.read_text())
                data[0]['roi']['cores'][0].update(wrong_path_fetched=fetched, wrong_path_unknown=unknown)
                path.write_text(json.dumps(data))
            summary = [dict(configuration='baseline')]
            row, = group_diagnostics(root, summary)
            self.assertEqual(row['wrong_path_unknown_percent'], 10)
            self.assertEqual(row['wrong_path_fetched'], 100)
            for fetched in (0, None):
                for path in (root / 'baseline').rglob('*.json'):
                    data = json.loads(path.read_text())
                    core = data[0]['roi']['cores'][0]
                    core.update(wrong_path_fetched=0, wrong_path_unknown=0)
                    if fetched is None:
                        del core['wrong_path_fetched']
                    path.write_text(json.dumps(data))
                self.assertIsNone(group_diagnostics(root, summary)[0]['wrong_path_unknown_percent'])

    def test_rejects_missing_unbalanced_duplicate_and_incomplete(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root, 'baseline', 'a', 100)
            with self.assertRaisesRegex(ValueError, 'no results'):
                collect(root)
            self.fixture(root, 'ideal_cbp', 'b', 100)
            with self.assertRaisesRegex(ValueError, 'unbalanced'):
                collect(root, True)
            self.fixture(root, 'ideal_cbp', 'a', 100)
            (root / 'ideal_cbp/spec/b.json').unlink()
            p = root / 'ideal_cbp/spec/a.json'
            p.with_name('duplicate.json').write_text(p.read_text())
            p.with_name('duplicate.log').write_text('Simulation complete\n')
            with self.assertRaisesRegex(ValueError, 'duplicate'):
                collect(root, True)
            p.with_name('duplicate.json').unlink()
            p.with_suffix('.log').write_text('unfinished')
            with self.assertRaisesRegex(ValueError, 'incomplete'):
                collect(root, True)
