"""Bounded actual submission recorder controls with original CPU data; no GPU."""
import os
from pathlib import Path
import subprocess
import tempfile
from build_gpu import ROOT
from analyze_event_journal import analyze


def main():
    modes=('stable','changed','bad-at-submit','payload-changed','compute','nested','repeated',
           'guarded','partial','depth','unavailable-ib','budget','pool','concurrent','exit','disabled','journal-disabled','existing','tail-wrap',
           'unavailable-at-consume','changed-recognized','remapped')
    with tempfile.TemporaryDirectory(prefix='shader-submit-',dir=ROOT/'work') as folder:
        out=Path(folder);binary=out/'fixture'
        subprocess.run(['clang++','-std=c++23','-O1','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-fno-sanitize-recover=all','-Wl,-export_dynamic',
                        '-I'+str(ROOT/'runtime/src'),str(ROOT/'tests/test_shader_submit.cpp'),'-o',str(binary)],check=True)
        for mode in modes:
            state=out/mode;state.mkdir()
            if mode=='existing':(state/'shader-submit-events.bin').write_bytes(b'keep original')
            result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=15,
                env=dict(os.environ,SOTC_SHADER_SUBMIT_TRACE='0' if mode=='disabled' else '1',
                         SOTC_SHADER_COPY_TRACE='1',SOTC_DIAGNOSTIC_JOURNAL='0' if mode=='journal-disabled' else '1',BB_GPU_USER_DIR=str(state)))
            assert result.returncode==(53 if mode=='exit' else 0),(mode,result.returncode,result.stdout,result.stderr)
            if mode in ('disabled','journal-disabled'):assert not (state/'shader-submit-events.bin').exists();continue
            if mode=='existing':assert (state/'shader-submit-events.bin').read_bytes()==b'keep original';continue
            report=analyze(state/'shader-submit-events.bin','shader_submit')
            assert report['state']=='readable',(mode,report)
            samples=[e for e in report['events'] if e['event']=='sample']
            consumed=[e for e in samples if e['phase']=='consume']
            if mode in ('stable','changed','payload-changed','compute','bad-at-submit','nested','repeated','tail-wrap'):
                assert len(consumed)==1,(mode,consumed)
                assert consumed[0]['head']==([0x1234,0x5678] if mode=='bad-at-submit' else [0xbeeb03ff,4])
                assert consumed[0]['compute']==(mode=='compute')
                assert consumed[0]['ambiguous_packet']==(mode=='repeated')
            if mode=='exit':assert samples and not consumed
            if mode=='tail-wrap':assert consumed[0]['capture_id']==1 and report['channels'][0]['overwritten']>0
            if mode=='pool':assert report['capture_counters']['pool_refused']==1
            fresh=analyze(state/'shader-consume-events.bin','shader_consume')
            if consumed:
                assert fresh['state']=='readable' and not fresh['incomplete_or_stale'],(mode,fresh)
                event=fresh['events'][0]
                assert event['watch_state_at_consume']==-1 and event['watch_state_after_scan']==-1
                assert event['sampled_prefix_changed']==(mode in ('changed','changed-recognized','remapped'))
                assert event['head_available']==(mode!='unavailable-at-consume')
                assert event['registration_eligible']==(mode not in ('bad-at-submit','repeated'))
                if mode=='changed':assert event['head']==[0x1234,0x5678]
                if mode=='changed-recognized':assert event['head']==[0xbeeb03ff,9] and event['channel']==1
                if mode=='compute':assert consumed[0]['queue']==3
                assert fresh['observation_counters']['observed']==len(fresh['events'])
                mappings=analyze(state/'shader-mapping-events.bin','shader_mapping')
                assert mappings['state']=='readable' and not mappings['incomplete_or_stale'],(mode,mappings)
                if event['sampled_prefix_changed']:
                    assert len(mappings['events'])==1
                    mapping=mappings['events'][0]
                    assert mapping['physical_before']==4096
                    assert mapping['physical_at_consume']==(8192 if mode=='remapped' else 4096)
                else:assert not mappings['events']
            print('PASS: '+mode,flush=True)
        state=out/'copy-disabled';state.mkdir()
        subprocess.run([str(binary),'stable'],check=True,timeout=15,
            env=dict(os.environ,SOTC_SHADER_SUBMIT_TRACE='1',SOTC_DIAGNOSTIC_JOURNAL='1',
                     SOTC_SHADER_COPY_TRACE='0',BB_GPU_USER_DIR=str(state)))
        assert not (state/'shader-consume-events.bin').exists()
    print('PASS: bounded pre-enqueue/consume shader observations; no shader/game instructions or GPU executed')


if __name__=='__main__':main()
