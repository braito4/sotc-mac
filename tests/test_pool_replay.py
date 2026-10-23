"""Recorded-layout controls for private import-range replay; no GPU/log input."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).parents[1]/'tools'))
import replay_pool_windows as replay


class PoolReplayTests(unittest.TestCase):
    def setUp(self):
        self.plans={'legacy':[(0,35),(30,35),(60,40)],
                    'balanced-v1':[(0,25),(20,35),(50,50)],
                    'balanced-v2':[(0,22),(17,35),(47,53)]}

    def test_recorded_v1_hits_are_not_mislabelled_as_legacy_observations(self):
        rows=[dict(event='pool_window_hit',physical=22,size=30,window_start=20,window_size=35),
              dict(event='pool_window_cross',physical=19,size=30,window_start=0,window_size=25)]
        result=replay.summarize(rows,self.plans,100)
        self.assertEqual(result['recorded_observation_mismatches'],0)
        self.assertEqual(result['comparisons']['legacy']['copies'],2)
        self.assertEqual(result['comparisons']['balanced-v1']['newly_direct'],1)
        self.assertEqual(result['comparisons']['recorded']['newly_direct'],1)
        self.assertEqual(result['comparisons']['recorded']['newly_crossing'],0)

    def test_newly_missed_range_and_inconsistent_record_are_reported(self):
        rows=[dict(event='pool_window_hit',physical=23,size=32,window_start=20,window_size=35),
              dict(event='pool_window_hit',physical=99,size=2,window_start=60,window_size=40)]
        result=replay.summarize(rows,self.plans,100)
        self.assertEqual(result['comparisons']['balanced-v1']['newly_crossing'],1)
        self.assertEqual(result['recorded_observation_mismatches'],1)
        self.assertEqual(result['observations_outside_supplied_pool'],1)

    def test_exact_boundaries_and_invalid_sizes(self):
        self.assertTrue(replay.fits([(0,22),(17,35)],17,35))
        self.assertFalse(replay.fits([(0,22),(17,35)],17,36))
        self.assertFalse(replay.fits([(0,22),(17,35)],16,35))
        self.assertFalse(replay.fits([(0,22)],0,0))
        self.assertFalse(replay.fits([(0,22)],-1,1))
        self.assertFalse(replay.fits([],0,1))


if __name__=='__main__':unittest.main()
