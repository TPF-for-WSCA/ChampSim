import csv
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'aggregation_scripts'))
from plot_ipc import summarize, deviation_from_baseline, write_csv, write_plot_csv, configuration_storage, select_default_baseline, main


class BaselineSelectionTests(unittest.TestCase):
    def test_smallest_conventional_capacity_in_an_unordered_sweep(self):
        configs = ['fdip_icache_128k', 'fdip_icache_ubs_2k', 'fdip_icache_16k',
                   'fdip_icache_4k', 'fdip_icache_ubs_4k', 'fdip_icache_8k']
        self.assertEqual(select_default_baseline(configs), 'fdip_icache_4k')
        self.assertEqual(select_default_baseline(['cache_64k', 'cache_8k']), 'cache_8k')
        self.assertEqual(select_default_baseline(['cache_32k', 'cache_128k']), 'cache_32k')

    def test_missing_or_ambiguous_baselines_require_an_override(self):
        for configs, message in [(['fdip_icache_ubs_4k', 'fdip_icache_ubs_16k'], 'no conventional'),
                                 (['baseline'], 'no conventional'),
                                 (['a_4k', 'b_4k', 'a_16k'], 'multiple smallest')]:
            with self.subTest(configurations=configs), self.assertRaisesRegex(ValueError, message):
                select_default_baseline(configs)

    def test_cli_exports_selected_reference_and_preserves_explicit_override(self):
        records = {name: {'client_001': dict(ipc=ipc, group='client', path='source.json')}
                   for name, ipc in [('fdip_icache_4k', 1.0), ('fdip_icache_16k', 2.0), ('fdip_icache_ubs_4k', 1.5)]}
        for override in (None, 'fdip_icache_16k'):
            with self.subTest(override=override), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                output = root / 'graphs.png'
                argv = ['plot_ipc.py', str(root), '--output', str(output)]
                if override:
                    argv += ['--baseline', override]
                with patch.object(sys, 'argv', argv), patch('plot_ipc.collect_results', return_value=(records, [])), \
                        patch('plot_ipc.plot') as plotting, patch('plot_ipc.plot_wrong_path_workloads'), \
                        patch('plot_mpki.write_mpki_outputs') as mpki, patch('builtins.print'):
                    main()
                selected = override or 'fdip_icache_4k'
                with (root / 'graphs_relative.csv').open() as stream:
                    rows = {row['configuration']: row for row in csv.DictReader(stream)}
                key = f'ipc_percent_deviation_from_{selected}'
                self.assertEqual(float(rows[selected][key]), 0)
                expected = 50.0 if override is None else -25.0
                self.assertEqual(float(rows['fdip_icache_ubs_4k'][key]), expected)
                self.assertEqual(plotting.call_args.args[5], f'Harmonic mean speedup vs {selected} (%)')
                conventional_comparison = mpki.call_args_list[1]
                self.assertEqual(conventional_comparison.args[3],
                                 {'fdip_icache_4k': 'fdip_icache_4k', 'fdip_icache_16k': 'fdip_icache_4k'})
                self.assertEqual(conventional_comparison.args[4].name, 'graphs_conventional_vs_4k.png')


class PlotCSVTests(unittest.TestCase):
    def test_ubs_sizes_pair_with_same_nominal_baseline(self):
        from plot_ubs_presentation import presentation_rows
        for size in (4, 8, 16, 32, 64, 128, 256):
            self.assertEqual(configuration_storage(f'fdip_icache_ubs_{size}k'), size)
            self.assertEqual(configuration_storage(f'fdip_icache_{size}k'), size)
        stale = [{'configuration': 'fdip_icache_ubs_4k', 'storage_kib': '2', 'value': '1.234'},
                 {'configuration': 'fdip_icache_ubs_128k', 'storage_kib': '64', 'value': '5.678'},
                 {'configuration': 'fdip_icache_256k', 'storage_kib': '256', 'value': '9'}]
        rows = presentation_rows(stale)
        self.assertEqual([r['storage_kib'] for r in rows], [4, 128])
        self.assertEqual([r['value'] for r in rows], ['1.234', '5.678'])

    def test_harmonic_ipc_and_relative_speedups(self):
        records = {'base': {'a': dict(ipc=1.0, group='client'), 'b': dict(ipc=1.0, group='client')},
                   'test': {'a': dict(ipc=0.5, group='client'), 'b': dict(ipc=2.0, group='client')}}
        configs, traces = list(records), {'a', 'b'}
        _, absolute = summarize(records, configs, traces)
        self.assertAlmostEqual(absolute['test']['overall']['mean'], 0.8)
        relative = deviation_from_baseline(records, configs, traces, 'base')
        _, summary = summarize(relative, configs, traces, relative=True)
        self.assertAlmostEqual(summary['test']['overall']['mean'], -20.0)

    def test_exports_exact_aggregates_and_unrounded_inputs(self):
        records = {
            'fdip_icache_4k': {'a': dict(ipc=1.1234567891234567, group='client', path='a.json'),
                               'b': dict(ipc=3.0, group='server', path='b.json')},
            'fdip_icache_8k': {'a': dict(ipc=2.0, group='client', path='a.json'),
                               'b': dict(ipc=4.0, group='server', path='b.json')},
        }
        configs = list(records)
        traces = {'a', 'b'}
        relative = deviation_from_baseline(records, configs, traces, configs[0])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'plot.csv'
            for values in (records, relative):
                categories, summary = summarize(values, configs, traces, relative=(values is relative))
                write_plot_csv(summary, configs, categories, path, 'metric')
                rows = list(csv.DictReader(io.StringIO(path.read_text())))
                self.assertEqual(len(rows), 6)
                for row in rows:
                    actual = summary[row['configuration']][row['category']]
                    self.assertEqual(float(row['value']), actual['mean'])
                    self.assertEqual(int(row['trace_count']), actual['count'])
            write_csv(records, configs, traces, path)
            rows = list(csv.DictReader(io.StringIO(path.read_text())))
            self.assertEqual(float(rows[0]['ipc']), records[configs[0]]['a']['ipc'])
