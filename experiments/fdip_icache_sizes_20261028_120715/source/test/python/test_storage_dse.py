import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'aggregation_scripts'))
from plot_storage_dse import collect, storage_from_log

spec = importlib.util.spec_from_file_location('storage_generate', ROOT / 'IDUN_CONFIGS/storage_dse/generate.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class StorageDSETests(unittest.TestCase):
    def test_sweep_covers_exact_entry_counts(self):
        configs = list(generator.configurations())
        self.assertEqual(len(configs), 80)
        self.assertEqual(len({name for name, _ in configs}), 80)
        for name, config in configs:
            cpu = config['ooo_cpu'][0]
            kentries = int(name.split('_b')[1][:-1])
            self.assertEqual(cpu['btb_sets'] * cpu['btb_ways'], kentries * 1024)
            if cpu['btb'] == 'xbtbx':
                self.assertEqual(len(cpu['btb_target_sizes']), cpu['btb_ways'])

    def fixture(self, root, name, trace='a', cache=8192, btb=4096):
        config_dir = root / 'configs'
        config_dir.mkdir(exist_ok=True)
        (config_dir / (name + '.json')).write_text(json.dumps({'executable_name': 'storage_dse/' + name}))
        result = root / 'results' / name / 'spec'
        result.mkdir(parents=True, exist_ok=True)
        (result / (trace + '.json')).write_text(json.dumps([{'traces': [trace + '.champsimtrace.xz'],
            'roi': {'cores': [{'instructions': 200, 'cycles': 100}]}}]))
        (result / (trace + '.log')).write_text(f'CPU 0 basic_btb STORAGE (modeled): total={btb} bits\n'
            f'cpu0_L1I UBS STORAGE (modeled): total={cache} bits\nSimulation complete\n')

    def test_reads_measured_storage_and_sorts(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root, 'baseline_i8k_b2k', cache=32768)
            self.fixture(root, 'ubs_i8k_b2k', cache=8192)
            summary, traces = collect(root / 'results', root / 'configs')
            self.assertEqual(summary[0]['design'], 'UBS')
            self.assertEqual(summary[0]['total_storage_kib'], 1.5)
            self.assertAlmostEqual(summary[0]['ipc_per_kib'], 2 / 1.5)
            self.assertEqual(len(traces), 2)

    def test_uses_harmonic_mean(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            name = 'baseline_i8k_b2k'
            self.fixture(root, name, trace='a')
            self.fixture(root, name, trace='b')
            path = root / 'results' / name / 'spec/b.json'
            data = json.loads(path.read_text())
            data[0]['roi']['cores'][0]['instructions'] = 800
            path.write_text(json.dumps(data))
            summary, _ = collect(root / 'results', root / 'configs')
            self.assertAlmostEqual(summary[0]['ipc'], 3.2)  # H(2, 8)
            self.assertAlmostEqual(summary[0]['ipc_per_kib'], 3.2 / 1.5)

    def test_rejects_unbalanced_trace_coverage(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root, 'baseline_i8k_b2k')
            self.fixture(root, 'ubs_i8k_b2k', trace='b')
            with self.assertRaisesRegex(ValueError, 'unbalanced'):
                collect(root / 'results', root / 'configs')

    def test_rejects_missing_and_partial_storage(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'run.log'
            path.write_text('Simulation complete\n')
            with self.assertRaises(ValueError):
                storage_from_log(path)
            path.write_text('cpu0_L1I [partial: replacement storage unavailable] STORAGE (modeled): total=8 bits\nSimulation complete\n')
            with self.assertRaisesRegex(ValueError, 'partial'):
                storage_from_log(path)


if __name__ == '__main__':
    unittest.main()
