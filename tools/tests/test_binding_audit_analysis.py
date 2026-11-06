import importlib.util,json,tempfile,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('audit',Path(__file__).resolve().parents[1]/'analyze_binding_audit.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
class AuditAnalysis(unittest.TestCase):
 def fixture(self,p):
  descriptor={'matched':True,'guest':4096,'bytes':4,'requested_bytes':4,'adjust':4,'sharp_hex':(4096).to_bytes(16,'little').hex()}
  binding={'slot':2,'bytes':8,'copied':8,'before':0,'after':8,'after_bytes':8,'binding_audit':descriptor}
  (p/'bundle-1.json').write_text(json.dumps({'sequence':1,'completed':True,'bindings':[binding]}));(p/'bundle-1.bin').write_bytes(b'----abcd----WXYZ')
  for phase,content in [('before','abcd'),('after','WXYZ')]:
   sample={'guest':4096,'physical':64,'bytes':4,'guest_read':1,'backing_read':1,'guest_hex':content.encode().hex(),'backing_hex':content.encode().hex(),'begin_ns':1,'end_ns':2}
   (p/f'binding-audit-1-slot-2-{phase}.json').write_text(json.dumps({'sequence':1,'slot':2,'guest':4096,'phase':int(phase=='after'),'clock':'CLOCK_MONOTONIC','generation':4,'truncated':False,'samples':[sample]}))
 def test_adjust_and_phase(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);self.fixture(p);r=audit.review(p);self.assertTrue(r['descriptors'][0]['address_matches']);self.assertEqual(len(r['comparisons']),2)
   self.assertTrue(all(x['guest_equals_gpu'] and x['backing_equals_gpu'] and x['cpu_views_equal'] for x in r['comparisons']))
 def test_mismatch_protection_and_missing(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);self.fixture(p);f=p/'binding-audit-1-slot-2-before.json';d=json.loads(f.read_text());d['samples'][0]['guest_hex']=b'zzzz'.hex();f.write_text(json.dumps(d));r=audit.review(p)['comparisons'][0];self.assertFalse(r['cpu_views_equal']);self.assertFalse(r['guest_equals_gpu']);self.assertTrue(r['backing_equals_gpu'])
   d['samples'][0]['guest_read']=0;d['samples'][0]['guest_hex']='';f.write_text(json.dumps(d));r=audit.review(p)['comparisons'][0];self.assertIsNone(r['cpu_views_equal']);self.assertIsNone(r['guest_equals_gpu'])
   f.unlink();self.assertEqual(len(audit.review(p)['missing']),1)
 def test_wrong_report_and_short_binary(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);self.fixture(p);f=p/'binding-audit-1-slot-2-before.json';d=json.loads(f.read_text());d['guest']=8192;f.write_text(json.dumps(d))
   with self.assertRaises(ValueError):audit.review(p)
   self.fixture(p);(p/'bundle-1.bin').write_bytes(b'')
   with self.assertRaises(ValueError):audit.review(p)
if __name__=='__main__':unittest.main()
