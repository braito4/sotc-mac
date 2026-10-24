"""Bounded shared evidence survives each terminating boundary without GPU work."""
import json
import os
from pathlib import Path
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_event_journal import analyze
from analyze_diagnostic_run import collect


@unittest.skipUnless(shutil.which('clang++'),'native C++ compiler required')
class DiagnosticJournalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory(prefix='event-journal-',dir=ROOT/'work')
        cls.root=Path(cls.temporary.name);cls.binary=cls.root/'fixture'
        subprocess.run(['clang++','-std=c++23','-O1','-Wall','-Wextra','-Werror','-pthread',
            '-fsanitize=undefined,bounds','-I'+str(ROOT/'runtime/src'),
            str(ROOT/'tests/test_diagnostic_journal.cpp'),'-o',str(cls.binary)],check=True)

    @classmethod
    def tearDownClass(cls):cls.temporary.cleanup()

    def run_fixture(self,mode,directory):
        return subprocess.run([str(self.binary),mode],env=dict(os.environ,
            BB_GPU_USER_DIR=str(directory),SOTC_DIAGNOSTIC_JOURNAL='0' if mode=='disabled' else '1'),
            capture_output=True,text=True,timeout=5)

    def test_all_abrupt_exits_keep_cpu_and_shader_tails_without_final_hook(self):
        for mode,code in [('signal',-signal.SIGSEGV),('shader',23),('fiber',23),('watchdog',21)]:
            with self.subTest(mode=mode),tempfile.TemporaryDirectory(dir=self.root) as directory:
                result=self.run_fixture(mode,directory);self.assertEqual(result.returncode,code,result.stderr)
                cpu=analyze(Path(directory)/'cpu-events.bin','cpu')
                shader=analyze(Path(directory)/'shader-events.bin','shader')
                self.assertEqual(cpu['state'],'readable');self.assertEqual(cpu['retained'],9)
                self.assertEqual([c['retained'] for c in cpu['channels']],[1,8,0])
                self.assertEqual(cpu['channels'][1]['overwritten'],92)
                self.assertEqual(shader['retained'],513)
                self.assertEqual([c['retained'] for c in shader['channels']],[1,512])
                self.assertEqual(shader['channels'][1]['overwritten'],108)
                self.assertEqual(shader['suppressed'],1)
                self.assertEqual(shader['events'][0]['kind'],1) # Hot lookups cannot evict write channel.
                self.assertEqual(shader['events'][-1]['head'],[619,2])
                self.assertEqual((Path(directory)/'cpu-events.bin').stat().st_mode&0o777,0o600)
                self.assertLess(cpu['bytes']+shader['bytes'],200000)
                settings=dict(SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_SHADER_PROVENANCE='1',SOTC_CPU_FIELD_TRACE='0x40:7:0')
                log=Path(directory)/'run.log';log.write_text('GPU supervisor: settings='+json.dumps(settings)+'\n  rax=0x4567\n')
                report=collect(log,Path(directory))
                self.assertEqual(report['capture_readiness']['cpu_journal'],'readable')
                self.assertEqual(report['pre_failure_journals']['cpu']['fault_register_history']['registers'][0]['retained_match_count'],9)
                self.assertIn('independent committed per-site journal', '\n'.join(report['evidence_gaps']))
                # A preserved file from another child cannot be current evidence.
                log.write_text(log.read_text()+'GPU supervisor: child_pid=1\n')
                foreign=collect(log,Path(directory))['pre_failure_journals']['cpu']
                self.assertEqual(foreign['state'],'foreign_process')
                self.assertEqual(foreign['events'],[])

    def test_interrupted_and_contended_writers_do_not_create_fake_events(self):
        for mode in ('interrupted','threads','delayed'):
            with self.subTest(mode=mode),tempfile.TemporaryDirectory(dir=self.root) as directory:
                result=self.run_fixture(mode,directory);self.assertEqual(result.returncode,0,result.stderr)
                cpu=analyze(Path(directory)/'cpu-events.bin','cpu')
                self.assertEqual(cpu['state'],'readable');self.assertLessEqual(cpu['retained'],17)
                self.assertEqual(cpu['channels'][0]['retained'],1)
                self.assertEqual(len({e['seq'] for e in cpu['events']}),cpu['retained'])
                self.assertTrue(all(e['value']==0x4567 for e in cpu['events']))
                if mode=='interrupted':self.assertEqual(cpu['incomplete_or_stale'],1)
                if mode=='delayed':self.assertEqual(cpu['busy_drops'],1)

    def test_disabled_existing_and_symlink_outputs_are_preserved(self):
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            path=Path(directory)/'cpu-events.bin'
            result=self.run_fixture('disabled',directory)
            self.assertEqual(result.returncode,0,result.stderr);self.assertFalse(path.exists())
            target=Path(directory)/'sentinel';target.write_bytes(b'preserve')
            path.symlink_to(target)
            result=self.run_fixture('refuse',directory)
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(target.read_bytes(),b'preserve')
            path.unlink();path.write_bytes(b'previous evidence')
            result=self.run_fixture('refuse',directory)
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(path.read_bytes(),b'previous evidence')

    def test_invalid_or_partial_file_is_reported_not_decoded(self):
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            path=Path(directory)/'cpu-events.bin'
            self.assertEqual(analyze(path,'cpu')['state'],'absent')
            for data in (b'',b'x'*4096,b'SOTCEV01'+bytes(4096)):
                path.write_bytes(data);self.assertEqual(analyze(path,'cpu')['state'],'invalid')
            result=self.run_fixture('refuse',directory);self.assertEqual(result.returncode,0)

    def test_interrupted_terminal_snapshot_is_not_promoted_to_context(self):
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            result=self.run_fixture('threads',directory);self.assertEqual(result.returncode,0,result.stderr)
            path=Path(directory)/'cpu-events.bin';data=bytearray(path.read_bytes())
            struct.pack_into('<Q',data,128*8,2**64-1);path.write_bytes(data)
            report=analyze(path,'cpu')
            self.assertTrue(report['terminal_context_incomplete'])
            self.assertNotIn('terminal_context',report)
            struct.pack_into('<3Q',data,128*8,1,99,1);path.write_bytes(data)
            report=analyze(path,'cpu')
            self.assertTrue(report['terminal_context_invalid'])
            self.assertNotIn('terminal_context',report)

    def test_owner_launcher_loads_private_selection_as_data_and_respects_overrides(self):
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            root=Path(directory);(root/'work').mkdir();(root/'packaging').mkdir()
            script=root/'launch.command';shutil.copyfile(ROOT/'prueba_grafica_nativa.command',script)
            (root/'work/cpu-combined-trace.txt').write_text('literal $(touch UNEXPECTED) `not a command`')
            (root/'work/cpu-combined-fault-fields.txt').write_text('7:0:4')
            (root/'packaging/test-arm64.sh').write_text(
                '#!/bin/bash\npython3 -c \'import json,os;print(json.dumps({k:v for k,v in os.environ.items() if k.startswith("SOTC_")}))\'\n')
            env={k:v for k,v in os.environ.items() if not k.startswith('SOTC_')}
            result=subprocess.run(['bash',str(script)],env=env,capture_output=True,text=True,timeout=5,cwd=root)
            self.assertEqual(result.returncode,0,result.stderr);settings=json.loads(result.stdout)
            self.assertEqual(settings['SOTC_DIAGNOSTIC_JOURNAL'],'1')
            self.assertEqual(settings['SOTC_SHADER_PROVENANCE'],'1')
            self.assertEqual(settings['SOTC_CPU_FIELD_TRACE'],'literal $(touch UNEXPECTED) `not a command`')
            self.assertFalse((root/'UNEXPECTED').exists())
            # The launcher applies restrictive permissions to new local output
            # without evaluating selections or altering guest memory access.
            (root/'packaging/test-arm64.sh').write_text(
                '#!/bin/bash\ntouch private-capture\npython3 -c \'import json,os;print(json.dumps({k:v for k,v in os.environ.items() if k.startswith("SOTC_")}))\'\n')
            secured=subprocess.run(['bash',str(script)],env=env,capture_output=True,text=True,timeout=5,cwd=root)
            self.assertEqual(secured.returncode,0,secured.stderr)
            self.assertEqual((root/'private-capture').stat().st_mode&0o777,0o600)
            env.update(SOTC_CPU_FIELD_TRACE='',SOTC_DIAGNOSTIC_JOURNAL='0')
            result=subprocess.run(['bash',str(script)],env=env,capture_output=True,text=True,timeout=5,cwd=root)
            settings=json.loads(result.stdout)
            self.assertEqual(settings['SOTC_CPU_FIELD_TRACE'],'')
            self.assertEqual(settings['SOTC_DIAGNOSTIC_JOURNAL'],'0')
            env.update(SOTC_REQUIRE_POINTER_TRACE='1')
            result=subprocess.run(['bash',str(script)],env=env,capture_output=True,text=True,timeout=5,cwd=root)
            self.assertEqual(result.returncode,24)
            self.assertIn('game not launched',result.stderr)
            (root/'work/cpu-pointer-trace.txt').write_text('7:13:24:8')
            (root/'work/cpu-pointer-profile.json').write_text('{}')
            result=subprocess.run(['bash',str(script)],env=env,capture_output=True,text=True,timeout=5,cwd=root)
            self.assertEqual(result.returncode,0,result.stderr)
            settings=json.loads(result.stdout)
            self.assertEqual(settings['SOTC_CPU_POINTER_TRACE'],'7:13:24:8')


if __name__=='__main__':unittest.main()
