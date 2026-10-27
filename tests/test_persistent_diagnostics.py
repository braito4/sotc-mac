"""Synthetic state checks; never invoke Codex, Metal or a game."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from run_persistent_diagnostics import review_state, controller_locked, review_effort


class PersistentTests(unittest.TestCase):
    def test_effort_is_scoped_to_result(self):
        with tempfile.TemporaryDirectory() as folder:
            batch = Path(folder)
            self.assertEqual(review_effort(batch), 'medium')
            (batch / 'round-01-result.json').write_text(json.dumps(dict(round=1, returncode=139)))
            self.assertEqual(review_effort(batch), 'low')
            recommendation = batch / 'next-review-effort.json'
            recommendation.write_text(json.dumps(dict(after_round=1, effort='high', reason='Specific semantic comparison')))
            self.assertEqual(review_effort(batch), 'high')
            (batch / 'round-02-result.json').write_text(json.dumps(dict(round=2, returncode=23)))
            self.assertEqual(review_effort(batch), 'medium')
            recommendation.write_text(json.dumps(dict(after_round=2, effort='high', reason='')))
            with self.assertRaises(ValueError):review_effort(batch)

    def test_wait_review_and_fail_closed(self):
        with tempfile.TemporaryDirectory() as folder:
            batch = Path(folder)
            requests = batch / 'requests'; requests.mkdir()
            state = batch / 'controller-state.json'
            state.write_text(json.dumps(dict(status='waiting_for_reviewed_request', updated_unix=100)))
            self.assertEqual(review_state(batch, 105), 'review')
            pending = requests / '0001.json'; pending.touch()
            self.assertEqual(review_state(batch, 105), 'wait')
            pending.unlink()
            self.assertEqual(review_state(batch, 200), 'controller_unavailable')
            result = batch / 'round-01-result.json'
            result.write_text(json.dumps(dict(round=1, safe_to_continue=False)))
            self.assertEqual(review_state(batch, 105), 'unsafe')
            result.unlink()
            result = batch / 'round-20-result.json'
            result.write_text(json.dumps(dict(round=20, safe_to_continue=True)))
            self.assertEqual(review_state(batch, 105), 'review')
            (batch / 'round-99-result.json').write_text(json.dumps(dict(round=99, safe_to_continue=False)))
            (batch / 'round-100-result.json').write_text(json.dumps(dict(round=100, safe_to_continue=True)))
            self.assertEqual(review_state(batch, 105), 'review')
            (batch / 'STOP').touch()
            self.assertEqual(review_state(batch, 105), 'stop')

    def test_real_controller_lock_not_overwritten(self):
        import fcntl
        with tempfile.TemporaryDirectory() as folder:
            batch = Path(folder)
            self.assertFalse(controller_locked(batch))
            with (batch / 'controller.lock').open('a+') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                self.assertTrue(controller_locked(batch))
            self.assertFalse(controller_locked(batch))


if __name__ == '__main__':
    unittest.main()
