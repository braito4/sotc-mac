"""Replay retained private range observations through the actual import planner.

This measures geometry only, not Metal allocation/residency or complete lifetimes.
No guest memory is read, modified or executed.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
from build_gpu import ROOT


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    log=args.log.read_text()
    pool=re.search(r'\((\d+) MiB imported\)',log)
    settings=json.loads(log.splitlines()[0].split('settings=',1)[1])
    if not pool or not settings.get('BB_FRAME_LOG'):
        raise SystemExit('Need a recorded pool size and private state path; refusing to guess.')
    state=Path(settings['BB_FRAME_LOG']).parent
    pool_bytes=int(pool[1])*(1<<20)
    # Use the maintained C++ planner itself rather than a duplicated Python policy.
    with tempfile.TemporaryDirectory(prefix='pool-replay-',dir=ROOT/'work') as temp:
        source=Path(temp)/'plan.cpp';binary=Path(temp)/'plan'
        source.write_text(r'''
#include "gpu_pool_windows.h"
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv) {
 if(argc!=2)return 1;
 auto bytes=std::strtoull(argv[1],nullptr,10);
 std::printf("[");
 for(bool legacy:{true,false}) {
  if(!legacy)std::printf(","); std::printf("["); bool first=true;
  for(auto w:SotcPoolWindows::Plan(bytes,true,legacy)) {
   std::printf("%s[%llu,%llu]",first?"":",",(unsigned long long)w.start,(unsigned long long)w.size);first=false;
  }
  std::printf("]");
 }
 std::printf("]");
}
''')
        subprocess.run(['clang++','-std=c++20','-O1','-I'+str(ROOT/'runtime/src'),str(source),'-o',str(binary)],check=True)
        legacy,balanced=json.loads(subprocess.check_output([str(binary),str(pool_bytes)]))
    def fits(windows,physical,size):
        candidates=[(start,length) for start,length in windows if start<=physical]
        if not candidates:return False
        start,length=candidates[-1]
        return physical-start<=length and size<=length-(physical-start)
    rows=[]
    for line in (state/'memory/buffers.jsonl').read_text().splitlines():
        row=json.loads(line)
        if row.get('event') in ('pool_window_hit','pool_window_cross'):rows.append(row)
    avoided=[];introduced=[];matched=0;disagreed=0
    for row in rows:
        physical,size=row['physical'],row['size']
        before=fits(legacy,physical,size);after=fits(balanced,physical,size)
        if before!=(row['event']=='pool_window_hit'):disagreed+=1
        else:matched+=1
        if not before and after:avoided.append(size)
        if before and not after:introduced.append(size)
    report=dict(pool_bytes=pool_bytes,observations=len(rows),legacy_observation_matches=matched,
                legacy_observation_mismatches=disagreed,
                avoided_copy_events=len(avoided),maximum_avoided_request_bytes=max(avoided,default=0),
                introduced_copy_events=len(introduced),maximum_introduced_request_bytes=max(introduced,default=0),
                legacy_import_bytes=sum(w[1] for w in legacy),balanced_import_bytes=sum(w[1] for w in balanced),
                limitation='Retained large-range CPU observations only; repeated events are not independent allocations. No native memory-savings or complete-lifetime claim.')
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__=='__main__':main()
