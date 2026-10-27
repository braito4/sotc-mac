"""Actual SH register copy cases with adjacent canaries, bounded lookup trace; no GPU."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from build_gpu import ROOT, source_path, build_path
from check_buffer_copy_chunks import function
from analyze_diagnostic_run import shader_provenance


def case(text):
    begin=text.index('case PM4ItOpcode::SetShReg:')
    end=text.index('case PM4ItOpcode::SetUconfigReg:',begin)
    return text[begin:end]


def main():
    tree=source_path(ROOT);path=Path('gpu/shadps4/video_core/amdgpu/liverpool.cpp')
    with tempfile.TemporaryDirectory(prefix='shader-provenance-',dir=ROOT/'work') as folder:
        out=Path(folder);old=out/path;old.parent.mkdir(parents=True);old.write_bytes((tree/path).read_bytes())
        old.with_suffix('.h').write_bytes((tree/path.with_suffix('.h')).read_bytes())
        for name in ('sotc-shader-submit-trace.patch.json','sotc-shader-producer-trace.patch.json','sotc-shader-provenance.patch.json'):
            patch=json.loads((ROOT/'tools/gpu_patches'/name).read_text())
            subprocess.run(['git','-C',str(out),'apply','--reverse'],input='\n'.join(patch['lines'])+'\n',
                           text=True,check=True,capture_output=True)
        template=(ROOT/'tests/shader_provenance_fixture.cpp.in').read_text()
        for label,text in [('previous',old.read_text()),('adapted',(tree/path).read_text())]:
            graphics=function(text,'void Liverpool::ApplyGraphicsRegisterPacket(')
            compute=function(text,'Liverpool::Task Liverpool::ProcessGraphics(')
            cpp=out/(label+'.cpp');binary=out/label
            cpp.write_text(template.replace('@GRAPHICS@',case(graphics)).replace('@COMPUTE@',case(compute)))
            subprocess.run(['clang++','-std=c++23','-O1','-Wall','-Wextra','-Werror',
                '-fsanitize=undefined,bounds','-fno-sanitize-recover=all',
                '-I'+str(ROOT/'runtime/src'),'-isystem',str(tree/'gpu/shim'),'-isystem',str(tree/'gpu/shadps4'),
                '-I'+str(tree/'deps/macos-arm64/include'),'-I'+str(build_path(ROOT)/'include'),
                str(cpp),'-o',str(binary)],check=True)
            for mode in ('compute-end','compute-cross','graphics-over','compute-valid','graphics-valid'):
                run=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5,
                    env=dict(os.environ,SOTC_SHADER_PROVENANCE='1'))
                bad=mode not in ('compute-valid','graphics-valid')
                expected=23 if label=='adapted' and bad else 0
                assert run.returncode==expected,(label,mode,run.returncode,run.stderr)
                if bad:
                    assert ('STOP_GPU_REGISTER_RANGE' in run.stderr) if label=='adapted' else ('canary' in run.stdout)
                elif label=='adapted' and mode=='graphics-valid':
                    assert 'kind=1 actor=1' in run.stdout and 'retained=3 total=3 omitted=0' in run.stdout,run.stdout
                    assert 'suppressed=99' in run.stdout and 'found=0 head=00000003,00000002' in run.stdout,run.stdout
                    assert all(event.get('ns') and event.get('thread') for event in
                        shader_provenance(run.stdout)[0]['events']),run.stdout
            if label=='adapted':
                for mode in ('compute-zero','graphics-zero'):
                    run=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5)
                    assert run.returncode==0,run.stdout+run.stderr
                disabled=subprocess.run([str(binary),'graphics-valid'],capture_output=True,text=True,timeout=5,
                    env=dict(os.environ,SOTC_SHADER_PROVENANCE='0'))
                assert disabled.returncode==0 and 'Shader provenance:' not in disabled.stdout
                run=subprocess.run([str(binary),'ring'],capture_output=True,text=True,timeout=5)
                assert run.returncode==0 and 'retained=8 total=20 omitted=12' in run.stdout,run.stdout+run.stderr
                assert 'retained=0 total=0 omitted=0' in run.stdout,run.stdout
                for mode,expected in [('address','matched_programs=8 omitted_programs=2'),
                                      ('missing-address','matched_programs=0 omitted_programs=0')]:
                    run=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5)
                    assert run.returncode==0 and expected in run.stdout,run.stdout+run.stderr
                    assert 'program=0x63' not in run.stdout
                from analyze_event_journal import analyze
                for mode in ('producer-full','producer-mutated','producer-partial','producer-short','producer-other'):
                    state=out/mode;state.mkdir()
                    run=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=5,
                        env=dict(os.environ,SOTC_SHADER_PROVENANCE='1',SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(state)))
                    assert run.returncode==53,(mode,run.returncode,run.stderr)
                    events=analyze(state/'shader-events.bin','shader')['events']
                    assert len(events)==1
                    observed=events[0]['write_observation']
                    assert observed['register_matches_observed_address']
                    expected=None if mode in ('producer-partial','producer-short','producer-other') else mode!='producer-mutated'
                    assert observed['packet_matches_register']==expected,(mode,observed)
                    assert (observed['packet_address_words'] is None)==(expected is None)
                for mode in ('full','gfxqueue','mutated','partial','short','other'):
                    state=out/('compute-producer-'+mode);state.mkdir()
                    run=subprocess.run([str(binary),'compute-producer-'+mode],capture_output=True,text=True,timeout=5,
                        env=dict(os.environ,SOTC_SHADER_PROVENANCE='1',SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(state)))
                    assert run.returncode==53,(mode,run.returncode,run.stderr)
                    events=analyze(state/'shader-events.bin','shader')['events']
                    assert len(events)==1
                    event=events[0];observed=event['write_observation']
                    assert event['actor']==(6 if mode=='gfxqueue' else 7)
                    assert observed['register_matches_observed_address'],(mode,observed)
                    assert observed['register_origin']==(0x987600 if mode=='mutated' else 0x12345600)
                    expected=None if mode in ('partial','short','other') else mode!='mutated'
                    assert observed['packet_matches_register']==expected,(mode,observed)
            print(label+': actual graphics/compute SH end/crossing controls, valid bytes and canaries checked')
        # Confirm all three legacy SH write paths call the checked guard;
        # the alternate decoder records its own already-applied register writes.
        text=(tree/path).read_text()
        assert text.count('SotcShaderProvenance::CheckShWrite(')==3
        assert 'SotcShaderProvenance::Graphics(self->regs' in text
        print('PASS: outside writes stop before copy, bounded write/lookup history distinguishes unchanged address/content changes; no GPU')


if __name__=='__main__':main()
