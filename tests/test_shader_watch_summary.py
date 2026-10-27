"""Bounded summary table replacement and conservative offline interpretation."""
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_shader_watch_summary import review

@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','ARM64 required')
class SummaryTests(unittest.TestCase):
    def test_replacement_and_incomplete(self):
        source=r'''
#include <assert.h>
#include "shader_watch_summary.h"
int main(void){
 sotc_ws_init();assert(sotc_ws_map);
 uint64_t a=0x100000,other=a+256;
 sotc_ws_record(1,a,2,3,1,0,1);
 for(unsigned n=0;n<10000;++n)sotc_ws_record(2,a,n,n+1,n+2,a,n+2);
 unsigned b=sotc_ws_bucket(a);
 atomic_store(&sotc_ws_busy[b],1);sotc_ws_record(4,a,1,2,0,0,20000);
 atomic_store(&sotc_ws_busy[b],0);assert(sotc_ws_map[6]==1);
 for(unsigned n=0;n<4;++n){
  while(sotc_ws_bucket(other)!=b)other+=256;
  sotc_ws_record(1,other,5,6,n+1,0,30000+n);other+=256;
 }
 assert(sotc_ws_map[7]==1);
 sotc_ws_record(1,a,21,22,20000,0,40000);
 return 0;
}'''
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as d:
            d=Path(d);c=d/'s.c';c.write_text(source);exe=d/'s'
            subprocess.run(['clang','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT/'runtime/src'),str(c),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],env=dict(os.environ,SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(d)),check=True,capture_output=True)
            p=d/'shader-watch-summary.bin';r=review(p,0x100000)
            self.assertEqual(r['state'],'observed');self.assertEqual(r['summary']['observations'],1)
            self.assertEqual(r['table_replacements'],2);self.assertEqual(r['contention_drops'],1)
            self.assertEqual(r['continuity'],'coverage_incomplete')
            self.assertGreater(r['summary']['admission_id'],10000)
            self.assertEqual(review(p,1)['state'],'not_retained')
            raw=p.read_bytes();p.write_bytes(raw[:-1]);self.assertEqual(review(p,0x100000)['state'],'invalid')

if __name__=='__main__':unittest.main()
