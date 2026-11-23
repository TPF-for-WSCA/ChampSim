import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "aggregation_scripts"))
from plot_replacement import compare, compare_reference


def entry(ipc):
    return {"ipc": ipc, "group": "test", "path": Path("result.json")}


class ReplacementComparisonTest(unittest.TestCase):
    def test_paired_ratios_are_averaged_before_percentage_conversion(self):
        records = {
            "utb128_lru": {"a": entry(1), "b": entry(4), "unmatched": entry(99)},
            "utb128_reference_count": {"a": entry(2), "b": entry(2)},
        }
        rows, summary = compare(records)
        self.assertEqual(len(rows), 2)
        overall = summary[0]
        self.assertEqual(overall["trace_count"], 2)
        self.assertAlmostEqual(overall["speedup_percent"], -20)
        self.assertAlmostEqual(overall["baseline_ipc"], 1.6)
        self.assertAlmostEqual(overall["policy_ipc"], 2)

    def test_sweep_intersection_and_pairwise_mode(self):
        records = {f"utb{size}_{policy}": {"a": entry(1), **extra}
                   for size, extra in ((128, {"b": entry(2)}), (256, {}))
                   for policy in ("lru", "reference_count")}
        _, summary = compare(records)
        self.assertEqual([r["trace_count"] for r in summary if r["category"] == "overall"], [1, 1])
        _, summary = compare(records, pairwise=True)
        self.assertEqual([r["trace_count"] for r in summary if r["category"] == "overall"], [2, 1])

    def test_missing_counterpart_and_disjoint_traces_fail(self):
        with self.assertRaisesRegex(ValueError, "counterpart"):
            compare({"utb128_lru": {"a": entry(1)}})
        with self.assertRaisesRegex(ValueError, "no matched traces"):
            compare({"utb128_lru": {"a": entry(1)},
                     "utb128_reference_count": {"b": entry(1)}}, pairwise=True)

    def test_invalid_ipc_fails(self):
        for value in (0, -1, float("nan"), float("inf")):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "positive"):
                compare({"utb128_lru": {"a": entry(value)},
                         "utb128_reference_count": {"a": entry(1)}})


class FixedReferenceTest(unittest.TestCase):
    def test_both_policies_use_fixed_reference_and_common_traces(self):
        records = {
            "litebtb_i32k_b4k": {"a": entry(2), "b": entry(4)},
            "utb128_lru": {"a": entry(1), "b": entry(8)},
            "utb128_reference_count": {"a": entry(4)},
        }
        rows, summary = compare_reference(records)
        overall = {r["configuration"]: r for r in summary if r["category"] == "overall"}
        self.assertAlmostEqual(overall["utb128_lru"]["speedup_percent"], -50)
        self.assertAlmostEqual(overall["utb128_reference_count"]["speedup_percent"], 100)
        self.assertTrue(all(r["baseline"] == "litebtb_i32k_b4k" for r in rows))
        self.assertTrue(all(r["trace_count"] == 1 for r in summary))

    def test_missing_reference_fails(self):
        with self.assertRaisesRegex(ValueError, "missing baseline configuration"):
            compare_reference({})
