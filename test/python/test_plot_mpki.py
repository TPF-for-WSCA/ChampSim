import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'aggregation_scripts'))
from plot_mpki import load_mpki, summarize_mpki, write_mpki_outputs


class MPKIPlotTests(unittest.TestCase):
    def test_pool_phases_and_select_core(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'result.json'
            phases = [dict(roi=dict(cores=[dict(instructions=100), dict(instructions=n)],
                                   cpu1_L1I={'LOAD': {'miss': [999, misses]}}))
                      for n, misses in [(1000, 10), (9000, 20)]]
            path.write_text(json.dumps(phases))
            self.assertEqual(load_mpki(path, 1), 3)
            del phases[1]['roi']['cpu1_L1I']
            path.write_text(json.dumps(phases))
            self.assertIsNone(load_mpki(path, 1))
            path.write_text(json.dumps(dict(roi=dict(cores=[dict(instructions=0)],
                                                   cpu0_L1I={'LOAD': {'miss': [0]}}))))
            self.assertIsNone(load_mpki(path))

    def test_ratio_of_means_paired_traces_and_undefined_values(self):
        def entry(value):
            return dict(group='client', mpki=value)
        records = {'base': {'a': entry(10), 'b': entry(100), 'unmatched': entry(500)},
                   'ubs': {'a': entry(5), 'b': entry(100)},
                   'ideal': {'a': entry(0), 'b': entry(0)},
                   'missing': {'a': entry(None), 'b': entry(0)}}
        baselines = {name: 'base' for name in records}
        categories, absolute, reduction = summarize_mpki(records, list(records), {'a', 'b', 'unmatched'}, baselines)
        self.assertEqual(categories, ['overall', 'client'])
        self.assertEqual(absolute['ubs']['overall']['mean'], 52.5)
        self.assertAlmostEqual(reduction['ubs']['overall']['mean'], 100 * (1 - 105 / 110))
        self.assertEqual(reduction['ubs']['overall']['count'], 2)
        self.assertEqual(reduction['ideal']['overall']['mean'], 100)
        self.assertIsNone(reduction['missing']['overall']['mean'])
        _, _, zero = summarize_mpki(records, ['ideal'], {'a', 'b'}, {'ideal': 'ideal'})
        self.assertIsNone(zero['ideal']['overall']['mean'])
        _, _, absent = summarize_mpki(records, ['ubs'], {'a', 'b'}, {'ubs': 'absent'})
        self.assertIsNone(absent['ubs']['overall']['mean'])

    def test_csv_exports_include_baseline_and_missing_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result = root / 'result.json'
            result.write_text(json.dumps([dict(roi=dict(cores=[dict(instructions=1000)],
                                                       cpu0_L1I={'LOAD': {'miss': [12]}}))]))
            records = {'base': {'a': dict(group='server', path=result)}}
            with patch('plot_mpki.plot'):
                write_mpki_outputs(records, ['base'], {'a'}, {'base': 'base'}, root / 'graph.png')
            with (root / 'graph_mpki.csv').open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(float(rows[0]['mpki']), 12)
            self.assertEqual(float(rows[0]['mpki_reduction_percent']), 0)
            self.assertEqual(rows[0]['baseline'], 'base')
