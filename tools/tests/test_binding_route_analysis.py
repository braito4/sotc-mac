import importlib.util,json,tempfile,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('route',Path(__file__).resolve().parents[1]/'analyze_binding_route.py');route=importlib.util.module_from_spec(spec);spec.loader.exec_module(route)
class RouteAnalysis(unittest.TestCase):
    def fixture(self,p):
        for n in (1,2):
            b=dict(slot=2,metal=100,native_epoch=1,offset=64,bytes=32,audit=dict(matched=True,reason='matched',written=n==1))
            d=dict(schema=1,route=n,phase='before',shader_request=42,entry='synthetic',command=n,encoding_ns=n*100,preparation_thread=1,encoder_thread=2,bindings=[b])
            (p/f'route-{n}-before.json').write_text(json.dumps(d))
            if n==1:
                d=dict(d,phase='encoded',command=11,encoding_ns=110);(p/'route-1-encoded.json').write_text(json.dumps(d))
                (p/'route-1-completed.json').write_text(json.dumps(dict(schema=1,route=1,phase='completed',command=11,ns=120,success=True)))
    def test_cross_thread_guard_replacement_and_completed_candidate(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);self.fixture(p);d=route.review(p)
            self.assertTrue(d['routes'][0]['command_changed_by_diagnostic']);self.assertTrue(d['routes'][1]['cross_thread']);self.assertFalse(d['routes'][1]['encoded']);self.assertIsNone(d['routes'][1]['completed'])
            self.assertEqual(len(d['writer_candidates']),1);self.assertTrue(d['writer_candidates'][0]['completion_observed_before_consumer'])
    def test_no_completed_or_stale_identity_claims(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);self.fixture(p);(p/'route-1-completed.json').unlink();d=route.review(p);self.assertFalse(d['writer_candidates'][0]['completion_observed_before_consumer'])
            f=p/'route-2-before.json';d=json.loads(f.read_text());d['bindings'][0]['native_epoch']=2;f.write_text(json.dumps(d));self.assertEqual(route.review(p)['writer_candidates'],[])
    def test_unknown_and_wrong_command(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);self.assertTrue(route.review(p)['missing_coverage']);self.fixture(p)
            f=p/'route-1-completed.json';d=json.loads(f.read_text());d['command']=99;f.write_text(json.dumps(d))
            with self.assertRaises(ValueError):route.review(p)
if __name__=='__main__':unittest.main()
