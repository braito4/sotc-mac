"""Compile bounded metadata lookup against the actual pinned BinaryInfo layout."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))

class ShaderMetadataTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_signature_search_does_not_prove_program_association(self):
        source=(ROOT/'runtime/upstream/gpu/shadps4/video_core/amdgpu/regs_shader.h').read_text()
        begin=source.index('struct BinaryInfo {');end=source.index('enum class FpRoundMode',begin)
        with tempfile.TemporaryDirectory(prefix='metadata-association-') as directory:
            work=Path(directory)
            (work/'binary_info_fixture.h').write_text(
                '#include <array>\n#include <cstdint>\nusing u8=std::uint8_t;'
                'using u32=std::uint32_t;using u64=std::uint64_t;\n'+source[begin:end])
            binary=work/'association'
            subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-fno-sanitize-recover=all',
                '-I'+str(work),'-I'+str(ROOT/'runtime/src'),
                str(ROOT/'tests/test_shader_metadata_association.cpp'),'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],check=True,capture_output=True,text=True,timeout=15)
            self.assertIn('PASS: mapped non-program prefix cannot borrow',result.stdout)
            self.assertIn('PASS: conflicting prologue hint cannot borrow',result.stdout)
            print(result.stdout.strip())

    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_real_binary_info_layout_and_bounded_lookup(self):
        header=ROOT/'runtime/upstream/gpu/shadps4/video_core/amdgpu/regs_shader.h'
        source=header.read_text()
        begin=source.index('struct BinaryInfo {')
        end=source.index('enum class FpRoundMode',begin)
        with tempfile.TemporaryDirectory(prefix='shader metadata ') as directory:
            work=Path(directory)
            (work/'binary_info_fixture.h').write_text(
                '#include <array>\n#include <cstdint>\nusing u8=std::uint8_t;'
                'using u32=std::uint32_t;using u64=std::uint64_t;\n'+source[begin:end])
            binary=work/'check'
            subprocess.run(['c++','-std=c++20','-Wall','-Wextra','-Werror',
                            '-I'+str(work),'-I'+str(ROOT/'runtime/src'),
                            str(ROOT/'tests/test_shader_metadata.cpp'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=15)

    @unittest.skipUnless(shutil.which('c++') and (ROOT/'work/gpu-source-84daa9f13e5c/deps/macos-arm64/include/fmt/format.h').is_file(),
                         'C++ compiler and prepared renderer headers required')
    def test_runtime_wrapper_and_required_failure_evidence(self):
        import os
        source=ROOT/'runtime/upstream'
        prefix=ROOT/'work/gpu-source-84daa9f13e5c/deps/macos-arm64'
        with tempfile.TemporaryDirectory(prefix='shader lookup ') as directory:
            work=Path(directory)
            binary=work/'lookup'
            command=['c++','-std=c++23','-Wall','-Wextra',
                            '-I'+str(ROOT/'runtime/src'), '-I'+str(source/'gpu/shim'),
                            '-I'+str(source/'gpu/shadps4'), '-I'+str(prefix/'include'),
                            str(ROOT/'tests/test_shader_lookup.cpp'), '-o',str(binary)]
            subprocess.run(command,check=True)
            subprocess.run([str(binary)],check=True,timeout=15)
            indexed=work/'indexed-fail';indexed.mkdir()
            result=subprocess.run([str(binary),'indexed-fail'],capture_output=True,text=True,timeout=15,
                env=dict(os.environ,BB_GPU_USER_DIR=str(indexed)))
            self.assertEqual(result.returncode,23,result.stderr)
            from analyze_diagnostic_run import shader_submission_index
            context=dict(line.split('=',1) for line in
                         (indexed/'shader-metadata-failure/context.txt').read_text().splitlines())
            snapshot=shader_submission_index(indexed/'shader-metadata-failure/submission-prefix.txt',int(context['address']))
            self.assertEqual(snapshot['state'],'matched')
            self.assertEqual(snapshot['matches'][0]['packet'],0x800)
            self.assertEqual(snapshot['matches'][0]['head'],[0xbeeb03ff,4])
            self.assertEqual(snapshot['matches'][0]['capture_id'],7)
            state=work/'compute-required';state.mkdir()
            result=subprocess.run([str(binary),'compute-required'],capture_output=True,text=True,timeout=15,
                env=dict(os.environ,BB_GPU_USER_DIR=str(state)))
            self.assertEqual(result.returncode,23,result.stderr)
            folder=state/'shader-metadata-failure'
            meta=dict(line.split('=',1) for line in (folder/'context.txt').read_text().splitlines())
            self.assertEqual(meta['descriptor_layout'],'compute_address40_at48_shift8')
            self.assertEqual(meta['descriptor_address_offset'],'48')
            import struct
            raw=(folder/'descriptor.bin').read_bytes()
            self.assertEqual(struct.unpack_from('<Q',raw)[0],0x100000001)
            self.assertEqual((struct.unpack_from('<Q',raw,48)[0]&((1<<40)-1))<<8,int(meta['address']))
            # Reproduce the preceding wrapper's second descriptor read, using
            # the same actual metadata wrapper with only that return expression restored.
            previous=work/'previous-address';previous.mkdir()
            fixed=(ROOT/'runtime/src/gpu_shader_lookup.h').read_text()
            guarded='.code=std::span{reinterpret_cast<const u32*>(address),result.info.length/sizeof(u32)},'
            original='.code=std::span{program.template Address<const u32*>(),result.info.length/sizeof(u32)},'
            self.assertEqual(fixed.count(guarded),1)
            (previous/'gpu_shader_lookup.h').write_text(fixed.replace(guarded,original,1))
            before=command.copy();before[1:1]=['-I'+str(previous)];before[-1]=str(work/'previous-lookup')
            subprocess.run(before,check=True)
            env=dict(os.environ,SOTC_TEST_MUTATE_PROGRAM='1')
            result=subprocess.run([str(work/'previous-lookup'),'changed-valid'],capture_output=True,text=True,timeout=15,env=env)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('valid->code.data()',result.stderr)
            result=subprocess.run([str(binary),'changed-valid'],capture_output=True,text=True,timeout=15,env=env)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('same captured address',result.stdout)
            result=subprocess.run([str(binary),'fail'],capture_output=True,text=True,timeout=15,
                                  env=dict(os.environ,BB_GPU_USER_DIR=str(work)))
            self.assertEqual(result.returncode,23,result.stderr)
            self.assertIn('STOP_GPU_SHADER_METADATA: role=synthetic-required',result.stderr)
            folder=work/'shader-metadata-failure'
            self.assertEqual((folder/'code.bin').read_bytes(),bytes(4096))
            self.assertIn('role=synthetic-required',(folder/'context.txt').read_text())
            # A second failure preserves the first evidence instead of overwriting it.
            (folder/'context.txt').write_text('preserve first capture')
            result=subprocess.run([str(binary),'fail'],capture_output=True,text=True,timeout=15,
                                  env=dict(os.environ,BB_GPU_USER_DIR=str(work)))
            self.assertEqual(result.returncode,23)
            self.assertEqual((folder/'context.txt').read_text(),'preserve first capture')
            # The same production wrapper must invoke an optional host exactly
            # on required failure, preserving valid/speculative behavior and 23.
            subprocess.run(command+['-DSOTC_TEST_HISTORY_HOOK']+
                (['-Wl,-export_dynamic'] if sys.platform=='darwin' else []),check=True)
            valid=subprocess.run([str(binary)],capture_output=True,text=True,timeout=15)
            self.assertEqual(valid.returncode,0,valid.stderr)
            self.assertNotIn('SYNTHETIC_CPU_HISTORY',valid.stderr)
            for case in ('fail','large'):
                state=work/('with-hook-'+case);state.mkdir()
                result=subprocess.run([str(binary),case],capture_output=True,text=True,timeout=15,
                    env=dict(os.environ,BB_GPU_USER_DIR=str(state)))
                self.assertEqual(result.returncode,23,result.stderr)
                self.assertEqual(result.stderr.count('SYNTHETIC_CPU_HISTORY'),1)
                self.assertLess(result.stderr.index('SYNTHETIC_CPU_HISTORY'),result.stderr.index('GPU shader lookup evidence:'))
                import re
                scanned=int(re.search(r'scanned=(\d+)',result.stderr)[1])
                self.assertEqual(len((state/'shader-metadata-failure/code.bin').read_bytes()),scanned)
                self.assertIn('found=0 length=0',result.stderr)
            # The history callback mutates backing after the failed scan.
            # Saved evidence must retain the scanned zeros, with no second read.
            state=work/'mutated-after-scan';state.mkdir()
            result=subprocess.run([str(binary),'large'],capture_output=True,text=True,timeout=15,
                env=dict(os.environ,BB_GPU_USER_DIR=str(state),SOTC_TEST_MUTATE_HISTORY='1',
                         SOTC_TEST_MUTATE_DESCRIPTOR='1',SOTC_SHADER_PROVENANCE='1'))
            self.assertEqual(result.returncode,23,result.stderr)
            folder=state/'shader-metadata-failure'
            self.assertEqual((folder/'code.bin').read_bytes(),bytes(65564))
            self.assertIn('snapshot_from_scan=1',(folder/'context.txt').read_text())
            self.assertIn('Shader provenance:',(folder/'provenance.txt').read_text())
            import struct
            raw=(folder/'descriptor.bin').read_bytes()
            meta=dict(line.split('=',1) for line in (folder/'context.txt').read_text().splitlines())
            self.assertEqual((struct.unpack_from('<Q',raw)[0]&((1<<40)-1))<<8,int(meta['address']))
            state=work/'descriptor-changed-during-scan';state.mkdir()
            result=subprocess.run([str(binary),'large'],capture_output=True,text=True,timeout=15,
                env=dict(os.environ,BB_GPU_USER_DIR=str(state),SOTC_TEST_MUTATE_PROGRAM='1',
                         SOTC_SHADER_PROVENANCE='1',SOTC_DIAGNOSTIC_JOURNAL='1'))
            self.assertEqual(result.returncode,23,result.stderr)
            address=re.search(r'STOP_GPU_SHADER_METADATA: .*?address=(0x[0-9a-f]+)',result.stderr)[1]
            provenance=(state/'shader-metadata-failure/provenance.txt').read_text()
            self.assertIn('address='+address+' kind=2',provenance)
            from analyze_event_journal import analyze
            evidence=analyze(state/'shader-events.bin','shader')
            self.assertEqual(evidence['state'],'readable')
            self.assertEqual(evidence['retained'],1)
            self.assertEqual(evidence['events'][0]['address'],int(address,16))
            self.assertEqual(evidence['events'][0]['required'],1)
            self.assertEqual(evidence['events'][0]['metadata_context'],dict(offset=0,hint=0,scanned=65564))
            self.assertEqual(evidence['events'][0]['metadata_association'],'unbound_missing_prologue')

            state=work/'association-required';state.mkdir()
            result=subprocess.run([str(binary),'association-required'],capture_output=True,text=True,timeout=15,
                env=dict(os.environ,BB_GPU_USER_DIR=str(state),SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_SHADER_PROVENANCE='1'))
            self.assertEqual(result.returncode,23,result.stderr)
            self.assertIn('association=unbound_hint_metadata',result.stderr)
            evidence=analyze(state/'shader-events.bin','shader')
            self.assertEqual(evidence['events'][0]['found'],0)
            self.assertEqual(evidence['events'][0]['metadata_association'],'unbound_hint_metadata')
            self.assertTrue((state/'shader-metadata-failure/code.bin').is_file())

            for mode in ('fallback-exit','fallback-cap','fallback-types','fallback-duplicate','fallback-disabled','fallback-existing'):
                state=work/mode;state.mkdir()
                if mode=='fallback-existing':
                    preserved=state/'shader-lookup-trace/case-0';preserved.mkdir(parents=True)
                    (preserved/'code.bin').write_bytes(b'preserve existing capture')
                result=subprocess.run([str(binary),mode],capture_output=True,text=True,timeout=15,
                    env=dict(os.environ,BB_GPU_USER_DIR=str(state),SOTC_DIAGNOSTIC_JOURNAL='1',
                             SOTC_SHADER_PROVENANCE='0' if mode=='fallback-disabled' else '1'))
                self.assertEqual(result.returncode,53 if mode=='fallback-exit' else 0,result.stderr)
                if mode=='fallback-disabled':
                    self.assertFalse((state/'shader-lookup-trace').exists());continue
                evidence=analyze(state/'shader-events.bin','shader')
                self.assertEqual(evidence['state'],'readable')
                counters=evidence['lookup_capture']
                if mode=='fallback-existing':
                    self.assertEqual((preserved/'code.bin').read_bytes(),b'preserve existing capture')
                    self.assertEqual(counters['write_failures'],1);continue
                expected=8 if mode in ('fallback-cap','fallback-types') else 1
                folders=list((state/'shader-lookup-trace').glob('case-*'))
                self.assertEqual(len(folders),expected)
                self.assertEqual(counters['admitted'],expected)
                self.assertEqual(counters['completed'],expected)
                self.assertEqual(counters['refused'],4 if expected==8 else 0)
                self.assertEqual(counters['duplicates'],19 if mode=='fallback-duplicate' else 0)
                for folder in folders:
                    data=(folder/'code.bin').read_bytes()
                    self.assertEqual(len(data),4096)
                    self.assertEqual(data[:8],bytes(8))
                    metadata=(folder/'metadata.bin').read_bytes()
                    self.assertEqual(data[512:512+len(metadata)],metadata)
                    meta=dict(line.split('=',1) for line in (folder/'context.txt').read_text().splitlines())
                    self.assertEqual(meta['scan_head_matches_lookup'],'1')
                    self.assertEqual(meta['association_validated'],'0')
                    self.assertEqual(meta['accepted'],'0')
                from analyze_diagnostic_run import collect
                log=state/'synthetic.log';log.write_text('')
                cases=collect(log,state)['shader_lookup_trace']['cases']
                self.assertEqual(len(cases),expected)
                self.assertTrue(all(c['state']=='readable' and c['metadata_matches_scan'] for c in cases))
                if mode=='fallback-exit':
                    first=state/'shader-lookup-trace/case-0'
                    meta=dict(line.split('=',1) for line in (first/'context.txt').read_text().splitlines())
                    decoded=state/'shader-decode-failure';decoded.mkdir()
                    (decoded/'code.bin').write_bytes((first/'code.bin').read_bytes()[:32])
                    (decoded/'context.txt').write_text('origin='+meta['origin']+'\nshader='+meta['shader']+'\nsaved_start_word=0\n')
                    matched=collect(log,state)['shader_decode_snapshot']['lookup_candidate_comparisons']
                    self.assertEqual(matched,[dict(case=0,decode_window_covered=True,decode_window_matches_scan=True)])
                    (decoded/'code.bin').write_bytes(b'X'*32)
                    matched=collect(log,state)['shader_decode_snapshot']['lookup_candidate_comparisons']
                    self.assertFalse(matched[0]['decode_window_matches_scan'])
                log.write_text('GPU supervisor: child_pid=1\n')
                cases=collect(log,state)['shader_lookup_trace']['cases']
                self.assertTrue(all(c['state']=='foreign_process' for c in cases))

if __name__=='__main__':
    unittest.main()
