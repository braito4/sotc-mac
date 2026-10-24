"""Original conservative stage-association and coverage classification tests."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_shader_consume import review, mapping_review


class ConsumptionReviewTests(unittest.TestCase):
    def setUp(self):
        self.origin=dict(lookup=dict(address=256,thread=7,ns=300,head=[3,4]),
            address_write=dict(actor=1,packet=8,thread=7,ns=200,first=0x2c08,words=2))
        self.event=dict(origin=256,packet=8,thread=7,ns=150,register_index=8,compute=False,
            ambiguous_packet=False,capture_id=42,head_available=True,head=[3,4],
            pre_enqueue_available=True,sampled_prefix_changed=True,registration_eligible=True,
            watch_state_after_scan=5,watch_state_at_consume=5,watch_packet=8,watch_capture_id=42)
    def run_review(self,event=None):
        return review(self.origin,dict(state='readable',events=[event or self.event]))
    def test_exact_association_and_changed_interval(self):
        result=self.run_review()
        self.assertEqual(result['state'],'observed')
        self.assertTrue(result['consume_prefix_matches_lookup'])
        self.assertTrue(result['pre_enqueue_to_consume_changed'])
        self.assertEqual(result['watch_coverage_observation'],'same_registration_at_consume')
        for key,value in [('origin',512),('packet',16),('thread',9),('ns',250),('compute',True),('register_index',0x48)]:
            self.assertEqual(self.run_review(dict(self.event,**{key:value}))['state'],'matching_consumption_absent')
        self.assertEqual(review(self.origin,dict(state='readable',events=[self.event,self.event]))['state'],'ambiguous_consumption')
        self.assertEqual(self.run_review(dict(self.event,ambiguous_packet=True))['state'],'repeated_packet_ambiguous')
    def test_coverage_gaps_are_not_causal_claims(self):
        cases=[(dict(registration_eligible=False),'sample_not_eligible_for_registration'),
            (dict(watch_state_after_scan=3),'watch_not_confirmed_after_registration_attempt'),
            (dict(watch_state_at_consume=2),'watch_present_after_scan_missing_at_consume'),
            (dict(watch_capture_id=43),'different_registration_at_consume'),
            (dict(watch_state_at_consume=4),'watch_state_unavailable_at_consume')]
        for changes,want in cases:
            self.assertEqual(self.run_review(dict(self.event,**changes))['watch_coverage_observation'],want)
        result=self.run_review(dict(self.event,head_available=False))
        self.assertNotIn('consume_prefix_matches_lookup',result)
        self.assertEqual(review(self.origin,dict(state='absent'))['state'],'absent')

    def test_pool_offset_comparison_requires_exact_change_observation(self):
        consume=dict(state='observed',sample=dict(self.event,pre_enqueue_head=[1,2]))
        event=dict(consume['sample'],physical_before=4096,physical_at_consume=8192,
            physical_before_available=True,physical_at_consume_available=True)
        def run(row):return mapping_review(consume,dict(state='readable',events=[row]))
        self.assertEqual(run(event)['state'],'observed_pool_offset_changed')
        self.assertEqual(run(dict(event,physical_at_consume=4096))['state'],'same_observed_pool_offset')
        self.assertEqual(run(dict(event,physical_at_consume_available=False))['state'],'pool_offset_unavailable')
        self.assertEqual(run(dict(event,capture_id=43))['state'],'matching_mapping_absent')
        self.assertEqual(run(dict(event,ns=151))['state'],'matching_mapping_absent')


if __name__=='__main__':unittest.main()
