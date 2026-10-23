"""Original mixed-stop/rotated-journal controls; no game or renderer."""
import json
import struct
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_diagnostic_run import collect, summarize, fault_register_history, shader_provenance, fault_field_transitions
from analyze_gpu_submission import analyze as gpu_analysis


class DiagnosticRunTests(unittest.TestCase):
    def test_fault_field_transition_uses_readable_prior_same_field_and_states_limits(self):
        bad=0x123456789abcde
        rows=[dict(field=32,value=64,status=3,ns=1000,seq=1),
              dict(field=40,value=bad,status=3,ns=1100,seq=2),
              dict(field=32,value=bad,status=1,ns=1200,seq=3),
              dict(field=32,value=bad,status=3,ns=2000,seq=4)]
        review=fault_field_transitions(rows,dict(rax=bad,rbx=bad,rcx=64))
        self.assertEqual(len(review['transitions']),1)
        change=review['transitions'][0]
        self.assertEqual(change['previous_different_observation']['value'],64)
        self.assertEqual(change['matching_registers'],['rax','rbx'])
        self.assertEqual(change['interval_ms'],0.001)
        self.assertIn('does not mean valid',review['limitation'])
        self.assertEqual(fault_field_transitions(rows,dict(rax=64))['transitions'],[])
    def test_fault_source_is_separate_from_earlier_focus_and_reports_refused_admission(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);log=root/'run.log'
            log.write_text('CPU fatal minimum: signal=0xb address=0x123456789 pc=0x456 lr=0x789\n')
            words=[0]*512;words[0]=int.from_bytes(b'SOTCEV01','little')
            words[1:6]=[1,3,1,16,1];words[16]=1;words[183]=1;words[192]=7
            payload=[0x40,0x200,0x220,0,0x220,0x123456789,15|512|1024|16384|(1<<16),0,0,0,8]
            words[160]=1;words[161]=1
            words[162:173]=[0x30,0x300,0x320,0,0x320,0x987654321,15|(1<<16),0,0,0,9]
            data=struct.pack('<512Q',*words)+struct.pack('<16Q',1,0,1,1,2,*payload)+bytes(2*128)
            (root/'pointer-events.bin').write_bytes(data)
            report=collect(log,root)
            self.assertEqual(report['object_review']['suspicious']['object'],0x300)
            source=report['fault_pointer_review']['matching_source_observations'][0]
            self.assertEqual(source['object'],0x200)
            self.assertFalse(source['watch_admitted'])
            self.assertEqual(report['pre_failure_journals']['pointer']['admission_refusals_by_probe'],[7])
            self.assertTrue(any('terminal fault source observed but' in s for s in report['evidence_gaps']))
            words[8]=2
            (root/'pointer-events.bin').write_bytes(struct.pack('<512Q',*words)+data[4096:])
            log.write_text('GPU supervisor: child_pid=1\n'+log.read_text())
            self.assertNotIn('fault_pointer_review',collect(log,root))

    def test_shader_provenance_keeps_packet_address_lookup_head_and_slot_limits(self):
        text=('Shader provenance: program=0x10 retained=2 total=12 omitted=10 slot_evictions=3 suppressed=90; CPU observations\n'
              'Shader provenance event: seq=9 program=0x10 address=0x400 kind=1 actor=2 bank=0x20 packet=0x30 first=4 words=2 found=0 head=00000000,00000000 length=0 hash=0000000000000000 required=0\n'
              'Shader provenance event: seq=10 program=0x10 address=0x400 kind=2 actor=0 bank=0 packet=0 first=0 words=0 found=0 head=baab0000,11223344 length=0 hash=00000000000000ff required=1\n'
              'Shader provenance event: seq=11 program=0x11 address=0x400 kind=2 head=00000000,00000000 hash=00\n')
        report=shader_provenance(text)[0]
        self.assertEqual(report['omitted'],10)
        self.assertEqual(report['slot_evictions'],3)
        self.assertEqual(report['events'][0]['packet'],48)
        self.assertEqual(report['events'][1]['head'],[0xbaab0000,0x11223344])
        self.assertEqual(report['events'][1]['hash'],255)
        self.assertEqual(len(report['events']),2)
        self.assertTrue(report['mismatched_event_program'])

    def test_scan_snapshot_source_and_provenance_sidecar_are_preserved(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);log=root/'run.log';log.write_text('STOP_GPU_REGISTER_RANGE: words=4\n')
            capture=root/'shader-metadata-failure';capture.mkdir()
            (capture/'code.bin').write_bytes(b'1234')
            (capture/'context.txt').write_text('snapshot_from_scan=1\nscanned=4\nscan_head_matches_initial=-1\n')
            (capture/'provenance.txt').write_text('Shader provenance: program=0x10 retained=0 total=0 omitted=0 slot_evictions=3\n')
            report=collect(log,root)
            self.assertEqual(len(report['current']['stops']),1)
            evidence=report['shader_metadata_snapshot']
            self.assertIn('Exact saved scanner input',evidence['limitation'])
            self.assertEqual(evidence['context']['scanned'],'4')
            self.assertEqual(evidence['provenance'][0]['slot_evictions'],3)

    def test_fiber_guard_stop_is_preserved_with_missing_or_bounded_context(self):
        with tempfile.TemporaryDirectory() as folder:
            log=Path(folder)/'run.log'
            log.write_text('STOP: fiber guest stack overflow\n')
            report=collect(log,None)
            self.assertEqual(report['current']['stops'][0]['message'],'STOP: fiber guest stack overflow')
            self.assertTrue(any('no stack/RSP/marker' in gap for gap in report['evidence_gaps']))
            log.write_text('STOP: fiber guest stack overflow\n'
                'Fiber stack failure: fiber=0x12 stack=0x1000 size=0x100 rsp=0x1080 '
                'bounds_valid=0x1 rsp_in_bounds=0x1 observed=0x0 expected=0x123\n')
            report=collect(log,None)
            evidence=report['current']['fiber_stack_failures'][0]
            self.assertEqual(evidence['rsp_in_bounds'],1)
            self.assertEqual(evidence['observed'],0)
            self.assertFalse(any('no stack/RSP/marker' in gap for gap in report['evidence_gaps']))

    def test_fault_register_links_include_unreadable_addresses_not_fake_values(self):
        text=('  rax=0x0 rdi=0x1234\n  r8=0x1 r15=0x4567\n'
              'CPU field trace: bounded site observations; not exhaustive\n'
              'CPU field trace event: seq=0x1 site=0x80 object=0x888 field=0x890 value=0x4567 status=0x7 thread=0x1 ns=0x10\n'
              'CPU field trace event: seq=0x2 site=0x90 object=0x1234 field=0x4567 value=0x0 status=0x5 thread=0x2 ns=0x20\n'
              'CPU field trace event: seq=0x3 site=0x90 object=0x1234 field=0x0 value=0x0 status=0x4 thread=0x2 ns=0x30\n')
        cpu=summarize(text)['cpu']
        self.assertEqual(cpu['events'],3)
        self.assertEqual(cpu['unavailable_events'],2)
        self.assertEqual(cpu['unavailable_tail'][0]['seq'],2)
        self.assertEqual(cpu['retained_span_ms'],0x20/1e6)
        registers={r['register']:r for r in cpu['fault_register_history']['registers']}
        self.assertEqual(registers['rdi']['latest_matches'][0]['matches'],['object_address'])
        self.assertEqual(registers['r15']['latest_matches'][0]['matches'],['read_value'])
        self.assertEqual(registers['r15']['latest_matches'][1]['matches'],['field_address'])
        self.assertNotIn('rax',registers)
        self.assertEqual(cpu['dump_state'],'partial')

    def test_fault_register_matches_are_bounded_and_ambiguous_snapshots_stay_separate(self):
        rows=[dict(seq=i,site=32,object=64,field=72,value=80,status=7,thread=1,ns=i)
              for i in range(1,21)]
        result=fault_register_history('  rax=0x50\n',rows)['registers'][0]
        self.assertEqual(result['retained_match_count'],20)
        self.assertEqual(result['omitted_matches'],12)
        self.assertEqual([r['event']['seq'] for r in result['latest_matches']],list(range(13,21)))
        result=fault_register_history('  rax=0x50\n  rax=0x60\n',rows)
        self.assertEqual(result['state'],'ambiguous')
        self.assertEqual(result['registers'],[])
        self.assertEqual(fault_register_history('',rows)['state'],'absent')

    def test_gpu_callbacks_are_scoped_to_batch_and_report(self):
        header='GPU submission trace: CPU-recorded candidates, not per-command GPU progress; scheduler=0x12 awaited=8 completed=7 evictions=1 missing_notes=0 missing_metal_events=0\n'
        text=header+'''GPU submission trace: LAST_COMPLETED tick=7 command_buffers=1 missing_buffers=0
GPU submission trace: buffer[0]=0x20 recording=11
GPU submission trace: Metal chunk=30 commit_returned=1 scheduled_callback=1 completed_callback=1 status=4 error=0
GPU submission trace: Metal native chunk=30 available=0 status=-1 memory_available=0 allocated_bytes=0
GPU submission trace: AWAITED tick=8 submit_returned=1 result=0 command_buffers=1 missing_buffers=0
GPU submission trace: wait semaphore=0x90 value=4
GPU submission trace: signal semaphore=0x91 value=8
GPU submission trace: buffer[0]=0x20 recording=12
GPU submission trace: Metal chunk=31 commit_returned=1 scheduled_callback=1 completed_callback=0 status=0 error=0
GPU submission trace: Metal native chunk=31 available=1 status=3 memory_available=1 allocated_bytes=4096
GPU submission trace: guest_candidates retained=2 total=3 omitted=1 limit=2
'''+header+'GPU submission trace: awaited submission not retained or submit not reached\n'
        reports=gpu_analysis(text)['reports']
        old,new=reports[0]['submissions']
        self.assertEqual(old['observed_scheduled_without_completion'],0)
        self.assertIsNone(old['native_allocated_bytes_range'])
        self.assertEqual(new['observed_scheduled_without_completion'],1)
        self.assertEqual(new['native_allocated_bytes_range'],[4096,4096])
        self.assertEqual(new['buffers'][0]['chunks'][0]['callback']['status'],0)
        self.assertEqual(new['buffers'][0]['chunks'][0]['native']['status'],3)
        self.assertEqual(new['waits'],[dict(semaphore=144,value=4)])
        self.assertEqual(new['signals'],[dict(semaphore=145,value=8)])
        self.assertTrue(any('omits earlier work' in gap for gap in reports[0]['evidence_gaps']))
        self.assertEqual(reports[1]['submissions'],[])
        self.assertTrue(any('not retained' in gap for gap in reports[1]['evidence_gaps']))

    def test_partial_gpu_report_never_invents_completion_or_memory(self):
        text='''GPU submission trace: AWAITED tick=1 command_buffers=1
GPU submission trace: CPU-recorded candidates, not per-command GPU progress; awaited=2 missing_notes=3
GPU submission trace: AWAITED tick=2 command_buffers=2 missing_buffers=1
GPU submission trace: buffer[0]=0x20 recording=12
GPU submission trace: Metal native chunk=31 available=1 memory_available=1 allocated_bytes=50
'''
        report=gpu_analysis(text)['reports'][0]
        batch=report['submissions'][0]
        self.assertEqual(batch['tick'],2)
        self.assertEqual(batch['observed_chunks'],0)
        self.assertIsNone(batch['native_allocated_bytes_range'])
        self.assertTrue(any('buffer details missing' in gap for gap in report['evidence_gaps']))
        self.assertTrue(any('callbacks absent' in gap for gap in report['evidence_gaps']))

    def test_complete_cpu_budget_and_gpu_stop_keep_evidence_gaps(self):
        with tempfile.TemporaryDirectory() as folder:
            log=Path(folder)/'run.log'
            log.write_text('STOP_GPU_STALL: renderer reported a stalled GPU\n'
                'CPU field trace summary: attempted=0x10 retained=0x0 budget_omitted=0x10\n')
            report=collect(log,None)
            self.assertEqual(report['current']['cpu']['dump_state'],'complete')
            self.assertEqual(len(report['current']['stops']),1)
            self.assertTrue(any('omits 16' in gap for gap in report['evidence_gaps']))

    def test_nested_report_minimum_is_not_a_complete_guest_report(self):
        text=('CPU fatal minimum: signal=0xb address=0x123 pc=0x456 lr=0x789 next=0xa sample=0x1 seq=0xa value=0x42\n'
              'CPU fault context: concurrent or nested fatal fault; report truncated\n'
              'Host ARM64 context: private register snapshot\n  host control: pc=0x456 lr=0x789\n'
              'Host ARM64 context: end; no recovery attempted\n')
        with tempfile.TemporaryDirectory() as folder:
            log=Path(folder)/'run.log';log.write_text(text)
            report=collect(log,None)
            self.assertTrue(report['current']['host']['host_context_complete'])
            fatal=report['current']['fatal_reporting']
            self.assertEqual(fatal['minimal_snapshots'][0]['value'],66)
            self.assertEqual(fatal['truncation_notices'],1)
            self.assertFalse(fatal['guest_context_complete'])
            self.assertEqual(report['current']['cpu']['dump_state'],'not_dumped')
            self.assertTrue(any('fatal report truncated' in x for x in report['evidence_gaps']))

    def test_specific_decode_reason_survives_generic_assertion(self):
        result=summarize('GPU shader decode: reason=unknown_instruction_format phase=main stage=6 word_offset=1 input_owned=1; CPU decode evidence\nSTOP: GPU library assertion failed\n')
        self.assertEqual(result['shader_decode_failures'][0]['reason'],'unknown_instruction_format')
        self.assertEqual(result['shader_decode_failures'][0]['input_owned'],'1')

    def test_exit_zero_does_not_hide_a_long_presentation_gap(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'visual').mkdir();log=root/'run.log'
            log.write_text('Window: closed (quit requested)\nGPU supervisor: finished elapsed=120s child_returncode=0 exit=0\n')
            (root/'visual/frames.jsonl').write_text(json.dumps(dict(event='present_recorded',frame=1,t_s=10))+'\n')
            review=collect(log,root)['progress_review']
            self.assertTrue(review['prolonged_presentation_gap'])
            self.assertTrue(review['window_close_requested'])
            self.assertEqual(review['approx_no_presentation_before_exit_s'],110)

    def test_distinct_stops_do_not_imply_common_cause_or_empty_history(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);old=root/'old.log';new=root/'new.log'
            old.write_text('STOP_EXECUTION_FAULT: SIGSEGV address=0x123\n')
            new.write_text('STOP_GPU_SHADER_METADATA: role=synthetic\n'
                'GPU supervisor: finished elapsed=1.25s child_returncode=23 exit=23\n')
            report=collect(new,None,[old])
            self.assertEqual(report['current']['cpu']['dump_state'],'not_dumped')
            self.assertEqual(report['current']['exit']['supervisor'],23)
            self.assertIn('CPU history absent or partial',report['evidence_gaps'])
            self.assertIn('SIGSEGV',report['recent_runs'][0]['stops'][0]['message'])
            self.assertIn('METADATA',report['current']['stops'][0]['message'])
            self.assertIn('not proof of a common cause',report['limitations'])

    def test_rotated_journals_missing_samples_and_malformed_tail(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);log=root/'capture.log';log.write_text('')
            (root/'visual').mkdir()
            for name,frame in [('frames.previous.jsonl',7),('frames.jsonl',8)]:
                (root/'visual'/name).write_text(json.dumps(dict(event='present_recorded',frame=frame))+'\n')
            with (root/'visual/frames.jsonl').open('a') as f:f.write('{truncated\n')
            report=collect(log,root)
            self.assertEqual(report['visual']['retained_presentations'],2)
            self.assertEqual(report['visual']['first_frame'],7)
            self.assertIsNone(report['visual']['last_native_bytes'])
            self.assertIsNone(report['last_memory_snapshot'])
            self.assertIsNone(report['audio']['queue_failures'])
            self.assertTrue(any('malformed' in x for x in report['evidence_gaps']))

    def test_cooperative_stop_and_complete_empty_dump_are_distinct(self):
        report=summarize('STOP_DIAGNOSTIC_CONTROL: parent requested termination\n'
            'CPU field trace summary: attempted=0x0 retained=0x0\n'
            'GPU supervisor: finished elapsed=1.0s child_returncode=21 exit=31\n')
        self.assertEqual(report['cpu']['dump_state'],'complete')
        self.assertEqual(report['exit']['supervisor'],31)
        self.assertEqual(report['cpu']['events'],0)
        self.assertIsNone(report['cpu']['retained_span_ms'])


if __name__=='__main__':unittest.main()
