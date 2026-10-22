import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from analyze_shader_failures import analyze


class ShaderFailureTests(unittest.TestCase):
    failure = 'Failed to create graphics pipeline fs_0x1,vs_0x2: ErrorInitializationFailed; its draws are skipped\n'
    conversion = '[mvk-error] SPIR-V to MSL conversion error: PerVertexKHR unsupported\n'

    def test_adjacent_driver_evidence_and_repetitions(self):
        result = analyze([self.conversion, '[mvk-error] VK_ERROR_INITIALIZATION_FAILED: details\n',
                          self.failure, self.failure])
        self.assertEqual(result['pipeline_failure_events'], 2)
        self.assertEqual(result['unique_failed_shader_pairs'], 1)
        self.assertEqual(result['pipeline_adjacent_conversion'], {'PerVertexKHR unsupported': 1, 'unattributed': 1})
        self.assertEqual(result['unattributed_events_with_fragment_evidence_elsewhere'], 1)

    def test_intervening_work_and_distance_break_association(self):
        for middle in (['GPU unrelated operation\n'], ['\n'] * 3):
            with self.subTest(middle=middle):
                result = analyze([self.conversion, *middle, self.failure])
                self.assertEqual(result['pipeline_adjacent_conversion'], {'unattributed': 1})

    def test_srt_provenance_does_not_invent_missing_context(self):
        result = analyze(['Failed to compute offset for SRT walker\n'] * 3)
        self.assertEqual(result['srt_failure_events'], 3)
        self.assertEqual(result['srt_provenance'], [])
        result = analyze(['GPU SRT diagnostic: shader=1234 stage=2 site=copy expression=0:Phi(imm,ref0); bounded IR shape, values omitted, CPU evidence only\n'])
        self.assertEqual(result['srt_provenance'][0]['expression'], '0:Phi(imm,ref0)')


if __name__ == '__main__':
    unittest.main()
