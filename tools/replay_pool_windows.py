"""Replay private recorded ranges through the actual C++ import planner, CPU only.

Recorded window origins/lengths provide the observation control. A journal from
balanced-v1 must not silently be treated as an original-legacy-layout capture.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
from build_gpu import ROOT


def fits(windows, physical, size):
    if physical < 0 or size <= 0:
        return False
    candidates = [(start, length) for start, length in windows if start <= physical]
    if not candidates:
        return False
    start, length = candidates[-1]
    return physical - start <= length and size <= length - (physical - start)


def summarize(rows, plans, pool_bytes):
    stats = {name: dict(direct=0, copies=0, newly_direct=0, newly_crossing=0)
             for name in ('recorded', 'legacy', 'balanced-v1', 'balanced-v2')}
    mismatches = out_of_pool = 0
    for row in rows:
        physical, size = row['physical'], row['size']
        before = row['event'] == 'pool_window_hit'
        observed = fits([(row['window_start'], row['window_size'])], physical, size)
        mismatches += observed != before
        out_of_pool += physical < 0 or size <= 0 or physical > pool_bytes or size > pool_bytes - physical
        decisions = {'recorded': before}
        decisions.update((name, fits(windows, physical, size)) for name, windows in plans.items())
        after = decisions['balanced-v2']
        for name, direct in decisions.items():
            stats[name]['direct' if direct else 'copies'] += 1
            stats[name]['newly_direct'] += not direct and after
            stats[name]['newly_crossing'] += direct and not after
    return dict(observations=len(rows), recorded_observation_mismatches=mismatches,
                observations_outside_supplied_pool=out_of_pool, comparisons=stats)


def plans(pool_bytes):
    # Compile the maintained planner rather than duplicate candidate policy in Python.
    with tempfile.TemporaryDirectory(prefix='pool-replay-', dir=ROOT/'work') as temp:
        source = Path(temp)/'plan.cpp'; binary = Path(temp)/'plan'
        source.write_text(r'''
#include "gpu_pool_windows.h"
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv) {
 if(argc!=2)return 1;
 auto bytes=std::strtoull(argv[1],nullptr,10);std::printf("[");
 for(unsigned mode=0;mode<3;++mode) {
  std::printf("%s[",mode?",":"");bool first=true;
  for(auto w:SotcPoolWindows::Plan(bytes,true,mode==0,mode==1)) {
   std::printf("%s[%llu,%llu]",first?"":",",(unsigned long long)w.start,(unsigned long long)w.size);first=false;
  }
  std::printf("]");
 }
 std::printf("]");
}
''')
        subprocess.run(['clang++', '-std=c++20', '-O1', '-I'+str(ROOT/'runtime/src'),
                        str(source), '-o', str(binary)], check=True)
        result = json.loads(subprocess.check_output([str(binary), str(pool_bytes)]))
    return dict(zip(('legacy', 'balanced-v1', 'balanced-v2'), result))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path, nargs='?')
    parser.add_argument('--all-retained', action='store_true')
    parser.add_argument('--pool-mib', type=int)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if bool(args.log) == args.all_retained:
        parser.error('Choose one log or --all-retained.')
    if args.all_retained:
        if args.pool_mib is None or not 0 < args.pool_mib <= 65536:
            parser.error('--all-retained requires an explicit pool size in 1..65536 MiB.')
        pool_bytes = args.pool_mib << 20
        journals = sorted((ROOT/'work').glob('gpu-game-state-*/memory/buffers.jsonl'))
    else:
        log = args.log.read_text()
        pool = re.search(r'\((\d+) MiB imported\)', log)
        settings = json.loads(log.splitlines()[0].split('settings=', 1)[1])
        if not pool or not settings.get('BB_FRAME_LOG'):
            parser.error('Missing recorded pool size/state path; refusing to guess.')
        if args.pool_mib is not None:
            parser.error('Single-log mode uses its recorded pool size.')
        pool_bytes = int(pool[1]) << 20
        journals = [Path(settings['BB_FRAME_LOG']).parent/'memory/buffers.jsonl']
    geometry = plans(pool_bytes)
    all_rows = []; captures = []
    for path in journals:
        if path.is_symlink() or path.stat().st_size > 128 << 20:
            parser.error('Redirected or oversized private journal.')
        rows = [row for line in path.read_text().splitlines()
                if (row := json.loads(line)).get('event') in ('pool_window_hit', 'pool_window_cross')]
        all_rows.extend(rows)
        captures.append(dict(state=path.parent.parent.name, **summarize(rows, geometry, pool_bytes)))
    report = dict(schema=2, pool_bytes=pool_bytes, journals=len(journals),
                  windows=geometry, imported_bytes={n: sum(w[1] for w in ws) for n, ws in geometry.items()},
                  **summarize(all_rows, geometry, pool_bytes), captures=captures,
                  limitation='Explicit supplied pool size; retained partial CPU observations only. '
                  'Repeated events are not independent allocations. No native memory-savings, '
                  'lifetime, universal-range or pixel-correctness claim.')
    if args.output:
        target = args.output.resolve()
        if not target.is_relative_to((ROOT/'work').resolve()):
            parser.error('Private replay results must stay inside work/.')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'captures'}))


if __name__ == '__main__':
    main()
