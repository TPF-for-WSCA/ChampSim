import csv
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'aggregation_scripts'))
from plot_ipc import summarize, deviation_from_baseline, write_csv, write_plot_csv, configuration_storage


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
