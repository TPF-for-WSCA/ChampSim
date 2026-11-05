import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from contextlib import redirect_stderr
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'aggregation_scripts'))
from plot_storage_dse import DESIGNS, collect, interactive_figure, plot, plot_interactive, storage_from_log, pareto_boundary

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
                self.assertEqual(cpu['btb_tag_size'], 13)
                self.assertEqual(cpu['btb_full_target_tag_size'], 15)
                self.assertEqual((cpu['btb_utb_entries'], cpu['btb_utb_ways']), generator.UTB_GEOMETRY[kentries])
                self.assertEqual(cpu['btb_utb_replacement'], 'lru')
            else:
                self.assertTrue(cpu['btb_clipped_tag'])
                self.assertEqual(cpu['btb_tag_size'], 15)

    def test_checked_in_sweeps_use_configured_btb_tags(self):
        for directory in ('storage_dse', 'storage_dse_wrongpath'):
            paths = sorted((ROOT / 'IDUN_CONFIGS' / directory).glob('*.json'))
            self.assertEqual(len(paths), 80)
            for path in paths:
                with self.subTest(config=str(path)):
                    cpu = json.loads(path.read_text())['ooo_cpu'][0]
                    self.assertTrue(cpu['btb_clipped_tag'])
                    self.assertEqual(cpu['btb_tag_size'], 15 if cpu['btb'] == 'basic_btb' else 13)
                    if cpu['btb'] == 'xbtbx':
                        self.assertEqual(cpu['btb_full_target_tag_size'], 15)
                        size = cpu['btb_sets'] * cpu['btb_ways'] // 1024
                        self.assertEqual((cpu['btb_utb_entries'], cpu['btb_utb_ways']), generator.UTB_GEOMETRY[size])
                        self.assertEqual(cpu['btb_utb_replacement'], 'lru')

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
            self.assertAlmostEqual(summary[0]['ipc_per_mib'], 2 * 1024 / 1.5)
            self.assertTrue(all(row['ipc_per_mib'] == row['ipc'] * 1024 / row['total_storage_kib'] for row in traces))
            self.assertEqual(len(traces), 2)

    def test_prefetch_buffer_budget_is_excluded_from_icache_storage(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'run.log'
            path.write_text('CPU 0 basic_btb STORAGE (modeled): total=4096 bits\n'
                'cpu0_L1I UBS STORAGE (modeled): total=8192 bits\n'
                'Prefetcher cpu0_L1I prefetch buffer STORAGE (modeled): total=19312 bits\n'
                'Simulation complete\n')
            self.assertEqual(storage_from_log(path), (8192, 4096))

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
            self.assertAlmostEqual(summary[0]['ipc_per_mib'], 3.2 * 1024 / 1.5)

    def test_rejects_unbalanced_trace_coverage(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root, 'baseline_i8k_b2k')
            self.fixture(root, 'ubs_i8k_b2k', trace='b')
            with self.assertRaisesRegex(ValueError, 'unbalanced'):
                collect(root / 'results', root / 'configs')

    def test_allow_incomplete_keeps_only_maximum_workload_count(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root, 'baseline_i8k_b2k')
            for name in ('ubs_i8k_b2k', 'combined_i8k_b2k'):
                for trace in ('a', 'b'):
                    self.fixture(root, name, trace=trace)
            # A configuration with no results is also excluded.
            (root / 'configs/litebtb_i8k_b2k.json').write_text(json.dumps(
                {'executable_name': 'storage_dse/litebtb_i8k_b2k'}))
            errors = io.StringIO()
            with redirect_stderr(errors):
                summary, traces = collect(root / 'results', root / 'configs', allow_incomplete=True)
            self.assertEqual({row['configuration'] for row in summary}, {'ubs_i8k_b2k', 'combined_i8k_b2k'})
            self.assertTrue(all(row['trace_count'] == 2 for row in summary))
            self.assertEqual({(row['configuration'], row['trace']) for row in traces},
                             {(name, trace) for name in ('ubs_i8k_b2k', 'combined_i8k_b2k') for trace in ('a', 'b')})
            self.assertIn('Skipping incomplete configuration: baseline_i8k_b2k (1/2 workloads)', errors.getvalue())
            self.assertIn('Skipping missing configuration: litebtb_i8k_b2k', errors.getvalue())

    def test_requires_allow_incomplete_to_skip_fewer_workloads(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root, 'baseline_i8k_b2k')
            for trace in ('a', 'b'):
                self.fixture(root, 'ubs_i8k_b2k', trace=trace)
            with self.assertRaisesRegex(ValueError, 'unbalanced'):
                collect(root / 'results', root / 'configs')

    def test_allow_incomplete_rejects_equal_counts_with_different_traces(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.fixture(root, 'baseline_i8k_b2k', trace='a')
            self.fixture(root, 'ubs_i8k_b2k', trace='b')
            with self.assertRaisesRegex(ValueError, 'unbalanced'):
                collect(root / 'results', root / 'configs', allow_incomplete=True)

    def test_rejects_missing_and_partial_storage(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'run.log'
            path.write_text('Simulation complete\n')
            with self.assertRaises(ValueError):
                storage_from_log(path)
            path.write_text('cpu0_L1I [partial: replacement storage unavailable] STORAGE (modeled): total=8 bits\nSimulation complete\n')
            with self.assertRaisesRegex(ValueError, 'partial'):
                storage_from_log(path)


class StorageDSEPlotTests(unittest.TestCase):
    def test_pareto_boundary_handles_ties_and_dominated_points(self):
        points = [dict(total_storage_kib=x, ipc=y) for x, y in
                  [(4, 3), (2, 2), (1, 1), (2, 3), (2, 3), (3, 2), (5, 4)]]
        self.assertEqual([(r['total_storage_kib'], r['ipc']) for r in pareto_boundary(points, 'ipc')],
                         [(1, 1), (2, 3), (5, 4)])
        self.assertEqual(pareto_boundary([], 'ipc'), [])

    def summary(self):
        return [dict(configuration=f'{key}_i{size}k_b{entries // 1024}k', design=label,
                     nominal_icache_kib=size, btb_entries=entries,
                     l1i_bits=size * 8192, btb_bits=entries * 32,
                     total_storage_kib=size + entries / 256, trace_count=50,
                     ipc=0.6 + size / 1000 + index / 100,
                     ipc_per_mib=(0.6 + size / 1000 + index / 100) * 1024 / (size + entries / 256))
                for index, (key, (label, _, _)) in enumerate(DESIGNS.items())
                for size in (8, 16) for entries in (2048, 4096)]

    @unittest.skipUnless(importlib.util.find_spec('matplotlib'), 'Matplotlib is required')
    def test_presentation_exports_preserve_axes_and_isolate_designs(self):
        saved = {}

        def capture(fig, path, **kwargs):
            saved[path.stem] = dict(
                transparent=kwargs['transparent'],
                geometry=[(ax.get_xlim(), ax.get_ylim(), ax.get_position().bounds) for ax in fig.axes],
                visible=[{artist.get_label() for artist in ax.collections if artist.get_visible()} for ax in fig.axes],
                axes=[ax.axison for ax in fig.axes], title=fig._suptitle.get_visible())

        with tempfile.TemporaryDirectory() as tmp:
            with patch('matplotlib.figure.Figure.savefig', autospec=True, side_effect=capture) as saving:
                plot(self.summary(), Path(tmp) / 'dse')
            self.assertEqual(saving.call_count, 20)  # PNG and PDF for every export.
        reference = saved['dse']['geometry']
        for key, (label, _, _) in DESIGNS.items():
            for suffix in ('', '_layer'):
                image = saved[f'dse_{key}{suffix}']
                self.assertEqual(image['geometry'], reference)
                self.assertTrue(image['transparent'])
                self.assertEqual(image['visible'], [{label}, {label}])
                self.assertEqual(image['axes'], [not suffix, not suffix])
                self.assertEqual(image['title'], not suffix)
        self.assertEqual(saved['dse_axes']['geometry'], reference)
        self.assertEqual(saved['dse_axes']['visible'], [set(), set()])
        self.assertTrue(saved['dse_axes']['transparent'])

    @unittest.skipUnless(importlib.util.find_spec('plotly'), 'Plotly is required')
    def test_interactive_groups_both_metrics_and_embeds_filter_metadata(self):
        summary = self.summary()
        fig = interactive_figure(summary)
        self.assertEqual(len(fig.data), 10)
        for key, (label, _, _) in DESIGNS.items():
            traces = [trace for trace in fig.data if trace.legendgroup == key]
            self.assertEqual(len(traces), 2)
            self.assertEqual([trace.showlegend for trace in traces], [True, False])
            for trace in traces:
                self.assertEqual(trace.name, label)
                self.assertEqual(len(trace.x), 4)
                for x, y, metadata in zip(trace.x, trace.y, trace.customdata):
                    row = next(row for row in summary if row['configuration'] == metadata[0])
                    self.assertEqual(x, row['total_storage_kib'])
                    self.assertEqual(y, row[trace.meta['metric']])
                    self.assertEqual(list(metadata[1:3]), [row['nominal_icache_kib'], row['btb_entries']])
        self.assertEqual(fig.layout.legend.groupclick, 'togglegroup')
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'dse'
            plot_interactive(summary, output)
            document = output.with_suffix('.html').read_text()
        self.assertIn('plotly.js v', document)
        self.assertIn('Nominal L1I capacity', document)
        self.assertIn('BTB entry count', document)
        self.assertNotIn("document.getElementById('{plot_id}')", document)
        self.assertIn("document.getElementById('storage-dse-plot')", document)


if __name__ == '__main__':
    unittest.main()
