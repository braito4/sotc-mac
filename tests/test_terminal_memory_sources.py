"""Original terminal-read association controls; no private addresses or game code."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_diagnostic_run import terminal_memory_sources


class TerminalMemorySourcesTests(unittest.TestCase):
    def context(self,lines):
        return 'CPU fault context: private, best-effort\n'+lines+'CPU fault context: end (bounded)\n'

    def test_offset_source_is_distinct_from_exact_address_and_deduplicates(self):
        bad=0x123456789abcde00
        text=self.context(f'  CPU fault memory rbx address=0x8000 words=0x2 0x0 {bad:#x}\n'
                          f'  CPU fault memory configured_field_candidate address=0x8008 words=0x1 {bad:#x}\n')
        row=terminal_memory_sources(text,dict(rdi=bad,r12=bad),bad+64)['candidates'][0]
        self.assertEqual(row['source_field'],0x8008)
        self.assertEqual(row['matching_registers'],['rdi','r12'])
        self.assertEqual(row['fault_displacement'],64)
        self.assertFalse(row['exact_fault_address_match'])
        self.assertEqual(row['labels'],['rbx','configured_field_candidate'])

    def test_exact_negative_offset_and_normal_values(self):
        bad=0x123456789abcde00
        text=self.context(f'  CPU fault memory rsp address=0x8000 words=0x3 {bad:#x} 0x9000 {bad-16:#x}\n')
        rows=terminal_memory_sources(text,dict(rdi=bad,rsi=0x9000),bad-16)['candidates']
        self.assertEqual([r['fault_displacement'] for r in rows],[-16,0])
        self.assertTrue(rows[1]['exact_fault_address_match'])
        self.assertEqual(rows[1]['matching_registers'],[])

    def test_incomplete_ambiguous_and_malformed_reads_do_not_supply_sources(self):
        bad='0x123456789abcde00';regs=dict(rdi=int(bad,16))
        for line in [f'  CPU fault memory rbx address=0x8000 words=0x2 {bad}\n',
                     f'  CPU fault memory rbx address=0xffffffffffffffff words=0x1 {bad}\n',
                     f'  CPU fault memory rbx address=0x8000 words=0x0 {bad}\n',
                     f'  CPU fault memory rbx address=0x8000 words=0x1 {bad} garbage\n']:
            self.assertEqual(terminal_memory_sources(self.context(line),regs,1)['candidates'],[])
        text=self.context(f'  CPU fault memory rbx address=0x8000 words=0x1 {bad}\n')
        self.assertEqual(terminal_memory_sources(text+text,regs,1)['state'],'ambiguous')
        self.assertEqual(terminal_memory_sources(text+'CPU fault context: private\n',regs,1)['state'],'ambiguous')
        self.assertEqual(terminal_memory_sources(text.split('CPU fault context: end')[0],regs,1)['state'],'absent')

    def test_output_budget_and_unreadable_tail(self):
        bad=0x123456789abcde00
        text=self.context(''.join(f'  CPU fault memory sample{i} address={0x8000+i*8:#x} words=0x1 {bad:#x} stopped_unreadable_or_guarded\n' for i in range(20)))
        result=terminal_memory_sources(text,dict(rdi=bad),bad+64)
        self.assertEqual(len(result['candidates']),16)
        self.assertEqual(result['omitted'],4)


if __name__=='__main__':unittest.main()
