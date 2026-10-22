"""Original synthetic ranges/addresses; no game or private capture."""
import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('host_fault_analysis',Path(__file__).parents[1]/'tools/analyze_host_fault.py')
analysis=importlib.util.module_from_spec(spec);spec.loader.exec_module(analysis)

class HostFaultAnalysisTests(unittest.TestCase):
    def test_return_candidate_and_unknown_pc(self):
        text='''Host image: load=0x1000 text=0x1000 size=0x1000 uuid=fixture name=synthetic
Host ARM64 context: private register snapshot
  host registers: x16=0x2000 x17=0x1100
  host control: pc=0xdeadbeef lr=0x1234 fp=0x9000 sp=0x8000
Host ARM64 context: end
'''
        result=analysis.analyze(text)
        self.assertTrue(result['host_context_complete'])
        candidates={x['register']:x for x in result['register_candidates']}
        self.assertEqual(candidates['pc']['image_matches'],[])
        self.assertEqual(candidates['lr']['image_matches'][0]['text_offset'],0x234)
        self.assertEqual(candidates['x16']['image_matches'],[]) # exclusive end
        self.assertEqual(candidates['x17']['image_matches'][0]['name'],'synthetic')

    def test_old_log_and_truncated_context(self):
        self.assertFalse(analysis.analyze('CPU fault context: unavailable')['host_context_present'])
        result=analysis.analyze('Host ARM64 context: private\n host control: pc=0x10 lr=0x0')
        self.assertTrue(result['host_context_present'])
        self.assertFalse(result['host_context_complete'])

    def test_last_context_and_no_register_guessing(self):
        text='Host ARM64 context: private\n host control: pc=0x111\nHost ARM64 context: end\n'
        text+='Host ARM64 context: private\n host control: pc=0x222\nHost ARM64 context: end\n'
        result=analysis.analyze(text)
        self.assertEqual([x['register'] for x in result['register_candidates']],['pc'])
        self.assertEqual(result['register_candidates'][0]['value'],0x222)

if __name__=='__main__':unittest.main()
