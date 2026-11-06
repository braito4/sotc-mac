import importlib.util,json,tempfile,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('page_content',Path(__file__).resolve().parents[1]/'analyze_page_content.py')
analysis=importlib.util.module_from_spec(spec);spec.loader.exec_module(analysis)

class PageContentComparison(unittest.TestCase):
    def fixture(self,path,fragment=True):
        binding={'slot':2,'metal':111,'offset':0,'bytes':16,'copied':16,'before':0,
                 'fallback_observed':fragment,'full_span':True,'copy_guest':4096,
                 'generation_before_copy':4,'generation_after_copy':4,
                 'mapping':[{'start':4096,'end':4104,'physical':64,'mapped':True},
                            {'start':4104,'end':4112,'physical':128,'mapped':True}]}
        (path/'bundle-1.json').write_text(json.dumps({'sequence':1,'completed':True,'bindings':[binding]}))
        (path/'bundle-1.bin').write_bytes(b'abcdefghABCDEFGH')
        root={'metal':111,'offset':0,'bytes':16,'physical':64,'pool_known':not fragment}
        (path/'history-1-slot-2.json').write_text(json.dumps({'clock':'CLOCK_MONOTONIC','slot':2,'report':1,'nodes':[{'span':root}]}))
        events=[]
        for seq,kind,at,text in [(1,'memcpy_returned',64,'zzzz'),(2,'map_locked',64,'abcd'),(3,'map_locked',128,'xxxx')]:
            events.append({'seq':seq,'kind':kind,'physical':at,'bytes':8,'guest':4096 if at==64 else 4104,'ns':seq,
                'sample':{'status':1,'physical':at,'bytes':4,'hex':text.encode().hex(),'begin_ns':seq,'end_ns':seq}})
        (path/'page-history-1-slot-2-node-1.json').write_text(json.dumps({'schema':3,'clock':'CLOCK_MONOTONIC','node':1,'buckets':[{'events':events},{'events':events}]}))
        return binding

    def test_fragment_translation_deduplication_and_lifetime(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);self.fixture(p);r=analysis.review(p)['comparisons'];self.assertEqual(len(r),3)
            self.assertTrue(r[0]['precedes_latest_retained_map']);self.assertTrue(r[1]['equal'])
            self.assertEqual(r[2]['binding_relative_offset'],8);self.assertFalse(r[2]['equal'])
            self.assertEqual(r[2]['producer_before_hex'],b'ABCD'.hex())
            self.assertTrue(all(x['guest_alias_matches_binding'] for x in r))

    def test_direct_and_unstable_mapping(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);b=self.fixture(p,False);r=analysis.review(p)['comparisons']
            self.assertEqual(len(r),2);self.assertTrue(all(x['guest_alias_matches_binding'] is None for x in r))
            b=self.fixture(p);b['generation_after_copy']=6
            (p/'bundle-1.json').write_text(json.dumps({'sequence':1,'completed':True,'bindings':[b]}))
            r=analysis.review(p);self.assertFalse(r['comparisons']);self.assertTrue(r['missing'])

    def test_no_unrelated_binding_and_no_truncated_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);self.fixture(p);h=p/'history-1-slot-2.json';d=json.loads(h.read_text());d['nodes'][0]['span']['metal']=222;h.write_text(json.dumps(d))
            self.assertFalse(analysis.review(p)['comparisons'])
            self.fixture(p);(p/'bundle-1.bin').write_bytes(b'a')
            with self.assertRaises(ValueError):analysis.review(p)

if __name__=='__main__':unittest.main()
