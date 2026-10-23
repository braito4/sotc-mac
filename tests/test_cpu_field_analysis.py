"""Original private-log parsing controls; no runtime or game required."""
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('field_analysis', ROOT/'tools/analyze_cpu_field_trace.py')
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)


def event(seq, value, status=7, thread=1):
    values = (seq, 64, 4096, 4133, value, status, thread, seq*100)
    return 'CPU field trace event:'+''.join(f' {key}={value:#x}' for key, value in zip(module.KEYS, values))+'\n'


class FieldAnalysisTests(unittest.TestCase):
    def test_cli_retains_history_with_non_utf8_log_text(self):
        with tempfile.TemporaryDirectory(prefix='field-log-encoding-',dir=ROOT/'work') as folder:
            log=Path(folder)/'run.log';output=Path(folder)/'analysis.json'
            log.write_bytes(b'foreign log text: \xff\xfe\n'+event(1,123).encode())
            run=subprocess.run([sys.executable,str(ROOT/'tools/analyze_cpu_field_trace.py'),
                str(log),'--output',str(output)],capture_output=True,text=True,timeout=5)
            self.assertEqual(run.returncode,0,run.stderr)
            report=json.loads(output.read_text())
            self.assertEqual(report['events'],1)
            self.assertEqual(report['text_replacement_characters'],2)
            self.assertEqual(report['fields'][0]['readable_samples'][0]['value'],123)

    def test_newest_first_partial_output_keeps_sequence_order_for_transitions(self):
        report=module.analyze('CPU field trace dump: snapshot_end=0x3 event_limit=0x800 newest_first=0x1\n'+event(3,0)+event(2,123))
        self.assertEqual(report['dump_state'],'partial')
        self.assertEqual(report['dump_metadata']['newest_first'],1)
        self.assertEqual([r['seq'] for r in report['fields'][0]['readable_samples']],[2,3])
        self.assertEqual(report['observed_nonzero_to_zero'],1)

    def test_sampled_transitions_and_unreadable_values_remain_distinct(self):
        text = event(1, 0)+event(2, 123)+event(3, 0, status=1)+event(4, 0, thread=2)
        text += 'CPU fault memory configured_field_candidate address=0x1025 words=0x1 0x0\n'
        report = module.analyze(text)
        self.assertEqual(report['events'], 4)
        self.assertEqual(report['observed_nonzero_to_zero'], 1)
        field = report['fields'][0]
        self.assertTrue(field['selected_at_fault'])
        self.assertEqual(field['unavailable'], 1)
        self.assertEqual([x['kind'] for x in field['observed_transitions']],
                         ['zero_to_nonzero', 'nonzero_to_zero'])
        self.assertIn('not exhaustive writes', report['limitations'])
        self.assertIn('threads may race', report['limitations'])

    def test_truncated_duplicate_and_absent_histories_are_explicit(self):
        text = event(1, 99)+event(1, 99)+'CPU field trace event: seq=0x2\n'
        text += 'CPU fault memory configured_field_candidate address=0x2000 words=0x0 stopped_unreadable_or_guarded\n'
        text += 'CPU field trace summary: attempted=0x10 retained=0x1 older_than_ring=0x8 busy_dropped=0x2 skipped_in_window=0x7 filtered=0x12 image_base=0x1000\n'
        report = module.analyze(text)
        self.assertEqual(report['malformed_events'], 1)
        self.assertEqual(report['duplicate_sequences'], 1)
        self.assertEqual(report['selected_fields_without_history'], [8192])
        self.assertEqual(report['observed_nonzero_to_zero'], 0)
        self.assertEqual(report['summary']['busy_dropped'], 2)
        self.assertEqual(report['summary']['filtered'], 18)
        self.assertEqual(report['dump_state'], 'partial')

    def test_missing_dump_is_distinct_from_a_complete_empty_ring(self):
        absent = module.analyze('STOP_DIAGNOSTIC_CONTROL: parent requested termination\n')
        self.assertEqual(absent['dump_state'], 'not_dumped')
        self.assertIsNone(absent['summary'])
        empty = module.analyze('CPU field trace summary: attempted=0x0 retained=0x0 filtered=0x12\n')
        self.assertEqual(empty['dump_state'], 'complete')
        self.assertEqual(empty['events'], 0)

    def test_budget_header_without_footer_is_partial_output(self):
        text = 'CPU field trace dump: snapshot_end=0x500 event_limit=0x80 budget_omitted=0x480\n'
        report = module.analyze(text+event(1280, 42))
        self.assertEqual(report['dump_state'], 'partial')
        self.assertEqual(report['dump_metadata']['budget_omitted'], 1152)


if __name__ == '__main__':
    unittest.main()
