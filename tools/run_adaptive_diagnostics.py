"""Run reviewed local diagnostic requests serially from a GPU-capable Terminal.

The analyst supplies one JSON request after reviewing the previous result.
No shell commands are accepted from requests, and no automatic retry is made.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
MEMORY_PROFILES=('memory-baseline','memory-resources','memory-render')

ALLOWED_VARIANTS=('base','caller','buffer','table','field578')


def digest(path):
    with Path(path).open('rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()


def validate_request(data,root):
    fields={'schema','round','variant','seconds','reason','fingerprints'}
    if isinstance(data,dict) and data.get('schema')==2: fields.add('profile')
    if not isinstance(data,dict) or set(data)!=fields:
        raise ValueError('unsupported request fields')
    if data['schema'] not in (1,2) or type(data['round']) is not int or data['round']<1:
        raise ValueError('invalid request schema/round')
    if data['variant'] not in ALLOWED_VARIANTS or type(data['seconds']) is not int or not 30<=data['seconds']<=500:
        raise ValueError('invalid comparison or deadline')
    if not isinstance(data['reason'],str) or not 1<=len(data['reason'])<=2000:
        raise ValueError('missing reviewed probe rationale')
    if data['schema']==2 and (data['profile'] not in MEMORY_PROFILES or data['variant']!='base'):
        raise ValueError('invalid focused memory profile')
    fingerprints=data['fingerprints']
    required={'work/probe_entry_arm64','work/gpu-build-a27f0ca477bd/libbbgpu.dylib','work/cpu-pointer-profile.json'}
    if data['schema']==2:
        required={'work/probe_entry_arm64','work/gpu-build-a27f0ca477bd/libbbgpu.dylib',
                  'prueba_memoria.command','packaging/test-arm64.sh','tools/run_adaptive_diagnostics.py',
                  'work/startup-no-intro-profile.json'}
    if not isinstance(fingerprints,dict) or not required<=fingerprints.keys():
        raise ValueError('missing executable/library/profile fingerprints')
    allowed=lambda p:p in required or p=='work/cpu-experiment-profile.json' or p.startswith(('runtime/src/','tools/','packaging/')) or p=='prueba_grafica_nativa.command'
    for name,want in fingerprints.items():
        if not isinstance(name,str) or '..' in Path(name).parts or Path(name).is_absolute() or not allowed(name):
            raise ValueError('unexpected fingerprint path')
        path=root/name
        if path.is_symlink() or not path.is_file() or not isinstance(want,str) or not re.fullmatch('[a-f0-9]{64}',want):
            raise ValueError('invalid fingerprint entry')
        if digest(path)!=want:raise ValueError('reviewed file changed: '+name)
    return data


def completed_log(text):
    return re.search(r'^GPU supervisor: finished .*child_returncode=(-?\d+) exit=(\d+) ',text,re.M)


def run_request(request,root,batch):
    validate_request(request,root)
    logs=root/'logs';before=set(logs.glob('graphics-game-*.log'))
    latest=max(before,key=lambda p:p.stat().st_mtime) if before else None
    if latest and not completed_log(latest.read_text(errors='replace')):
        raise RuntimeError('Latest game log is incomplete; confirm the previous run has closed before continuing.')
    if shutil.disk_usage(root).free<10*1024**3:raise RuntimeError('Less than 10 GiB free; no run started.')
    env=dict(os.environ,SOTC_DIAGNOSTIC_SECONDS=str(request['seconds']),SOTC_SHADER_COPY_TRACE='1',
        SOTC_SHADER_BIND_TRACE='1',SOTC_SKIP_INTRO='1',SOTC_REQUIRE_POINTER_TRACE='1')
    launcher='prueba_memoria.command' if request['schema']==2 else 'prueba_grafica_nativa.command'
    argument=request['profile'] if request['schema']==2 else request['variant']
    console=batch/f"round-{request['round']:02d}-console.log"
    started=time.time()
    with console.open('xb') as output:
        result=subprocess.run(['bash',str(root/launcher),argument],
            cwd=root,env=env,stdout=output,stderr=subprocess.STDOUT,check=False)
    added=set(logs.glob('graphics-game-*.log'))-before
    row=dict(round=request['round'],returncode=result.returncode,started_unix=started,
        finished_unix=time.time(),console=str(console),reason=request['reason'],game_logs=[str(p) for p in sorted(added)])
    row['profile']=request.get('profile','legacy')
    row['safe_to_continue']=False
    if len(added)==1:
        log=next(iter(added));text=log.read_text(errors='replace');done=completed_log(text)
        row['supervisor_finished']=bool(done)
        row['safe_to_continue']=bool(done and int(done[1])==result.returncode==int(done[2]) and
            result.returncode in (20,21,22,23,138,139) and
            'STOP_GPU_SUPERVISOR_CLEANUP:' not in text and 'STOP_GPU_STALL:' not in text)
    (batch/f"round-{request['round']:02d}-result.json").write_text(json.dumps(row,indent=2)+'\n')
    return row


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batch',type=Path,required=True)
    parser.add_argument('--max-runs',type=int,default=0,help='0 means no round-count limit')
    parser.add_argument('--idle-seconds',type=int,default=600)
    args=parser.parse_args()
    if args.max_runs<0 or not 30<=args.idle_seconds<=1800:parser.error('limits out of range')
    batch=args.batch.resolve();private=(ROOT/'work/analysis').resolve()
    if not batch.is_relative_to(private):parser.error('batch must be under private work/analysis')
    batch.mkdir(parents=True,exist_ok=True);os.chmod(batch,0o700)
    requests=batch/'requests';requests.mkdir(exist_ok=True)
    def state(status,**extra):
        path=batch/'controller-state.json';temporary=path.with_suffix('.tmp')
        temporary.write_text(json.dumps(dict(status=status,pid=os.getpid(),updated_unix=time.time(),**extra))+'\n')
        temporary.replace(path)
    interrupted=False
    def stop(signum,frame):
        nonlocal interrupted
        interrupted=True
        print('Stop requested; waiting for the foreground supervisor to confirm child exit.',flush=True)
    # Terminal Ctrl-C also reaches the foreground launcher/supervisor. The
    # supervisor owns cooperative cancellation; no external signals are added.
    signal.signal(signal.SIGINT,stop)
    with (batch/'controller.lock').open('a+') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise SystemExit('A controller already owns this batch.')
        if any(requests.glob('*.running')):raise RuntimeError('A previous round needs review; pending running request retained.')
        print('Adaptive controller ready: one reviewed request at a time; STOP file or Ctrl-C stops the series.',flush=True)
        completed=0;idle=time.monotonic();heartbeat=0
        state('waiting_for_reviewed_request')
        while (args.max_runs==0 or completed<args.max_runs) and not interrupted:
            if (batch/'STOP').exists():break
            pending=sorted(requests.glob('*.json'),key=lambda p:int(p.stem))
            if not pending:
                if time.monotonic()-heartbeat>=5:state('waiting_for_reviewed_request');heartbeat=time.monotonic()
                if time.monotonic()-idle>=args.idle_seconds:
                    print('No reviewed request arrived before the idle limit; controller stopped.',flush=True);break
                time.sleep(1);continue
            path=pending[0]
            if path.stat().st_size>65536:raise RuntimeError('oversized request')
            data=json.loads(path.read_text());validate_request(data,ROOT)
            if (batch/f"round-{data['round']:02d}-result.json").exists():raise RuntimeError('round already completed')
            active=path.with_suffix('.running');path.rename(active)
            state('running',round=data['round'])
            print(f"Starting round {data['round']}: {data['reason']}",flush=True)
            result=run_request(data,ROOT,batch);active.rename(path.with_suffix('.done'))
            print(json.dumps(result),flush=True);completed+=1;idle=time.monotonic()
            if not result['safe_to_continue']:
                print('Series stopped: precheck, stall, cleanup, concurrent-run or exit evidence requires review.',flush=True);break
            print('Waiting for analyst review and the next fingerprinted request.',flush=True)
            state('waiting_for_reviewed_request',completed=completed)
        state('stopped',completed=completed,interrupted=interrupted)
    return 0


if __name__=='__main__':
    os.umask(0o077)
    try:raise SystemExit(main())
    except (OSError,ValueError,RuntimeError) as error:raise SystemExit(str(error))
