"""Original guest pointer-chain fault and actual imported writers; no game/GPU."""
import json
import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_diagnostic_run import collect
from check_pointer_profile import check


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class PointerTraceTests(unittest.TestCase):
    def image(self,path,mode):
        image=bytearray(16384)
        # Original synthetic container -> header -> node, followed by a flag
        # read. A real call creates the continuation block where probes run.
        image[:12]=bytes.fromhex('4c8d2d')+struct.pack('<i',512-7)+bytes.fromhex('e8')+struct.pack('<i',64-12)
        consumer=bytes.fromhex('498b4518 488b7008 807e1900 c3')
        image[12:12+len(consumer)]=consumer
        copy=bytearray()
        dst=648+(6 if mode=='partial' else 7 if mode=='tagged' else 48 if mode=='nonoverlap' else 0)
        copy+=bytes.fromhex('488d3d')+struct.pack('<i',dst-(64+7))
        if mode=='memset':copy+=bytes.fromhex('beffffffff9090')
        else:copy+=bytes.fromhex('488d35')+struct.pack('<i',900-(64+14))
        count=0 if mode in ('zero','initial') else 1 if mode in ('partial','tagged') else 8
        copy+=b'\xba'+struct.pack('<I',count)
        copy+=bytes.fromhex('488b05')+struct.pack('<i',256-(64+26))
        copy+=bytes.fromhex('4883ec08 ffd0 4883c408 c3')
        bad=0x123456789abcdef0
        if mode=='direct':
            copy=bytes.fromhex('48b8')+struct.pack('<Q',bad)+bytes.fromhex('488d3d')+struct.pack('<i',648-(64+17))+bytes.fromhex('488907 c3')
        if mode=='restored':
            copy=bytearray(b'\x53') # Preserve RBX and align the guest call stack.
            def invoke(source):
                copy.extend(bytes.fromhex('488d3d')+struct.pack('<i',648-(64+len(copy)+7)))
                copy.extend(bytes.fromhex('488d35')+struct.pack('<i',source-(64+len(copy)+7)))
                copy.extend(b'\xba'+struct.pack('<I',8))
                copy.extend(bytes.fromhex('488b05')+struct.pack('<i',256-(64+len(copy)+7)))
                copy.extend(bytes.fromhex('ffd0'))
            invoke(900)
            copy.extend(b'\xbb'+struct.pack('<I',600));loop=len(copy);invoke(920)
            copy.extend(bytes.fromhex('ffcb75')+struct.pack('<b',loop-(len(copy)+4)))
            copy.extend(bytes.fromhex('5bc3'))
        if mode.startswith('churn'):
            copy=bytearray(b'\x53')
            def rip(op,target):
                copy.extend(bytes.fromhex(op));copy.extend(struct.pack('<i',target-(64+len(copy)+4)))
            rip('488d3d',1024);copy.extend(b'\xbb'+struct.pack('<I',64));loop=len(copy)
            rip('e8',320);copy.extend(bytes.fromhex('4883c708 ffcb75')+struct.pack('<b',loop-(len(copy)+8)))
            rip('488d3d',654 if mode=='churnpartial' else 648)
            rip('488d35',2048 if mode=='churnlarge' else 900)
            copy.extend(b'\xba'+struct.pack('<I',1024 if mode=='churnlarge' else 1 if mode=='churnpartial' else 8))
            rip('488b05',256);copy.extend(bytes.fromhex('ffd0 5b c3'));image[320]=0xc3
        if mode=='indexoverflow':
            copy=bytearray(bytes.fromhex('53 48bb')+struct.pack('<Q',(1<<64)-1))
            copy.extend(b'\xe8'+struct.pack('<i',320-(64+len(copy)+5)))
            copy.extend(bytes.fromhex('5b c3'));image[320]=0xc3
        image[64:64+len(copy)]=copy
        if mode in ('getterbad','gettervalid','getterexact','trap'):
            image[:12]=bytes.fromhex('488d3d')+struct.pack('<i',512-7)+bytes.fromhex('e8')+struct.pack('<i',64-12)
            image[12:19]=bytes.fromhex('8b88a8000000c3')
            image[64:69]=bytes.fromhex('488b4720c3')
            image[544:552]=struct.pack('<Q',bad)
            if mode=='getterexact':image[12:16]=bytes.fromhex('488b08c3')
            if mode=='trap':image[12:15]=bytes.fromhex('cd41c3')
        image[900:908]=struct.pack('<Q',bad)
        if mode in ('partial','tagged','churnpartial'):image[900]=0x12
        if mode=='churnlarge':image[2048:2056]=struct.pack('<Q',bad)
        name=b'8zTFvBIAIN8#1#u' if mode=='memset' else b'Q3VBxCXhUHs#1#u'
        relocs=[(536,0,640,0),(648,0,768,0),(656,0,512,0),(256,1,0,0)]
        if mode=='gettervalid':relocs.append((544,0,768,0))
        if mode=='initial':
            relocs.remove((648,0,768,0));image[648:656]=struct.pack('<Q',bad)
        if mode in ('valid','zero','nonoverlap'):relocs.append((900,0,768,0))
        if mode=='restored':relocs.append((920,0,768,0))
        path.write_bytes(struct.pack('<8s6Q',b'BBPROBE2',len(image),0,2,len(relocs),1,1)+
            struct.pack('<6Q',0,512,5,512,len(image)-512,6)+name.ljust(128,b'\0')+
            b''.join(struct.pack('<4Q',*row) for row in relocs)+image)
        return image

    def run_case(self,root,mode,jit,selection='7:13:24:8,12:13:24:8',stderr_mode='pipe',field_selection='0:4:0',scenario_profile=False):
        state=root/(mode+'-'+jit+'-'+stderr_mode);state.mkdir();image=state/'fixture.bin';code=self.image(image,mode)
        if scenario_profile:
            def signatures(config):
                return [dict(site=str(at),bytes=bytes(code[at:at+8]).hex()) for at in sorted({int(s.split(':')[0],0) for s in config.split(',')})]
            profile=state/'profile.json';profile.write_text(json.dumps(dict(selection=selection,signatures=signatures(selection),
                field_selection=field_selection,field_signatures=signatures(field_selection),scenarios=[
                    dict(name='scalar boundaries',cpu=list(range(len(field_selection.split(','))))),
                    dict(name='pointer boundaries',pointer=list(range(len(selection.split(',')))))])))
            checked=subprocess.run([sys.executable,str(ROOT/'tools/check_pointer_profile.py'),'--image',str(image),
                '--profile',str(profile),'--selection',selection,'--field-selection',field_selection,
                '--output',str(state/'pointer-profile-check.json')],capture_output=True,text=True)
            self.assertEqual(checked.returncode,0,checked.stderr)
        env=dict(os.environ,BB_JIT=jit,BB_DMEM_MB='16',BB_DMEM_EXTRA_MB='0',BB_GPU_USER_DIR=str(state),
                 SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_CPU_FIELD_TRACE=field_selection,SOTC_CPU_FAULT_FIELDS='',
                 SOTC_CPU_POINTER_TRACE=selection,SOTC_DIAGNOSTIC_SECONDS='5')
        log=state/'run.log'
        if stderr_mode=='pipe':
            result=subprocess.run([sys.executable,str(ROOT/'tools/run_gpu_diagnostic.py'),'--seconds','5','--log',str(log),
                '--',str(ROOT/'work/probe_entry_arm64'),str(image),'--bootstrap'],env=env,capture_output=True,text=True,timeout=10)
        else:
            result=subprocess.run([str(ROOT/'work/probe_entry_arm64'),str(image),'--bootstrap'],env=env,
                stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
            log.write_text('')
        return result,collect(log,state)

    def test_actual_copy_and_direct_store_faults_are_distinguished_in_both_modes(self):
        with tempfile.TemporaryDirectory(prefix='pointer-trace-',dir=ROOT/'work') as folder:
            root=Path(folder)
            for jit in ('1','0'):
                for mode in ('memcpy','memset','partial','direct'):
                    with self.subTest(jit=jit,mode=mode):
                        result,report=self.run_case(root,mode,jit)
                        self.assertEqual(result.returncode,139,result.stderr)
                        pointers=report['pre_failure_journals']['pointer']
                        self.assertEqual(pointers['state'],'readable')
                        self.assertEqual(pointers['target_probe_hits'],[1,1])
                        focus=pointers['first_out_of_range_link']
                        self.assertGreaterEqual(focus['value'],1<<40)
                        copies=[e for e in pointers['events'] if e['actor']!=1]
                        if mode=='direct':
                            self.assertEqual(copies,[]);self.assertEqual(focus['actor'],1)
                            self.assertIn('not intercepted',pointers['writer_coverage'])
                        else:
                            expected=[4,5] if mode=='memset' else [2,3]
                            self.assertEqual([e['actor'] for e in copies],expected)
                            self.assertLess(copies[0]['value'],1<<40)
                            self.assertEqual(copies[1]['value'],focus['value'])
                            self.assertEqual(copies[0]['write_source'],copies[1]['write_source'])
                            self.assertEqual(copies[1]['source_kind'],'fill_byte' if mode=='memset' else 'source_pointer')
                            self.assertEqual(copies[1]['status']&2,0) # No fresh container/header ownership read.
                            self.assertEqual(focus['actor'],expected[-1])
                            review=report['object_review']['copy_observation']
                            self.assertTrue(review['field_overlaps_copy'])
                            self.assertTrue(review['observed_value_changed'])
                            self.assertIn('earlier allocation',review['limitation'])
                            if mode=='partial':self.assertEqual(copies[1]['write_bytes'],1)
                        self.assertEqual(pointers['incomplete_or_stale'],0)
                result,report=self.run_case(root,'memcpy',jit,stderr_mode='discarded')
                self.assertEqual(result.returncode,139)
                self.assertIn('first_out_of_range_link',report['pre_failure_journals']['pointer'])

    def test_valid_zero_nonoverlap_and_unreached_probes_do_not_invent_a_writer(self):
        with tempfile.TemporaryDirectory(prefix='pointer-controls-',dir=ROOT/'work') as folder:
            root=Path(folder)
            for mode in ('valid','zero','nonoverlap','tagged'):
                result,report=self.run_case(root,mode,'1')
                # Returning from the synthetic entry is the unchanged harness exit 1.
                self.assertEqual(result.returncode,1,result.stderr)
                pointers=report['pre_failure_journals']['pointer']
                self.assertNotIn('first_out_of_range_link',pointers)
                self.assertEqual(pointers['target_probe_hits'],[1,1])
                if mode in ('zero','nonoverlap'):self.assertEqual(pointers['overlap_events'],0)
            result,report=self.run_case(root,'direct','1',selection='128:13:24:8')
            self.assertEqual(result.returncode,139,result.stderr)
            self.assertEqual(report['pre_failure_journals']['pointer']['target_probe_hits'],[0])
            self.assertTrue(any('never observed' in gap for gap in report['evidence_gaps']))

    def test_register_only_capture_never_reads_or_watches_values(self):
        with tempfile.TemporaryDirectory(prefix='register-capture-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                result,report=self.run_case(Path(folder),'getterbad',jit,
                    selection='64:7:0:0:2:2,68:0:0:0:2:2,12:0:0:0:2:2')
                self.assertEqual(result.returncode,139,result.stderr)
                p=report['pre_failure_journals']['pointer']
                self.assertEqual(p['target_probe_hits'],[1,1,1])
                self.assertEqual(p['watches_armed'],0)
                self.assertNotIn('first_out_of_range_link',p)
                rows=p['register_observations'];self.assertEqual(len(rows),3)
                self.assertTrue(all(e['probe_kind']=='register_value' and e['field']==0 and e['source_field']==0 for e in rows))
                self.assertEqual(rows[-1]['value'],0x123456789abcdef0)

    def test_guest_interrupt_keeps_exit_and_full_pre_exit_context(self):
        with tempfile.TemporaryDirectory(prefix='guest-interrupt-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                result,report=self.run_case(Path(folder),'trap',jit,selection='64:7:0:0:2:2')
                self.assertEqual(result.returncode,30,result.stderr)
                terminal=report['pre_failure_journals']['cpu']['terminal_context']
                self.assertEqual(terminal['kind'],3)
                self.assertEqual(terminal['source'],'instruction_failure')
                self.assertEqual(terminal['registers']['rax'],0x123456789abcdef0)
                self.assertEqual(len(terminal['registers']),16)
                failure=report['current']['guest_data_failures']['instruction_failures'][0]
                self.assertEqual(failure['interrupt_vector'],0x41)
                self.assertTrue(report['current']['fatal_reporting']['guest_context_complete'])

    def test_returned_pointer_is_observed_before_dereference_with_fourteen_channels(self):
        with tempfile.TemporaryDirectory(prefix='pointer-getter-',dir=ROOT/'work') as folder:
            selection=','.join(['64:7:32:0:1:1','68:7:32:0:1:1','12:0:168:0:1:2']+
                [f'64:7:{offset}:0:1:2' for offset in (0,8,16,24,40,48,56,64,72,80,88)])
            for jit in ('1','0'):
                for mode in ('gettervalid','getterbad','getterexact'):
                    result,report=self.run_case(Path(folder),mode,jit,selection=selection,scenario_profile=True)
                    self.assertEqual(result.returncode,1 if mode=='gettervalid' else 139,result.stderr)
                    pointers=report['pre_failure_journals']['pointer']
                    self.assertEqual(pointers['target_probe_hits'],[1]*14)
                    events={e['channel']:e for e in pointers['events'] if e['actor']==1}
                    self.assertEqual(events[0]['field'],events[1]['field'])
                    self.assertEqual(events[0]['value'],events[1]['value'])
                    self.assertEqual(events[2]['object'],events[1]['value'])
                    self.assertEqual(bool(events[2]['status']&8),mode=='gettervalid')
                    self.assertEqual(pointers['overlap_events'],0)
                    self.assertEqual(pointers['admission_refusals_by_probe'],[0]*14)
                    fault_review=report.get('fault_pointer_review',{})
                    self.assertEqual(len(fault_review.get('matching_source_observations',[])),2 if mode=='getterexact' else 0)
                    if not report['current']['fatal_reporting']['guest_context_complete']:
                        self.assertEqual(fault_review.get('terminal_register_source_candidates',[]),[])
                    self.assertTrue(all(t['state']=='observed' for s in report['analysis_scenarios'] for t in s['targets']))

    def test_profile_rejects_code_or_selection_changes_before_execution(self):
        with tempfile.TemporaryDirectory(prefix='pointer-profile-',dir=ROOT/'work') as folder:
            root=Path(folder);image=root/'fixture.bin';code=self.image(image,'direct');profile=root/'profile.json'
            selection='7:13:24:8,12:13:24:8'
            spec=dict(selection=selection,signatures=[dict(site=str(at),bytes=bytes(code[at:at+8]).hex()) for at in (7,12)])
            profile.write_text(json.dumps(spec))
            self.assertEqual(check(image,profile,selection),2)
            saved=root/'pointer-profile-check.json'
            run=subprocess.run([sys.executable,str(ROOT/'tools/check_pointer_profile.py'),
                '--image',str(image),'--profile',str(profile),'--selection',selection,'--output',str(saved)],capture_output=True,text=True,timeout=5)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertTrue(json.loads(saved.read_text())['validated_before_launch'])
            log=root/'run.log'
            log.write_text('GPU supervisor: settings='+json.dumps(dict(SOTC_CPU_POINTER_TRACE=selection,SOTC_REQUIRE_POINTER_TRACE='1'))+'\n')
            self.assertEqual(collect(log,root)['pointer_profile_check']['status'],'matched')
            log.write_text('GPU supervisor: settings='+json.dumps(dict(SOTC_CPU_POINTER_TRACE='different'))+'\n')
            self.assertTrue(any('selection differs' in gap for gap in collect(log,root)['evidence_gaps']))
            with self.assertRaises(ValueError):check(image,profile,selection+',1:7:0:8')
            data=bytearray(image.read_bytes());data[-16384+12]^=1;image.write_bytes(data)
            with self.assertRaises(ValueError):check(image,profile,selection)

    def test_first_bad_link_survives_restoration_and_wrapped_copy_channels(self):
        with tempfile.TemporaryDirectory(prefix='pointer-first-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                result,report=self.run_case(Path(folder),'restored',jit)
                self.assertEqual(result.returncode,1,result.stderr)
                pointers=report['pre_failure_journals']['pointer']
                focus=pointers['first_out_of_range_link']
                self.assertEqual(focus['actor'],3)
                self.assertGreaterEqual(focus['value'],1<<40)
                copies=[e for e in pointers['events'] if e['actor']!=1]
                self.assertTrue(copies)
                self.assertTrue(all(e['value']<1<<40 for e in copies))
                self.assertTrue(any(c['overwritten'] for c in pointers['channels']))
                self.assertLess(focus['ns'],copies[-1]['ns'])

    def test_pointer_field_does_not_chase_bad_value_and_all_channels_survive(self):
        with tempfile.TemporaryDirectory(prefix='pointer-fields-',dir=ROOT/'work') as folder:
            # Two chain observations and twelve direct/source fields
            # fields share actual block entries. All 28 scalar channels remain
            # independent of the terminal register space in the same header.
            selection='64:13:24:8,12:13:24:8,64:13:24:0:1,12:13:24:0:1,64:13:136:0:1,12:13:136:0:1'
            selection+=','+','.join(f'12:13:{offset}:0:1:2' for offset in (144,152,160,168,176,184,192,200))
            fields=','.join(f'64:13:{i*8}:0' for i in range(28))
            for jit in ('1','0'):
                for mode in ('memcpy','direct','initial','valid'):
                    with self.subTest(jit=jit,mode=mode):
                        result,report=self.run_case(Path(folder),mode,jit,selection=selection,field_selection=fields,scenario_profile=True)
                        self.assertEqual(result.returncode,1 if mode=='valid' else 139,result.stderr)
                        cpu=report['pre_failure_journals']['cpu'];pointers=report['pre_failure_journals']['pointer']
                        self.assertEqual([c['attempted'] for c in cpu['channels']],[1]*28)
                        self.assertEqual(pointers['target_probe_hits'],[1]*14)
                        self.assertEqual(len(report['analysis_scenarios']),2)
                        self.assertTrue(all(t['state']=='observed' for s in report['analysis_scenarios'] for t in s['targets']))
                        self.assertEqual(cpu['incomplete_or_stale'],0)
                        direct=[e for e in pointers['events'] if e['actor']==1 and e['status']&512]
                        self.assertEqual(len(direct),12)
                        self.assertTrue(all(e['header']==0 and e['source_field']==e['field'] and e['status']&15==15 for e in direct))
                        if mode=='valid':self.assertNotIn('first_out_of_range_link',pointers)
                        else:
                            if jit=='1':
                                self.assertEqual(cpu['terminal_context']['kind'],2)
                                self.assertEqual(len(cpu['terminal_context']['registers']),16)
                            else:
                                self.assertEqual(cpu['terminal_context']['kind'],4)
                                self.assertEqual(cpu['terminal_context']['source'],'interpreted_fault')
                                self.assertEqual(len(cpu['terminal_context']['registers']),16)
                            self.assertIn('first_out_of_range_link',pointers)
                        if mode in ('direct','initial'):
                            self.assertEqual(pointers['overlap_events'],0)
                            self.assertEqual(pointers['first_out_of_range_link']['actor'],1)
                        if mode=='initial':
                            self.assertEqual(pointers['first_out_of_range_link']['probe_kind'],'pointer_field')
                        if mode=='memcpy':
                            copies=[e for e in pointers['events'] if e['actor']!=1]
                            self.assertEqual([e['actor'] for e in copies],[2,3])
                            self.assertEqual(copies[1]['probe_kind'],'pointer_field')
            negative=Path(folder)/'unreached';negative.mkdir()
            result,report=self.run_case(negative,'direct','1',selection='128:13:24:8',scenario_profile=True)
            self.assertEqual(result.returncode,139,result.stderr)
            self.assertEqual(report['analysis_scenarios'][1]['targets'][0]['state'],'unreached')

    def test_field_profile_and_scenario_coverage_cannot_silently_use_other_selection(self):
        with tempfile.TemporaryDirectory(prefix='pointer-scenarios-',dir=ROOT/'work') as folder:
            root=Path(folder);image=root/'fixture.bin';code=self.image(image,'valid');profile=root/'profile.json'
            selection='7:13:136:0:1';fields='7:13:0:0'
            spec=dict(selection=selection,signatures=[dict(site='7',bytes=bytes(code[7:15]).hex())],
                field_selection=fields,field_signatures=[dict(site='7',bytes=bytes(code[7:15]).hex())],
                scenarios=[dict(name='original source field',cpu=[0],pointer=[0])])
            profile.write_text(json.dumps(spec))
            self.assertEqual(check(image,profile,selection,fields),1)
            for other in (None,'7:13:8:0'):
                with self.assertRaises(ValueError):check(image,profile,selection,other)
            spec['field_signatures']=[];profile.write_text(json.dumps(spec))
            with self.assertRaises(ValueError):check(image,profile,selection,fields)
            spec['field_signatures']=[dict(site='7',bytes=bytes(code[7:15]).hex())];profile.write_text(json.dumps(spec))
            saved=root/'pointer-profile-check.json'
            run=subprocess.run([sys.executable,str(ROOT/'tools/check_pointer_profile.py'),'--image',str(image),
                '--profile',str(profile),'--selection',selection,'--field-selection',fields,'--output',str(saved)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            log=root/'run.log';settings=dict(SOTC_CPU_POINTER_TRACE=selection,SOTC_CPU_FIELD_TRACE=fields)
            log.write_text('GPU supervisor: settings='+json.dumps(settings)+'\n')
            report=collect(log,root)
            self.assertEqual([t['state'] for t in report['analysis_scenarios'][0]['targets']],['unavailable']*2)
            settings['SOTC_CPU_FIELD_TRACE']='7:13:8:0';log.write_text('GPU supervisor: settings='+json.dumps(settings)+'\n')
            self.assertTrue(any('field profile selection differs' in gap for gap in collect(log,root)['evidence_gaps']))

    def test_persistent_source_survives_hot_fields_and_keeps_previous_value(self):
        with tempfile.TemporaryDirectory(prefix='pointer-persistent-',dir=ROOT/'work') as folder:
            root=Path(folder)
            for jit in ('1','0'):
                for mode in ('churn','churnpartial','churnlarge'):
                    result,report=self.run_case(root,mode,jit,selection='64:13:136:0:1:1,320:7:0:0:1:0,12:13:136:0:1:1')
                    self.assertEqual(result.returncode,139,result.stderr)
                    p=report['pre_failure_journals']['pointer']
                    self.assertEqual(p['target_probe_hits'],[1,64,1])
                    self.assertGreater(p['watch_slot_replacements'],0)
                    self.assertEqual(p['persistent_watches']['admitted'],1)
                    self.assertEqual(p['persistent_watches']['admission_refusals'],0)
                    focus=p['first_out_of_range_link']
                    self.assertEqual(focus['actor'],3)
                    self.assertEqual(focus['watch_policy'],'persistent')
                    self.assertTrue(focus['watch_admitted'])
                    previous=focus['previous_observation'];self.assertTrue(previous['available'])
                    self.assertLess(previous['value'],1<<40)
                    self.assertLessEqual(previous['ns'],focus['ns'])
                    source_copies=[e for e in p['events'] if e['actor']!=1 and e['watch_policy']=='persistent']
                    self.assertEqual([e['actor'] for e in source_copies],[2,3])
                    if mode=='churnpartial':self.assertEqual(source_copies[1]['write_bytes'],1)
            negative=root/'rotating';negative.mkdir()
            result,report=self.run_case(negative,'churn','1',selection='64:13:136:0:1:0,320:7:0:0:1:0,12:13:136:0:1:0')
            self.assertEqual(result.returncode,139,result.stderr)
            self.assertEqual(report['pre_failure_journals']['pointer']['overlap_events'],0)
            self.assertEqual(report['pre_failure_journals']['pointer']['first_out_of_range_link']['actor'],1)
            for mode in ('initial','direct'):
                negative=root/('persistent-'+mode);negative.mkdir()
                result,report=self.run_case(negative,mode,'1',selection='64:13:136:0:1:1,12:13:136:0:1:1')
                self.assertEqual(result.returncode,139,result.stderr)
                p=report['pre_failure_journals']['pointer'];focus=p['first_out_of_range_link']
                self.assertEqual(p['overlap_events'],0);self.assertEqual(focus['actor'],1)
                self.assertEqual(focus['previous_observation']['available'],mode=='direct')

    def test_observe_only_hot_helpers_leave_source_watch_capacity_available(self):
        with tempfile.TemporaryDirectory(prefix='pointer-helper-isolation-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                result,report=self.run_case(Path(folder),'churn',jit,
                    selection='64:13:136:0:1:1,320:7:0:0:1:2,12:13:136:0:1:1')
                self.assertEqual(result.returncode,139,result.stderr)
                p=report['pre_failure_journals']['pointer']
                self.assertEqual(p['target_probe_hits'],[1,64,1])
                self.assertEqual(p['persistent_watches']['admitted'],1)
                self.assertEqual(p['admission_refusals_by_probe'],[0,0,0])
                self.assertEqual(p['watches_armed'],1)
                self.assertEqual(p['watch_slot_replacements'],0)
                self.assertTrue(all(not e['watch_admitted'] for e in p['events'] if e['channel']==1))
                self.assertEqual([e['actor'] for e in p['events'] if e['actor']!=1],[2,3])
                self.assertEqual(p['first_out_of_range_link']['actor'],3)

    def test_grouped_watches_capture_small_partial_and_large_copies(self):
        with tempfile.TemporaryDirectory(prefix='pointer-banks-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                for mode in ('churn','churnpartial','churnlarge'):
                    result,report=self.run_case(Path(folder),mode,jit,
                        selection='64:13:136:0:1:1@3,320:7:0:0:1:1@1,12:13:136:0:1:1@3')
                    self.assertEqual(result.returncode,139,result.stderr)
                    p=report['pre_failure_journals']['pointer']
                    self.assertEqual([g['admitted'] for g in p['watch_groups']],[0,64,0,1])
                    copies=[e for e in p['events'] if e['actor']!=1 and e['field']==p['first_out_of_range_link']['field']]
                    self.assertEqual([e['actor'] for e in copies],[2,3])
                    self.assertTrue(all(e['watch_group']==3 for e in copies))
                    self.assertTrue(all(g['admission_refusals']==0 for g in p['watch_groups']))

    def test_observe_only_indexed_reference_and_overflow_do_not_arm_watches(self):
        with tempfile.TemporaryDirectory(prefix='pointer-indexed-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                # RBX is zero at the original synthetic callee entry. R13+24
                # contains the header; header+8 is its node reference.
                result,report=self.run_case(Path(folder),'valid',jit,selection='64:13:24:8:0:2:3:8,12:13:136:0:1:2')
                self.assertEqual(result.returncode,1,result.stderr)
                p=report['pre_failure_journals']['pointer'];self.assertEqual(p['watches_armed'],0)
                self.assertEqual(p['persistent_watches']['admitted'],0)
                indexed=p['events'][0];self.assertTrue(indexed['indexed_chain'])
                self.assertEqual(indexed['status']&15,15)
                self.assertEqual(indexed['watch_policy'],'observe_only')
                self.assertFalse(indexed['watch_admitted'])
                self.assertEqual(indexed['index'],dict(register=3,value=0,scale=8))
                self.assertEqual(indexed['value'],p['events'][1]['value'])
            negative=Path(folder)/'overflow';negative.mkdir()
            # R13 holds an image pointer: multiplying it by eight still fits,
            # but adds an unmapped address; no pointed-to value is fabricated.
            result,report=self.run_case(negative,'valid','1',selection='64:13:24:8:0:2:13:8')
            self.assertEqual(result.returncode,1,result.stderr)
            self.assertEqual(report['pre_failure_journals']['pointer']['events'][0]['status']&8,0)
            negative=Path(folder)/'multiply-overflow';negative.mkdir()
            result,report=self.run_case(negative,'indexoverflow','1',selection='320:13:24:8:0:2:3:8')
            self.assertEqual(result.returncode,1,result.stderr)
            self.assertEqual(report['pre_failure_journals']['pointer']['target_probe_hits'],[1])
            self.assertEqual(report['pre_failure_journals']['pointer']['events'][0]['status']&12,0)

    def test_object_reference_candidates_are_numeric_not_allocation_ownership(self):
        with tempfile.TemporaryDirectory(prefix='pointer-owner-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                result,report=self.run_case(Path(folder),'memcpy',jit,selection='64:13:24:16:0:2:3:8,64:13:136:0:1:1,12:13:136:0:1:1')
                self.assertEqual(result.returncode,139,result.stderr)
                review=report['object_review'];refs=review['retained_reference_candidates']
                self.assertEqual(len(refs),1);self.assertTrue(refs[0]['indexed_chain'])
                self.assertEqual(refs[0]['value'],review['suspicious']['object'])
                self.assertIn('not validated ownership',review['limitation'])

    def test_persistent_hash_capacity_refusal_and_concurrent_observations(self):
        with tempfile.TemporaryDirectory(prefix='pointer-capacity-',dir=ROOT/'work') as folder:
            binary=Path(folder)/'watch'
            built=subprocess.run(['clang','-arch','arm64','-std=gnu11','-O1','-pthread',
                '-fsanitize=undefined,bounds','-I'+str(ROOT/'runtime/src'),'-I'+str(ROOT/'runtime/upstream/src'),
                '-I'+str(ROOT/'runtime/upstream/recomp/src/cpu'),str(ROOT/'tests/test_pointer_watch.c'),
                str(ROOT/'runtime/src/platform_arm64.c'),'-o',str(binary)],capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stderr)
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=10)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertIn('PASS:',run.stdout)


if __name__=='__main__':unittest.main()
