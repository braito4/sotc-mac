"""Original submission/write/lookup joins; unknown evidence stays unknown."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_diagnostic_run import shader_submit_review,shader_origin_review,shader_submission_index,collect
from analyze_event_journal import analyze


def fixture():
    sample=dict(event='sample',phase='consume',capture_id=1,packet=0x800,origin=0x123400,
        raw_address=0x1234,head=[0xbeeb03ff,4],head_available=True,ambiguous_packet=False,
        compute=False,register_index=8,consumer_thread=42,submit_ns=10000,ns=15000,seq=1)
    write=dict(kind=1,actor=1,program=16,address=0x123400,packet=0x800,first=0x2c08,words=2,
        seq=2,ns=16000,thread=42,write_observation=dict(packet_origin=0x123400,
        packet_address_words=[0x1234,0],register_origin=0x123400))
    lookup=dict(kind=2,actor=2,program=16,address=0x123400,head=[0xbeeb03ff,4],seq=3,ns=20000,thread=42)
    return sample,write,lookup


class ShaderSubmitAnalysisTests(unittest.TestCase):
    def review(self,s,w,l):
        return shader_submit_review(shader_origin_review([w,l],l['address']),dict(state='readable',events=[s]))

    def test_prefix_change_and_already_unbound_are_distinct(self):
        s,w,l=fixture();r=self.review(s,w,l)
        self.assertTrue(r['pre_enqueue_head_matches_lookup']);self.assertEqual(r['enqueue_to_lookup_us'],10)
        l['head']=[3,4];r=self.review(s,w,l)
        self.assertEqual(r['observation'],'sampled_prefix_changed');self.assertTrue(r['pre_enqueue_prologue_present'])
        s['head']=[3,4];r=self.review(s,w,l)
        self.assertIn('without_recognized_prologue',r['observation'])

    def test_packet_and_prefix_changes_are_independent(self):
        s,w,l=fixture();w['write_observation']['packet_address_words']=[0x5678,0]
        r=self.review(s,w,l)
        self.assertFalse(r['pre_enqueue_packet_matches_write']);self.assertTrue(r['pre_enqueue_head_matches_lookup'])
        s['origin']=0x567800;r=self.review(s,w,l)
        self.assertEqual(r['state'],'address_changed');self.assertNotIn('pre_enqueue_head_matches_lookup',r)

    def test_corrected_compute_address_pair_joins_in_both_paths(self):
        for actor in (6,7):
            s,w,l=fixture();s.update(compute=True,register_index=0x20c)
            w.update(actor=actor,first=0x20c)
            self.assertEqual(self.review(s,w,l)['state'],'observed')

    def test_complete_address_write_is_retained_past_unrelated_write(self):
        s,w,l=fixture();l['seq']=4
        unrelated=dict(w,packet=0x900,first=0x2c12,seq=3,write_observation=dict(packet_origin=None,register_origin=0x123400))
        origin=shader_origin_review([w,unrelated,l],l['address'])
        self.assertEqual(origin['write']['packet'],0x900)
        self.assertEqual(origin['address_write']['packet'],0x800)
        self.assertEqual(shader_submit_review(origin,dict(state='readable',events=[s]))['state'],'observed')

    def test_foreign_thread_future_and_wrong_stage_are_excluded(self):
        for field,value in [('consumer_thread',99),('ns',21000),('submit_ns',16000),('compute',True),('packet',0x900),('register_index',0x48)]:
            s,w,l=fixture();s[field]=value
            self.assertEqual(self.review(s,w,l)['state'],'matching_consumption_absent',(field,value))

    def test_unavailable_and_repeated_prefixes_never_become_comparisons(self):
        for field,state in [('head_available','pre_enqueue_prefix_unavailable'),('ambiguous_packet','repeated_packet_ambiguous')]:
            s,w,l=fixture();s[field]=field=='ambiguous_packet';r=self.review(s,w,l)
            self.assertEqual(r['state'],state);self.assertNotIn('pre_enqueue_head_matches_lookup',r)

    def test_absent_foreign_and_legacy_observations_stay_unknown(self):
        s,w,l=fixture();origin=shader_origin_review([w,l],l['address'])
        for state in ['absent','foreign_process','invalid']:
            self.assertEqual(shader_submit_review(origin,dict(state=state,events=[s]))['state'],state)
        l.pop('ns');self.assertEqual(self.review(s,w,l)['state'],'lookup_time_unavailable')

    def test_failure_index_recovers_exact_pool_refused_submission(self):
        _,w,l=fixture();origin=shader_origin_review([w,l],l['address'])
        index=dict(state='matched',matches=[dict(packet=0x800,raw_address=0x1234,origin=0x123400,
            head=[0xbeeb03ff,4],head_available=True,ambiguous_packet=False,compute=False,
            register_index=8,capture_id=77,submit_ns=10000)])
        result=shader_submit_review(origin,dict(state='readable',events=[]),index)
        self.assertEqual(result['state'],'observed')
        self.assertEqual(result['evidence_source'],'failure_submission_index')
        self.assertTrue(result['pre_enqueue_head_matches_lookup'])
        self.assertNotIn('enqueue_to_consume_us',result)
        index['matches'][0]['packet']=0x900
        self.assertEqual(shader_submit_review(origin,dict(state='readable',events=[]),index)['state'],
                         'matching_consumption_absent')

    def test_failure_index_file_is_bounded_and_exact(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'submission-prefix.txt'
            path.write_text('schema=1\naddress=1192960\nstate=5\ntotal=9\nreplacements=2\n'
                'capacity=4096\nways=4\nbucket_population=1\nmatches=1\n'
                'match0_address=1192960\nmatch0_packet=2048\nmatch0_raw=4660\n'
                'match0_head=200145476052991\nmatch0_flags=1\nmatch0_capture_id=7\n'
                'match0_sampled_ns=10000\nmatch0_context=309237649152\n'
                'match0_physical=4096\nmatch0_stamp=9\n')
            result=shader_submission_index(path,0x123400)
            self.assertEqual(result['state'],'matched')
            self.assertEqual(result['matches'][0]['packet'],0x800)
            self.assertTrue(result['matches'][0]['head_available'])
            self.assertEqual(result['matches'][0]['origin'],0x123400)
            self.assertEqual(shader_submission_index(path,0x567800)['state'],'invalid')

    def test_reader_version_and_foreign_pid(self):
        with tempfile.TemporaryDirectory() as folder:
            state=Path(folder);header=[0]*512
            header[:9]=[int.from_bytes(b'SOTCEV01','little'),1,2,512,16,0,0,0,123]
            header[9:16]=[1,16,512,4096,4,262144,8]
            data=bytearray(4096+2*512*128);struct.pack_into('<512Q',data,0,*header)
            path=state/'shader-submit-events.bin';path.write_bytes(data)
            self.assertEqual(analyze(path,'shader_submit')['state'],'readable')
            log=state/'run.log';log.write_text('GPU supervisor: child_pid=999\n')
            report=collect(log,state)['pre_failure_journals']['shader_submit']
            self.assertEqual(report['state'],'foreign_process');self.assertFalse(report['events'])
            struct.pack_into('<Q',data,9*8,3);path.write_bytes(data)
            report=analyze(path,'shader_submit')
            self.assertEqual(report['state'],'readable')
            self.assertEqual(report['capture_limits']['head_samples'],64)
            struct.pack_into('<Q',data,9*8,4);path.write_bytes(data)
            self.assertEqual(analyze(path,'shader_submit')['state'],'invalid')
            struct.pack_into('<Q',data,12*8,32768);path.write_bytes(data)
            report=analyze(path,'shader_submit')
            self.assertEqual(report['state'],'readable')
            self.assertEqual(report['capture_limits']['packets_per_capture'],32768)
            struct.pack_into('<Q',data,9*8,5);path.write_bytes(data)
            self.assertEqual(analyze(path,'shader_submit')['state'],'invalid')


if __name__=='__main__':unittest.main()
