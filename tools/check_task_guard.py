"""CPU-only production coroutine quarantine and journal controls; never starts a game."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from build_gpu import source_path
from check_buffer_copy_chunks import function
from analyze_event_journal import analyze
from analyze_diagnostic_run import collect

ROOT=Path(__file__).resolve().parents[1]

def main():
    base=source_path(ROOT)/'gpu/shadps4/video_core/amdgpu'
    task=function((base/'liverpool.h').read_text(),'    struct Task {')+';'
    retirement=function((base/'liverpool.cpp').read_text(),'static void RetireCompletedTask(')
    if 'SotcTaskGuard::Allocate' not in task or 'SotcTaskGuard::Release' not in task:
        raise RuntimeError('Actual production promise does not use the reviewed guard')
    template=(ROOT/'tests/task_guard_fixture.cpp.in').read_text().replace('@TASK@',task).replace('@RETIREMENT@',retirement)
    with tempfile.TemporaryDirectory(prefix='task-guard-cpu-',dir=ROOT/'work') as folder:
        root=Path(folder);cpp=root/'fixture.cpp';cpp.write_text(template);binary=root/'fixture'
        subprocess.run(['clang++','-std=c++20','-O0','-pthread','-Wall','-Wextra','-Werror',
            '-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True,capture_output=True,text=True)
        for mode,enabled,extra in [('ordered','0',{}),('ordered','1',{}),('immediate','1',{}),('stale','1',{}),
            ('budget','1',{}),('churn','1',{}),('threads','1',{}),('ordered','1',{'SOTC_TEST_FAIL_PROTECT':'1'}),
            ('unmap-failure','1',{'SOTC_TEST_FAIL_UNMAP':'1'})]:
            state=root/(mode+'-'+enabled+'-'+str(len(list(root.iterdir()))));state.mkdir()
            env=dict(os.environ,SOTC_TASK_GUARD=enabled,SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(state),**extra)
            result=subprocess.run([str(binary),mode],env=env,capture_output=True,text=True,timeout=15)
            stale=mode in ('immediate','stale')
            if result.returncode not in ((138,139) if stale else (0,)):
                raise RuntimeError(mode+': '+result.stdout+result.stderr)
            if enabled=='0':
                assert not (state/'task-events.bin').exists();continue
            journal=analyze(state/'task-events.bin','task');assert journal['state']=='readable'
            assert journal['incomplete_or_stale']==0
            assert all(e['mapped_bytes']<=journal['mapped_budget'] for e in journal['events'])
            if stale:
                assert 'real protected/unmapped CPU access' in result.stderr
                retired=[e for e in journal['events'] if e['event']==2];assert retired
                log=state/'run.log';log.write_text('GPU supervisor: child_pid='+str(journal['producer_pid'])+'\n'+
                    'GPU supervisor: settings='+json.dumps(dict(SOTC_TASK_GUARD='1',SOTC_DIAGNOSTIC_JOURNAL='1'))+'\n'+result.stderr)
                report=collect(log,state);assert report['cpu_task_guard']['latest_matching_event']['event']==2
                log.write_text(log.read_text().replace('child_pid='+str(journal['producer_pid']),'child_pid=1'))
                assert 'cpu_task_guard' not in collect(log,state)
            if mode=='budget':assert any(e['event']==5 for e in journal['events'])
            if mode=='churn':assert any(c['overwritten'] for c in journal['channels'])
            if 'SOTC_TEST_FAIL_PROTECT' in extra:assert any(e['event']==4 for e in journal['events'])
            if 'SOTC_TEST_FAIL_UNMAP' in extra:assert any(e['event']==6 for e in journal['events'])
            print(mode+': '+('real stale access trapped' if stale else result.stdout.strip()))
    print('PASS: production CPU frame guard, ordered consumer, real stale-read faults, bounded fallback/churn/concurrency, failure and journal controls; no GPU')

if __name__=='__main__':main()
