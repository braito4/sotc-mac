"""Original fixtures: missing utilization is unknown, not idle."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from gpu_utilization import utilization_rows


class UtilizationTests(unittest.TestCase):
    def test_multiple_devices_and_missing_counters(self):
        data = b'"PerformanceStatistics" = {"Device Utilization %"=42,"Renderer Utilization %"=36,"Tiler Utilization %"=37}\n"PerformanceStatistics" = {"Device Utilization %"=0}'
        self.assertEqual(utilization_rows(data), [[42, 36, 37], [0, '', '']])

    def test_absent_and_invalid_are_not_zero(self):
        self.assertEqual(utilization_rows(b'no counters'), [])
        data = b'"PerformanceStatistics" = {"Device Utilization %"=101,"Renderer Utilization %"="9","Tiler Utilization %"=-1}'
        self.assertEqual(utilization_rows(data), [['', '', '']])
