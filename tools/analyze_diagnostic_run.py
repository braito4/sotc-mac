"""Combine private CPU/GPU evidence offline, keeping distinct stops and gaps explicit."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from analyze_cpu_field_trace import analyze as cpu_analysis
from analyze_shader_failures import analyze as shader_analysis
from analyze_host_fault import analyze as host_analysis
from analyze_index_capture import review as index_capture_review
from analyze_shader_bind import review as shader_bind_review
from analyze_shader_consume import review as shader_consume_review, mapping_review as shader_mapping_review
from analyze_shader_backing import review as shader_backing_review
from analyze_shader_copy import review as shader_copy_review,activity as shader_copy_activity
from analyze_shader_watch_summary import review as shader_watch_summary
from analyze_shader_watch import review as shader_watch_review, transition_review as shader_watch_transitions
from analyze_gpu_submission import analyze as gpu_analysis
from analyze_event_journal import analyze as event_journal
from analyze_event_journal import shader_write_observation

ROOT = Path(__file__).resolve().parents[1]


def shader_lookup_captures(state, child_pid=None):
    """First bounded fallback scans; association is never inferred from a signature."""
    cases=[]
    for index in range(8):
        folder=state/'shader-lookup-trace'/f'case-{index}'
        if not folder.exists():continue
        row=dict(case=index,state='incomplete')
        try:
            meta=dict(line.split('=',1) for line in (folder/'context.txt').read_text().splitlines() if '=' in line)
            if meta.get('schema')!='1':raise ValueError('unsupported lookup capture schema')
            row['context']=meta
            if child_pid is not None and int(meta['producer_pid'])!=child_pid:
                row['state']='foreign_process';cases.append(row);continue
            data=(folder/'code.bin').read_bytes();descriptor=(folder/'descriptor.bin').read_bytes()
            metadata=(folder/'metadata.bin').read_bytes()
            if not 8<=len(data)<=65596 or len(data)!=int(meta['scanned']):raise ValueError('invalid scan snapshot extent')
            if not 1<=len(descriptor)<=512 or len(descriptor)!=int(meta['descriptor_bytes']):raise ValueError('invalid descriptor extent')
            if not 1<=len(metadata)<=64 or len(metadata)!=int(meta['metadata_bytes']):raise ValueError('invalid metadata extent')
            offset=int(meta['metadata_offset'])
            if not 0<=offset<=len(data)-len(metadata):raise ValueError('metadata outside saved scan')
            head=list(struct.unpack_from('<2I',data))
            row.update(state='readable',scan_sha256=hashlib.sha256(data).hexdigest(),scan_bytes=len(data),
                descriptor_sha256=hashlib.sha256(descriptor).hexdigest(),metadata_sha256=hashlib.sha256(metadata).hexdigest(),
                metadata_matches_scan=data[offset:offset+len(metadata)]==metadata,
                scan_head_matches_lookup=head==[int(meta['lookup_head0']),int(meta['lookup_head1'])],
                expected_prologue_present=head[0]==0xbeeb03ff,
                association_validated=False)
            row['runtime_accepted']=bool(int(meta['accepted'])) if 'accepted' in meta else None
            row['metadata_association']={0:'unavailable',1:'bound_to_prologue',2:'unbound_missing_prologue',3:'unbound_invalid_hint',4:'unbound_hint_metadata'}.get(int(meta.get('association_status','-1')),'unknown')
            provenance=folder/'provenance.txt'
            if provenance.is_file():row['provenance']=shader_provenance(provenance.read_text(errors='replace'))
        except (OSError,ValueError,KeyError,struct.error) as error:row['reason']=str(error)
        cases.append(row)
    return dict(cases=cases,capacity=8,
        limitation='First distinct fallback candidates, not all compilations. Descriptor bytes are a separate non-atomic CPU observation; saved scan is not proof of a valid program, allocation identity or writer.')


def shader_origin_review(events, address, program=None, thread=None):
    lookups=[e for e in events if e.get('kind')==2 and e.get('address')==address
             and (program is None or e.get('program')==program)
             and (thread is None or not e.get('thread') or e['thread']==thread)]
    if not lookups:return dict(state='lookup_absent')
    lookup=max(lookups,key=lambda e:e['seq'])
    writes=[e for e in events if e.get('kind')==1 and e.get('program')==lookup['program'] and e['seq']<lookup['seq']
            and (not e.get('thread') or not lookup.get('thread') or e['thread']==lookup['thread'])]
    row=dict(state='write_absent',lookup=lookup,
        limitation='Nearest retained same-program/same-thread write when thread evidence exists; separate CPU observations, not atomic guest content, validated ownership or original producer/writer proof.')
    if not writes:return row
    complete=[e for e in writes if e.get('write_observation',{}).get('packet_origin') is not None]
    if complete:
        row['address_write']=max(complete,key=lambda e:e['seq'])
    write=max(writes,key=lambda e:e['seq']);observed=write.get('write_observation')
    row.update(state='observed' if observed else 'legacy_write',write=write)
    if observed:
        row.update(packet_matches_lookup=(observed['packet_origin']==address) if observed['packet_origin'] is not None else None,
                   register_matches_lookup=(observed['register_origin']==address) if observed['register_origin'] is not None else None)
    if lookup.get('ns') and write.get('ns') and lookup['ns']>=write['ns']:
        row['observed_interval_us']=(lookup['ns']-write['ns'])/1000
    return row


def shader_origin_with_fallback(events, fallback, address, program=None, thread=None):
    """Use a saved program snapshot if the general journal lost its lookup.

    Do not merge independent sequence domains or invent missing timestamps.
    """
    result=shader_origin_review(events,address,program,thread)
    result['evidence_source']='pre_failure_journal'
    if result['state']=='lookup_absent':
        saved=shader_origin_review(fallback,address,program,thread)
        if saved['state']!='lookup_absent':
            saved.update(evidence_source='failure_program_snapshot',
                         timing_available=bool(saved.get('lookup',{}).get('ns')))
            return saved
    return result


def shader_submission_index(path, address):
    """Read the bounded failure-time index without guest memory access."""
    path=Path(path);result=dict(state='absent',matches=[],limitation=
        'Latest bounded pre-enqueue observations sharing the numeric origin. Packet/capture matches do not prove allocation identity, intended shader identity, continuous lifetime or an original writer.')
    if not path.is_file():return result
    try:
        if path.stat().st_size>16384:raise ValueError('oversized submission index snapshot')
        rows=dict(line.split('=',1) for line in path.read_text().splitlines() if '=' in line)
        if rows.get('schema')!='1' or int(rows['address'])!=address:raise ValueError('invalid submission index identity')
        status=int(rows['state']);capacity=int(rows['capacity']);ways=int(rows['ways']);count=int(rows['matches'])
        if status not in (0,1,2,5) or capacity!=4096 or ways!=4 or not 0<=count<=ways or (status==5)!=(count>0):
            raise ValueError('invalid submission index layout')
        result.update(state={0:'no_observations',1:'empty_bucket',2:'address_not_retained',5:'matched'}[status],
            total=int(rows['total']),replacements=int(rows['replacements']),capacity=capacity,ways=ways,
            bucket_population=int(rows['bucket_population']))
        for i in range(count):
            prefix=f'match{i}_';values={key:int(rows[prefix+key]) for key in
                ('address','packet','raw','head','flags','capture_id','sampled_ns','context','physical','stamp')}
            if values['address']!=address or not values['packet'] or not values['capture_id'] or values['stamp']<=0:
                raise ValueError('invalid submission index observation')
            flags=values.pop('flags');context=values.pop('context');head=values.pop('head')
            values.update(head=[head&0xffffffff,head>>32],head_available=bool(flags&1),compute=bool(flags&2),
                ambiguous_packet=bool(flags&4),register_index=context>>32,packet_header=context&0xffffffff,
                raw_address=values.pop('raw'),origin=address,submit_ns=values.pop('sampled_ns'))
            result['matches'].append(values)
    except (OSError,ValueError,KeyError) as error:
        result.update(state='invalid',matches=[],reason=str(error))
    return result


def shader_submit_review(origin, journal, index=None):
    """Join the active submission's retained sample to a paired address write."""
    result=dict(state='unavailable',
        limitation='Bounded pre-enqueue eight-byte prefixes and later CPU observations, not atomic content, whole-program stability, original writer, intended shader identity or guest allocation lifetime. Indirect scans do not model conditional execution.')
    if journal.get('state')!='readable':
        result['state']=journal.get('state','absent');return result
    lookup=origin.get('lookup');write=origin.get('address_write',origin.get('write'))
    if not lookup or not write:result['state']='write_or_lookup_absent';return result
    if not lookup.get('ns'):result['state']='lookup_time_unavailable';return result
    if write.get('actor') not in (1,2,5,6,7):result['state']='legacy_write';return result
    compute=write['actor'] in (6,7)
    candidates=[]
    for e in journal['events']:
        if e.get('event')!='sample' or e.get('phase')!='consume' or e['packet']!=write.get('packet') or e['compute']!=compute:
            continue
        register_index=e['register_index']+(0 if compute else 0x2c00)
        if not write.get('first',0)<=register_index or register_index-write['first']+2>write.get('words',0):continue
        if not e.get('ns') or not e.get('submit_ns') or not e['submit_ns']<=e['ns']<=lookup['ns']:continue
        if lookup.get('thread') and e.get('consumer_thread')!=lookup['thread']:continue
        candidates.append(e)
    if candidates:
        event=max(candidates,key=lambda e:e['seq']);result['evidence_source']='consumed_capture'
    else:
        indexed=[]
        if index and index.get('state')=='matched':
            pair=write.get('write_observation',{}).get('packet_address_words')
            raw=(pair[0]|(pair[1]<<32)) if pair is not None else None
            indexed=[e for e in index.get('matches',[]) if e['packet']==write.get('packet') and
                     (raw is None or e['raw_address']==raw) and e['compute']==compute and
                     (not e.get('submit_ns') or not write.get('ns') or e['submit_ns']<=write['ns'])]
        if len(indexed)!=1:
            result.update(state='matching_consumption_absent',submission_index_state=index.get('state') if index else 'absent',
                          submission_index_matches=len(indexed));return result
        event=indexed[0];result['evidence_source']='failure_submission_index'
    result.update(state='observed',sample=event,address_write=write,
        enqueue_to_lookup_us=(lookup['ns']-event['submit_ns'])/1000 if event.get('submit_ns') else None,
        pre_enqueue_origin_matches_lookup=event['origin']==lookup['address'])
    if event.get('ns') and event.get('submit_ns'):
        result['enqueue_to_consume_us']=(event['ns']-event['submit_ns'])/1000
    observation=write.get('write_observation',{})
    pair=observation.get('packet_address_words')
    result['pre_enqueue_packet_matches_write']=(event['raw_address']==(pair[0]|(pair[1]<<32))) if pair is not None else None
    if event['ambiguous_packet']:
        result['state']='repeated_packet_ambiguous';return result
    if not event['head_available']:
        result['state']='pre_enqueue_prefix_unavailable';return result
    if not result['pre_enqueue_origin_matches_lookup']:
        result.update(state='address_changed',observation='different_observed_origins; content_change_not_inferred')
        return result
    matches=event['head']==lookup.get('head')
    result.update(pre_enqueue_head_matches_lookup=matches,pre_enqueue_prologue_present=event['head'][0]==0xbeeb03ff,
        observation='same_sampled_prefix' if matches else 'sampled_prefix_changed')
    if matches and event['head'][0]!=0xbeeb03ff:
        result['observation']='same_prefix_without_recognized_prologue_before_enqueue_and_at_lookup'
    return result


def fault_field_transitions(events, registers):
    """Retained observations only; a numeric field is not an allocation identity."""
    values={value for value in registers.values() if value&0x00ffffffffffffff >= 1<<40}
    previous={};result=[]
    for event in sorted(events,key=lambda e:(e['ns'],e['seq'])):
        if not event['status']&2:continue
        prior=previous.get(event['field'])
        if event['value'] in values and prior and prior['value']!=event['value']:
            result.append(dict(field=event['field'],previous_different_observation=prior,
                matching_observation=event,interval_ms=(event['ns']-prior['ns'])/1e6,
                matching_registers=[name for name,value in registers.items() if value==event['value']]))
        previous[event['field']]=event
    return dict(transitions=result[-16:],omitted=max(0,len(result)-16),
        limitation='Separated protected CPU reads matching out-of-domain terminal register values. Previous different does not mean valid; the interval is not a continuous watch or exclusive writer trace. Numeric-address reuse, concurrent reads and unobserved writes remain possible.')
STOP = re.compile(r'^(STOP_EXECUTION_FAULT:|STOP_GPU_SHADER_METADATA:|STOP_GPU_METAL_COMMAND:|'
                  r'STOP_GPU_STALL:|STOP_GPU_REGISTER_RANGE:|STOP_DIAGNOSTIC_CONTROL:|STOP_AT_IMPORT:|STOP:|GPU watchdog:)')


def shader_provenance(text):
    reports=[]
    for line in text.splitlines():
        if line.startswith('Shader provenance:'):
            row={k:int(v,16 if v.startswith('0x') else 10) for k,v in
                 re.findall(r'(\w+)=(0x[0-9a-f]+|\d+)(?=[ ;]|$)',line)}
            reports.append(dict(**row,events=[]))
        elif line.startswith('Shader provenance event:') and reports:
            row={k:int(v,16 if v.startswith('0x') else 10) for k,v in
                 re.findall(r'(\w+)=(0x[0-9a-f]+|\d+)(?=[ ;]|$)',line)}
            head=re.search(r'head=([0-9a-f]+),([0-9a-f]+)',line)
            hashed=re.search(r'hash=([0-9a-f]+)',line)
            row['head']=[int(v,16) for v in head.groups()] if head else None
            row['hash']=int(hashed[1],16) if hashed else None
            write=shader_write_observation(row)
            if write:row['write_observation']=write
            if row.get('kind')==2 and row.get('actor') in (1,2):
                row['metadata_context']=dict(offset=row.get('bank'),hint=row.get('packet'),scanned=row.get('first'))
                if row['actor']==2:
                    row['metadata_association']={0:'unavailable',1:'bound_to_prologue',2:'unbound_missing_prologue',3:'unbound_invalid_hint',4:'unbound_hint_metadata'}.get(row.get('words'),'unknown')
            if row.get('program')==reports[-1].get('program'):
                reports[-1]['events'].append(row)
            else:
                reports[-1]['mismatched_event_program']=True
    return reports


def fault_register_history(text, events):
    snapshots = re.findall(r'^  rax=.*$(?:\n  r8=.*$)?', text, re.MULTILINE)
    limitation = ('Numeric matches in retained observations only, not validated pointers, '
                  'allocation identity, writer attribution or causal ordering. Unreadable '
                  'sample values are excluded; object/field addresses remain observations.')
    if len(snapshots) != 1:
        return dict(state='absent' if not snapshots else 'ambiguous', registers=[],
                    limitation=limitation)
    registers = dict((key, int(value, 16)) for key, value in re.findall(
        r'\b(r(?:ax|bx|cx|dx|sp|bp|si|di|[89]|1[0-5]))=(0x[0-9a-f]+)', snapshots[0]))
    rows = []
    for register, value in registers.items():
        if not value:
            continue
        matches = []
        for event in sorted(events, key=lambda row: row['seq']):
            kinds = []
            if event['status'] & 2 and event['value'] == value:
                kinds.append('read_value')
            if event['object'] == value: kinds.append('object_address')
            if event['status'] & 1 and event['field'] == value:
                kinds.append('field_address')
            if kinds:
                matches.append(dict(event=event, matches=kinds))
        rows.append(dict(register=register, value=value, retained_match_count=len(matches),
                         latest_matches=matches[-8:], omitted_matches=max(0, len(matches)-8)))
    return dict(state='single_snapshot', registers=rows, limitation=limitation)


def terminal_memory_sources(text, registers, fault_address):
    """Keep protected fatal-path reads separate from pre-failure probe history."""
    contexts=re.findall(r'^CPU fault context: private[^\n]*\n(.*?)^CPU fault context: end[^\n]*$',text,re.M|re.S)
    if len(contexts)!=1:
        return dict(state='ambiguous' if contexts else 'absent',candidates=[],omitted=0)
    # A truncated second context must not be silently associated with the first.
    if len(re.findall(r'^CPU fault context: private',text,re.M))!=1:
        return dict(state='ambiguous',candidates=[],omitted=0)
    candidates={}
    for line in contexts[0].splitlines():
        match=re.fullmatch(r'  CPU fault memory (\w+) address=(0x[0-9a-f]+) words=(0x[0-9a-f]+)(.*)',line)
        if not match:continue
        label,address,count,tail=match.groups();address=int(address,16);count=int(count,16)
        words=tail.split()
        if words and words[-1]=='stopped_unreadable_or_guarded':words.pop()
        if not 0<count<=16 or len(words)!=count or any(not re.fullmatch(r'0x[0-9a-f]+',word) for word in words):continue
        for index,word in enumerate(words):
            field=address+index*8;value=int(word,16)
            if field>2**64-8 or value>=2**64:continue
            matches=[name for name,number in registers.items() if number==value]
            if not value or not (value==fault_address or matches and (value&0x00ffffffffffffff)>=1<<40):continue
            key=(field,value)
            if key not in candidates:
                candidates[key]=dict(source_field=field,value=value,labels=[],matching_registers=matches,
                    fault_displacement=fault_address-value,exact_fault_address_match=value==fault_address)
            candidates[key]['labels'].append(label)
    rows=list(candidates.values())
    return dict(state='observed',candidates=rows[:16],omitted=max(0,len(rows)-16),
        limitation='Protected terminal reads only, separate from earlier probe observations. A matching stored value and signed displacement do not validate the fault operand, caller, allocation lifetime, original writer or time of the preceding load.')


def summarize(text):
    cpu = cpu_analysis(text)
    observations = [r for f in cpu['fields']
                    for r in f['readable_samples'] + f['unavailable_samples']]
    observations += cpu['invalid_address_samples']
    unavailable = sorted((r for r in observations if not r['status'] & 2),
                         key=lambda row: row['seq'])
    events = [r for r in observations if r['status'] & 4]
    shaders = shader_analysis(text.splitlines())
    exits = re.findall(r'GPU supervisor: finished elapsed=([\d.]+)s child_returncode=(-?\d+|unknown|None) exit=(\d+)', text)
    stops = [dict(line=n, message=line) for n, line in enumerate(text.splitlines(), 1) if STOP.match(line)]
    decode = []
    minima=[]
    fibers=[]
    fiber_identities=[]
    stack_protectors=[]
    decompression=[];memory_failures=[];instruction_failures=[]
    for number,line in enumerate(text.splitlines(),1):
        if line.startswith('OODLE ERROR'):
            decompression.append(dict(line=number,message=line.strip()))
        match=re.search(r'Out of Memory, Requesting ([\d.]+) (GiB|MiB|KiB|B) from (\S+) - Free: ([\d.]+) (GiB|MiB|KiB|B) / ([\d.]+) (GiB|MiB|KiB|B)',line)
        if match:
            memory_failures.append(dict(line=number,requested=float(match[1]),requested_unit=match[2],pool=match[3],
                free=float(match[4]),free_unit=match[5],total=float(match[6]),total_unit=match[7]))
        match=re.match(r'STOP: bbcpu: (.*?): (\S+) at guest (0x[0-9a-f]+) \(bytes ([0-9a-f ]+)\)',line)
        if match:
            raw=bytes.fromhex(match[4]);instruction_failures.append(dict(line=number,reason=match[1],mnemonic=match[2],
                guest_rip=int(match[3],16),bytes=raw.hex(),interrupt_vector=raw[1] if match[2]=='int' and len(raw)==2 and raw[0]==0xcd else None))
    for line in text.splitlines():
        if line.startswith('Guest stack protector failure:'):
            stack_protectors.append({key:int(value,16) for key,value in re.findall(r'(\w+)=(0x[0-9a-f]+)',line)})
        if line.startswith('Fiber stack failure:'):
            fibers.append({key:int(value,16) for key,value in re.findall(r'(\w+)=(0x[0-9a-f]+)',line)})
        if line.startswith('Fiber stack identity:'):
            identity={key:int(value,16) for key,value in re.findall(r'(\w+)=(0x[0-9a-f]+)',line)}
            name=re.search(r'\bname_hex=([0-9a-f]{64})(?:[;\s]|$)',line)
            identity['name_hex']=name[1] if name else None
            fiber_identities.append(identity)
        if line.startswith('CPU fatal minimum:'):
            minima.append({key:int(value,16) for key,value in re.findall(r'(\w+)=(0x[0-9a-f]+)',line)})
        if line.startswith('GPU shader decode: '):
            values=dict(re.findall(r'(\w+)=([^ ;]+)',line))
            decode.append(values)
    return dict(stops=stops,
        guest_data_failures=dict(decompression_errors=decompression[-32:],decompression_error_count=len(decompression),
            memory_failures=memory_failures[-16:],memory_failure_count=len(memory_failures),
            instruction_failures=instruction_failures[-16:],
            limitation='Reported guest messages in log order; sizes retain their displayed units/rounding. No compressed-input validation, decoder cause, physical host OOM or shared cause is inferred. An interrupt stop may follow a deliberate guest failure path; it is not safe to skip.'),
        fatal_reporting=dict(truncation_notices=text.count('CPU fault context: concurrent or nested fatal fault; report truncated'),
                             minimal_snapshots=minima,
                             distinct_reporter_threads=len({r['reporter'] for r in minima if r.get('reporter')}) if any(r.get('reporter') for r in minima) else None,
                             guest_context_complete='CPU fault context: end (bounded, no recovery attempted)' in text),
        shader_decode_failures=decode,
        fiber_stack_failures=fibers,
        fiber_stack_identities=fiber_identities,
        stack_protector_failures=stack_protectors,
        shader_provenance=shader_provenance(text),
        guest_fault_sites=re.findall(r'exact guest instruction .*?guest offset (0x[0-9a-f]+)',text),
        exit=dict(elapsed_s=float(exits[-1][0]), child=exits[-1][1],
                supervisor=int(exits[-1][2])) if exits else None,
        cpu=dict(dump_state=cpu['dump_state'], events=cpu['events'], summary=cpu['summary'],
                 dump_metadata=cpu['dump_metadata'], malformed=cpu['malformed_events'],
                 duplicates=cpu['duplicate_sequences'],
                 unavailable_events=len(unavailable),
                 unavailable_tail=unavailable[-16:],
                 unavailable_tail_omitted=max(0, len(unavailable)-16),
                 fault_register_history=fault_register_history(text, observations),
                 retained_span_ms=(max(r['ns'] for r in events)-min(r['ns'] for r in events))/1e6 if events else None),
        shaders={k: shaders[k] for k in ('pipeline_failure_events', 'unique_failed_shader_pairs',
                                        'pipeline_adjacent_conversion', 'srt_failure_events')},
        gpu=gpu_analysis(text), host=host_analysis(text))


def journal(path, gaps):
    if not path.exists():
        gaps.append(str(path.name)+': absent')
        return []
    rows = []
    with path.open(errors='replace') as source:
        for n, line in enumerate(source, 1):
            try:
                row = json.loads(line)
                if not isinstance(row, dict): raise ValueError('not an object')
                rows.append(row)
            except (ValueError, TypeError):
                gaps.append(f'{path.name}: malformed line {n}')
    return rows


def cpu_experiment_review(text,state):
    match=re.search(r'^GPU supervisor: .*?settings=(\{.*\})$',text,re.M)
    try:settings=json.loads(match[1]) if match else {}
    except ValueError:settings={}
    variant=settings.get('SOTC_CPU_EXPERIMENT');interval=settings.get('SOTC_CPU_INTERPRET_RANGE') or ''
    if variant is None and not interval:return None
    result=dict(state='unverified',variant=variant,requested_range=interval,
                entry_observed=False,limitation='Backend comparison only; altered timing or another stop is not a native correction or proof of a CPU defect.')
    ranges=re.findall(r'^CPU region interpreter configured: begin=(0x[0-9a-f]+|0) end=(0x[0-9a-f]+|0)$',text,re.M)
    entries=re.findall(r'^CPU region interpreter entered: offset=(0x[0-9a-f]+|0)$',text,re.M)
    valid=variant=='base' and not interval and not ranges and not entries
    if variant in ('caller','buffer','table','field578') and re.fullmatch(r'0x[0-9a-f]+:0x[0-9a-f]+',interval):
        begin,end=[int(v,16) for v in interval.split(':')]
        valid=0<=begin<end<=512*1024*1024 and end-begin<=65536 and len(ranges)==1 and [int(v,0) for v in ranges[0]]==[begin,end]
        result['entry_observed']=bool(valid and len(entries)==1 and begin<=int(entries[0],0)<end)
        if entries and not result['entry_observed']:valid=False
    if state is not None and (state/'cpu-experiment-check.json').exists():
        try:
            path=state/'cpu-experiment-check.json'
            if path.stat().st_size>16384:raise ValueError('oversized comparison validation')
            check=json.loads(path.read_text())
            result['validation']=check
            matched=isinstance(check,dict) and check.get('status')=='matched' and check.get('variant')==variant and check.get('range')==interval
            if (state/'pointer-profile-check.json').exists():
                pointer=json.loads((state/'pointer-profile-check.json').read_text())
                matched=matched and pointer.get('image_sha256')==check.get('image_sha256')
            if valid and matched:
                result['state']='validated_base' if variant=='base' else ('validated_entered' if result['entry_observed'] else 'validated_not_observed')
            else:result['state']='evidence_mismatch'
        except (OSError,ValueError,TypeError,AttributeError):result['state']='validation_unreadable'
    return result


def collect(log, state, previous=()):
    report = dict(schema=1, log=log.name, current=summarize(log.read_text(errors='replace')),
                  evidence_gaps=[], recent_runs=[],
                  limitations='Different stop sites and counts are not proof of a common cause or a fix. '
                  'CPU history is sampled, not an exhaustive writer/lifetime trace. GPU notes are CPU '
                  'recording candidates, not per-command GPU progress. Missing output is not an empty ring. '
                  'Presentations are API events, not validated pixels; native samples are not fault-time memory.')
    experiment=cpu_experiment_review(log.read_text(errors='replace'),state)
    if experiment is not None:report['cpu_experiment']=experiment
    gaps = report['evidence_gaps']
    if any(stop['message']=='STOP: fiber guest stack overflow' for stop in report['current']['stops']):
        if not report['current']['fiber_stack_failures']:
            gaps.append('fiber guard failure has no stack/RSP/marker context; stack exhaustion versus overwrite unknown')
    omitted=(report['current']['cpu']['summary'] or {}).get('budget_omitted', 0)
    if omitted:
        gaps.append(f'CPU history output budget omits {omitted} additional ring records; complete footer is not complete lifecycle history')
    for index, gpu in enumerate(report['current']['gpu']['reports']):
        gaps.extend(f'GPU report {index+1}: {gap}' for gap in gpu['evidence_gaps'])
    if not report['current']['gpu']['reports'] and any(
            stop['message'].startswith(('GPU watchdog:', 'STOP_GPU_STALL:'))
            for stop in report['current']['stops']):
        gaps.append('GPU stall has no associated submission report; exact command progress unknown')
    if report['current']['fatal_reporting']['truncation_notices']:
        gaps.append('fatal report truncated by concurrent or nested fault; complete native registers do not imply complete guest context')
    if state is not None:
        # Already committed pre-failure evidence survives signals, _Exit, full
        # stderr and unrelated stop paths. Keep distinct from a terminating dump.
        journals={kind:event_journal(state/(kind.replace('_','-')+'-events.bin'),kind)
                  for kind in ('cpu','shader','pointer','task','shader_submit','shader_bind','shader_consume','shader_mapping','shader_backing','shader_watch')}
        report['pre_failure_journals']=journals
        # A diagnostic log can contain completed synthetic preflight runs before
        # the actual game run. Associate persisted journals with the last
        # supervised child, which is the process that produced the final run.
        child_pids=re.findall(r'^GPU supervisor: child_pid=(\d+)$',log.read_text(errors='replace'),re.M)
        child_pid=child_pids[-1] if child_pids else None
        report['shader_lookup_trace']=shader_lookup_captures(state,int(child_pid) if child_pid else None)
        for case in report['shader_lookup_trace']['cases']:
            if case['state']!='readable':gaps.append(f"shader lookup capture {case['case']} {case['state']}")
            elif not case['metadata_matches_scan']:gaps.append(f"shader lookup capture {case['case']} metadata differs from saved scan")
        for kind,item in journals.items():
            if child_pid and item['state']=='readable' and item['producer_pid']!=int(child_pid):
                item.update(state='foreign_process',events=[],channels=[],retained=0)
                item.pop('terminal_context',None)
                item.pop('first_out_of_range_link',None)
                item.pop('lookup_capture',None)
                item.pop('capture_counters',None)
                gaps.append(f'{kind} journal producer PID differs from logged child; stale evidence excluded')
            if item['state']=='invalid': gaps.append(f'{kind} pre-failure journal invalid: '+item['reason'])
            if item.get('counter_mismatch'): gaps.append(f'{kind} journal reservation counters interrupted/inconsistent')
            if item.get('incomplete_or_stale'): gaps.append(f'{kind} journal excludes incomplete/stale records')
            if item.get('terminal_context_incomplete'): gaps.append(f'{kind} terminal context publication interrupted')
            if item.get('terminal_context_invalid'): gaps.append(f'{kind} terminal context has invalid kind/availability')
            if item.get('first_out_of_range_link_incomplete'): gaps.append(f'{kind} first out-of-range link publication interrupted')
            if item.get('watch_slot_replacements'): gaps.append(f'{kind} watched fields replaced in bounded table; earlier writer coverage lost')
            prefix_watches=item.get('shader_prefix_watches',{})
            if prefix_watches.get('evictions'):gaps.append('shader prefix watch hash collisions replaced entries; earlier copy coverage lost')
            if prefix_watches.get('registration_busy_drops'):gaps.append('shader prefix watch registrations dropped on contention')
            persistent=item.get('persistent_watches',{})
            if persistent.get('admission_refusals'):gaps.append('persistent pointer watch admissions refused; some observed fields were not watched')
            if persistent.get('history_contention_drops'):gaps.append('persistent/rotating pointer previous-observation history lost updates to contention')
            if persistent.get('copy_candidate_skips'):gaps.append('pointer copy candidates skipped concurrent watch updates; overlap coverage incomplete')
            if item.get('busy_drops'): gaps.append(f'{kind} journal dropped contended/delayed reservations')
            capture=item.get('lookup_capture',{})
            if capture.get('refused'):gaps.append('shader lookup capture budget exhausted; later distinct candidates not saved')
            if capture.get('write_failures'):gaps.append('shader lookup capture output failed; inspect partial cases')
            if capture.get('admitted',0)>capture.get('completed',0):gaps.append('shader lookup capture incomplete or interrupted')
            submit=item.get('capture_counters',{})
            for counter in ('snapshot_fallback_failed','allocation_failed','scan_failed','limited_captures','unreadable_heads'):
                if submit.get(counter):gaps.append('shader submission capture '+counter+'; coverage incomplete')
            if submit.get('pool_refused') and not submit.get('snapshot_fallback'):
                gaps.append('shader submission capture pool_refused without fallback snapshots; coverage incomplete')
            if any(c['overwritten'] for c in item['channels']):
                gaps.append(f'{kind} journal earlier events overwritten within independent bounded channels')
        report['shader_copy_activity']=shader_copy_activity(journals['pointer'])
        if journals['cpu']['state']=='readable':
            journals['cpu']['fault_register_history']=fault_register_history(log.read_text(errors='replace'),journals['cpu']['events'])
            terminal=journals['cpu'].get('terminal_context',{})
            if terminal.get('kind') in (2,4) and terminal.get('available'):
                report['fault_field_transitions']=fault_field_transitions(journals['cpu']['events'],terminal.get('registers',{}))
        settings_match=re.search(r'^GPU supervisor: .*?settings=(\{.*\})$',log.read_text(errors='replace'),re.M)
        try: settings=json.loads(settings_match[1]) if settings_match else {}
        except ValueError: settings={}
        profile=state/'pointer-profile-check.json'
        if profile.is_file():
            try:
                checked=json.loads(profile.read_text())
                if not isinstance(checked,dict) or checked.get('status')!='matched':raise ValueError('invalid profile check')
                report['pointer_profile_check']=checked
                index_review=index_capture_review(checked,journals['cpu'],journals['pointer'],
                    settings.get('SOTC_CPU_POINTER_TRACE') or '',settings.get('SOTC_CPU_FIELD_TRACE') or '')
                if index_review is not None:report['index_capture']=index_review
                if checked.get('selection_sha256')!=hashlib.sha256((settings.get('SOTC_CPU_POINTER_TRACE') or '').encode()).hexdigest():
                    gaps.append('saved pointer profile selection differs from child settings')
                if 'field_selection_sha256' in checked and checked['field_selection_sha256']!=hashlib.sha256((settings.get('SOTC_CPU_FIELD_TRACE') or '').encode()).hexdigest():
                    gaps.append('saved field profile selection differs from child settings')
            except (ValueError,OSError):gaps.append('pointer profile check unreadable or invalid')
        elif settings.get('SOTC_REQUIRE_POINTER_TRACE')=='1':gaps.append('required pointer profile check evidence absent')
        report['capture_readiness']=dict(
            cpu_selection_configured=bool(settings.get('SOTC_CPU_FIELD_TRACE')),
            shader_provenance_enabled=settings.get('SOTC_SHADER_PROVENANCE')=='1',
            pre_failure_journal_requested=settings.get('SOTC_DIAGNOSTIC_JOURNAL')=='1',
            cpu_journal=journals['cpu']['state'],shader_journal=journals['shader']['state'],
            shader_submit_trace_requested=settings.get('SOTC_SHADER_SUBMIT_TRACE')=='1',
            shader_submit_journal=journals['shader_submit']['state'],
            pointer_trace_configured=bool(settings.get('SOTC_CPU_POINTER_TRACE')),
            pointer_trace_required=settings.get('SOTC_REQUIRE_POINTER_TRACE')=='1',
            pointer_journal=journals['pointer']['state'],
            task_guard_requested=settings.get('SOTC_TASK_GUARD')=='1',task_journal=journals['task']['state'],
            limitation='Configuration and retained evidence only, not guaranteed coverage of each future failure or all writers.')
        if settings.get('SOTC_DIAGNOSTIC_JOURNAL')=='1':
            for kind,item in journals.items():
                expected=(settings.get('SOTC_TASK_GUARD')=='1' if kind=='task' else bool(settings.get('SOTC_CPU_FIELD_TRACE')) if kind=='cpu' else
                          bool(settings.get('SOTC_CPU_POINTER_TRACE')) if kind=='pointer' else
                          settings.get('SOTC_SHADER_COPY_TRACE')=='1' if kind in ('shader_consume','shader_mapping','shader_backing','shader_watch') else
                          settings.get('SOTC_SHADER_SUBMIT_TRACE')=='1' if kind=='shader_submit' else settings.get('SOTC_SHADER_PROVENANCE')=='1')
                if expected and item['state']=='absent': gaps.append(f'{kind} pre-failure journal requested but absent; initialization or reachability unknown')
        pointers=journals['pointer']
        tasks=journals['task']
        if tasks['state']=='readable':
            text=log.read_text(errors='replace')
            fault=re.search(r'^STOP_EXECUTION_FAULT: \w+ address=(0x[0-9a-f]+)',text,re.M)
            address=int(fault[1],16) if fault else None
            matches=[e for e in tasks['events'] if address is not None and e['address'] and e['bytes'] and e['address']<=address and address-e['address']<e['bytes']]
            report['cpu_task_guard']=dict(fault_address=address,retained_matching_events=matches[-16:],
                latest_matching_event=matches[-1] if matches else None,
                peak_retained_mapped_bytes=max((e['mapped_bytes'] for e in tasks['events']),default=0),
                unguarded_fallbacks=max((e['fallbacks'] for e in tasks['events']),default=0),
                limitation='Bounded CPU coroutine mappings only. Latest retained mapping event is a candidate state, not GPU ownership or proof that no older/reused allocation was accessed. Other threads can publish after the fault before exit. Missing matches do not exclude lifetime bugs.')
            if report['cpu_task_guard']['unguarded_fallbacks']:gaps.append('CPU task guard budget/allocation fallback left some frames unguarded')
            if any(e['event'] in (4,6,7) for e in tasks['events']):gaps.append('CPU task guard protection/unmap/queue failure recorded; coverage incomplete')
        if pointers['state']=='readable':
            pointers['register_observations']=[e for e in pointers['events'] if e['probe_kind']=='register_value'][-64:]
            pointers['target_probe_hits']=[c['attempted'] for c in pointers['channels'][:-2]]
            pointers['overlap_events']=sum(e['actor']!=1 for e in pointers['events'])
            if not any(pointers['target_probe_hits']):gaps.append('pointer target probes never observed; configured coverage was not reached')
            pointers['fault_register_history']=fault_register_history(log.read_text(errors='replace'),[
                dict(e,status=(1 if e['status']&4 else 0)|(2 if e['status']&8 else 0)) for e in pointers['events'] if e['probe_kind']!='register_value'])
            fault_addresses={e['address'] for e in report['current']['fatal_reporting']['minimal_snapshots']}
            if len(fault_addresses)==1:
                address=next(iter(fault_addresses))
                matches=[e for e in pointers['events'] if e['actor']==1 and e['probe_kind']!='register_value' and e['status']&8 and e['value']==address]
                terminal=journals['cpu'].get('terminal_context',{})
                registers=terminal.get('registers',{}) if terminal.get('kind') in (2,4) and terminal.get('available') else {}
                if not registers:
                    snapshots=re.findall(r'^  rax=.*$(?:\n  r8=.*$)?',log.read_text(errors='replace'),re.M)
                    if len(snapshots)==1:
                        observed={name:int(value,16) for name,value in re.findall(
                            r'\b(rax|rcx|rdx|rbx|rsp|rbp|rsi|rdi|r8|r9|r10|r11|r12|r13|r14|r15)=(0x[0-9a-f]+|0)\b',snapshots[0])}
                        if len(observed)==16:registers=observed
                candidates=[dict(observation=e,matching_registers=[name for name,value in registers.items() if value==e['value']],
                    fault_displacement=address-e['value']) for e in pointers['events'] if e['actor']==1 and e['probe_kind']!='register_value' and e['status']&8 and
                    (e['value']&0x00ffffffffffffff)>=1<<40 and e['value'] in registers.values()]
                sources=matches+[e['observation'] for e in candidates]
                objects={e['object'] for e in sources}
                threads={e['thread'] for e in sources}
                refs=[e for e in pointers['events'] if e['actor']==1 and e['probe_kind']!='register_value' and e['status']&8 and
                    e['value'] in objects and e['thread'] in threads and e['object'] not in objects]
                report['fault_pointer_review']=dict(fault_address=address,
                    matching_source_observations=matches[-16:],omitted_sources=max(0,len(matches)-16),
                    terminal_register_source_candidates=candidates[-16:],omitted_register_sources=max(0,len(candidates)-16),
                    reference_candidates=refs[-16:],omitted_references=max(0,len(refs)-16),
                    related_cpu_fields=[e for e in journals['cpu'].get('events',[]) if e['object'] in objects][-32:],
                    limitation='Exact stored-value/fault-address matches and separately labelled out-of-domain terminal-register candidates. Signed displacement is numeric arithmetic, not proof of the fault operand. Separate from first heuristic focus; no validated call order, type, ownership, writer or allocation lifetime. Missing matches do not exclude pointer faults.')
                report['fault_pointer_review']['terminal_memory_sources']=terminal_memory_sources(
                    log.read_text(errors='replace'),registers,address)
                if any(e['watch_policy']=='persistent' and e['watch_admitted'] is False for e in matches):
                    gaps.append('terminal fault source observed but persistent watch admission refused; writer history incomplete')
            focus=pointers.get('first_out_of_range_link')
            if focus:
                references=[e for e in pointers['events'] if e['actor']==1 and e['probe_kind']!='register_value' and e['status']&8 and e['value']==focus['object'] and e['field']!=focus['field']]
                report['object_review']=dict(suspicious=focus,
                    retained_reference_candidates=references[-16:],omitted_reference_candidates=max(0,len(references)-16),
                    related_cpu_fields=[e for e in journals['cpu'].get('events',[]) if e['object']==focus['object']][-32:],
                    limitation='Numeric references from protected reads, not validated ownership/call order, object type, live allocation or permission to reuse/free memory.')
                if focus['actor'] in (2,3,4,5):
                    start=focus['write_address'];size=focus['write_bytes'];field=focus['field']
                    offset=field-start if field>=start else None
                    overlap=bool(size and offset is not None and offset<size or size and start>field and start-field<8)
                    source=focus['write_source']+offset if focus['actor'] in (2,3) and offset is not None else None
                    prior_observation=focus.get('previous_observation',{})
                    report['object_review']['copy_observation']=dict(
                        phase=focus['actor_name'],field_overlaps_copy=overlap,
                        field_offset_in_copy=offset,source_field_candidate=source if source is not None and source<=0xffffffffffffffff else None,
                        observed_value_changed=prior_observation.get('value')!=focus['value'] if prior_observation.get('available') else None,
                        limitation='Before/after observations of an admitted numeric address only. The watch may belong to an earlier allocation; a legal bulk initialization can replace it with non-pointer data. No object identity, exclusive writer, corruption or connection to the terminal fault is established.')
            pointers['object_counts_by_channel']=[dict(channel=c['channel'],retained_objects=len({e['object'] for e in pointers['events'] if e['channel']==c['channel'] and e['probe_kind']!='register_value'})) for c in pointers['channels'][:-2]]
        checked=report.get('pointer_profile_check',{})
        if isinstance(checked.get('scenarios'),list) and checked['scenarios']:
            report['analysis_scenarios']=[]
            selections_match=(checked.get('selection_sha256')==hashlib.sha256((settings.get('SOTC_CPU_POINTER_TRACE') or '').encode()).hexdigest() and
                checked.get('field_selection_sha256')==hashlib.sha256((settings.get('SOTC_CPU_FIELD_TRACE') or '').encode()).hexdigest())
            for scenario in checked['scenarios']:
                if not isinstance(scenario,dict):continue
                targets=[]
                for kind in ('cpu','pointer'):
                    channels=journals[kind].get('channels',[])
                    indices=scenario.get(kind,[])
                    if not isinstance(indices,list):
                        gaps.append('invalid analysis scenario indices');continue
                    for channel in indices:
                        valid=selections_match and journals[kind]['state']=='readable' and isinstance(channel,int) and 0<=channel<len(channels)-(2 if kind=='pointer' else 0)
                        hits=channels[channel]['attempted'] if valid else None
                        targets.append(dict(kind=kind,channel=channel,hits=hits,state='unavailable' if hits is None else 'observed' if hits else 'unreached'))
                report['analysis_scenarios'].append(dict(name=scenario.get('name','unnamed'),targets=targets,
                    limitation='Target hits only; finite observations do not establish writers, tree type or allocation identity.'))
                if any(t['state']!='observed' for t in targets):gaps.append('analysis scenario has unavailable/unreached targets: '+str(scenario.get('name','unnamed')))
        if settings_match and not settings.get('SOTC_CPU_FIELD_TRACE'):
            report['current']['cpu']['selection_disabled']=True
        frames = []
        for name in ('frames.previous.jsonl', 'frames.jsonl'):
            path = state/'visual'/name
            if path.exists(): frames.extend(journal(path, gaps))
        presents = [r for r in frames if r.get('event') == 'present_recorded']
        native = [r for r in presents if isinstance(r.get('native_allocated_bytes'), int)]
        report['visual'] = dict(retained_presentations=len(presents),
            first_frame=presents[0].get('frame') if presents else None,
            last_frame=presents[-1].get('frame') if presents else None,
            last_t_s=presents[-1].get('t_s') if presents else None,
            last_native_bytes=native[-1]['native_allocated_bytes'] if native else None,
            last_native_sample_age_ms=native[-1].get('native_sample_age_ms') if native else None,
            peak_retained_native_bytes=max(r['native_allocated_bytes'] for r in native) if native else None)
        if not presents: gaps.append('presentation evidence absent')
        exit_record=report['current']['exit']
        last_t=presents[-1].get('t_s') if presents else None
        idle=max(0.0,exit_record['elapsed_s']-last_t) if exit_record and isinstance(last_t,(int,float)) else None
        report['progress_review']=dict(approx_no_presentation_before_exit_s=idle,
            prolonged_presentation_gap=idle>=30 if idle is not None else None,
            window_close_requested='Window: closed (quit requested)' in log.read_text(errors='replace'),
            limitation='Post-run clue, not a live watchdog or proof of a blocked GPU. Clock origins/startup may differ; pauses and minimization can explain gaps. Exit zero does not validate gameplay.')
        memory = journal(state/'memory/summary.jsonl', gaps)
        report['last_memory_snapshot'] = memory[-1] if memory else None
        audio = journal(state/'audio.jsonl', gaps)
        report['audio'] = dict(windows=len(audio),
            queue_failures=sum(r.get('queue_wait_failures', 0) for r in audio) if audio else None,
            max_submit_gap_us=max(r.get('max_submit_gap_us', 0) for r in audio) if audio else None)
        snapshot = state/'shader-metadata-failure/code.bin'
        if snapshot.is_file():
            data = snapshot.read_bytes()
            report['shader_metadata_snapshot'] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                limitation='Bounded saved backing slice; may be shorter than scanner input and may change during capture.')
            context=snapshot.parent/'context.txt'
            if context.is_file():
                meta=dict(line.split('=',1) for line in context.read_text(errors='replace').splitlines() if '=' in line)
                report['shader_metadata_snapshot']['context']=meta
                if meta.get('snapshot_from_scan')=='1':
                    report['shader_metadata_snapshot']['limitation']='Exact saved scanner input; guest reads are not atomic and the earlier head read may differ.'
            provenance=snapshot.parent/'provenance.txt'
            if provenance.is_file():
                report['shader_metadata_snapshot']['provenance']=shader_provenance(provenance.read_text(errors='replace'))
            meta=report['shader_metadata_snapshot'].get('context',{})
            if 'address' in meta:
                report['shader_metadata_snapshot']['submission_index']=shader_submission_index(
                    snapshot.parent/'submission-prefix.txt',int(meta['address']))
            descriptor=snapshot.parent/'descriptor.bin'
            if descriptor.is_file():
                raw=descriptor.read_bytes()
                item=dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest())
                meta=report['shader_metadata_snapshot'].get('context',{})
                layout=meta.get('descriptor_layout')
                address_offset={'address40_shift8':0,'compute_address40_at48_shift8':48}.get(layout)
                # Old compute contexts used the graphics label incorrectly.
                if layout=='address40_shift8' and meta.get('role','').startswith('compute'):
                    address_offset=None
                    item['address_comparison_state']='legacy_compute_layout_unavailable'
                if address_offset is not None and address_offset+8<=len(raw)<=512:
                    register=struct.unpack_from('<Q',raw,address_offset)[0];origin=(register&((1<<40)-1))<<8
                    item.update(raw_address_register=register,origin_observation=origin,
                        matches_lookup_address=origin==int(meta.get('address','-1')),
                        address_offset=address_offset,atomic=False)
                report['shader_metadata_snapshot']['descriptor']=item
            meta=report['shader_metadata_snapshot'].get('context',{})
            if 'address' in meta:
                try:
                    events=journals['shader']['events'] if journals['shader']['state']=='readable' else []
                    fallback=[event for item in report['shader_metadata_snapshot'].get('provenance',[]) for event in item['events']]
                    report['shader_origin_review']=shader_origin_with_fallback(events,fallback,int(meta['address']),
                        int(meta['program']) if 'program' in meta else None,
                        int(meta['thread_id']) if 'thread_id' in meta else None)
                    report['shader_submission_review']=shader_submit_review(report['shader_origin_review'],journals['shader_submit'],
                        report['shader_metadata_snapshot'].get('submission_index'))
                    report['shader_binding_review']=shader_bind_review(report['shader_origin_review'],journals['shader_bind'])
                    report['shader_consumption_review']=shader_consume_review(report['shader_origin_review'],journals['shader_consume'])
                    report['shader_mapping_review']=shader_mapping_review(report['shader_consumption_review'],journals['shader_mapping'])
                    report['shader_watch_summary']=shader_watch_summary(state/'shader-watch-summary.bin',int(meta['address']))
                    report['shader_watch_transition_review']=shader_watch_transitions(journals['shader_watch'],int(meta['address']))
                    report['shader_backing_review']=shader_backing_review(report['shader_origin_review'],report['shader_submission_review'],journals['shader_backing'])
                    report['shader_copy_review']=shader_copy_review(report['shader_origin_review'],report['shader_submission_review'],journals['pointer'])
                    report['shader_prefix_watch_snapshot']=shader_watch_review(state/'shader-metadata-failure/prefix-watch.txt',
                        report['shader_origin_review'],report['shader_submission_review'])
                except (ValueError,KeyError):gaps.append('shader origin review unavailable')
        snapshot=state/'shader-decode-failure/code.bin'
        if snapshot.is_file():
            data=snapshot.read_bytes()
            report['shader_decode_snapshot']=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),
                limitation='Owned decode bytes only; context identifies saved word range. Does not validate the initial source or its writer.')
            context=snapshot.parent/'context.txt'
            if context.exists():
                report['shader_decode_snapshot']['context']=dict(line.split('=',1) for line in context.read_text().splitlines() if '=' in line)
                meta=report['shader_decode_snapshot']['context']
                matches=[]
                for case in report.get('shader_lookup_trace',{}).get('cases',[]):
                    candidate=case.get('context',{})
                    if case['state']!='readable' or candidate.get('origin')!=meta.get('origin') or candidate.get('shader')!=meta.get('shader'):continue
                    try:
                        start=int(meta['saved_start_word'])*4
                        scan=(state/'shader-lookup-trace'/f"case-{case['case']}"/'code.bin').read_bytes()
                        covered=0<=start<=len(scan) and len(data)<=len(scan)-start
                        matches.append(dict(case=case['case'],decode_window_covered=covered,
                            decode_window_matches_scan=(data==scan[start:start+len(data)]) if covered else None))
                    except (OSError,ValueError,KeyError):gaps.append('shader lookup/decode comparison unavailable')
                report['shader_decode_snapshot']['lookup_candidate_comparisons']=matches
            else: gaps.append('shader decode snapshot context absent')
            provenance=snapshot.parent/'provenance.txt'
            if provenance.is_file():
                report['shader_decode_snapshot']['provenance']=shader_provenance(provenance.read_text(errors='replace'))
        elif report['current']['shader_decode_failures']:
            gaps.append('shader decode snapshot absent: borrowed input or older capture')
    if any(s['message']=='STOP: guest stack protector detected corruption' for s in report['current']['stops']):
        terminal=report.get('pre_failure_journals',{}).get('cpu',{}).get('terminal_context',{})
        if not report['current']['stack_protector_failures'] and terminal.get('kind')!=1:
            gaps.append('stack protector stop has no saved import register context; caller/canary and writer unknown')
    provenance=report['current']['shader_provenance']
    if not provenance:
        provenance=report.get('shader_metadata_snapshot',{}).get('provenance',[])
    if not provenance:
        provenance=report.get('shader_decode_snapshot',{}).get('provenance',[])
    for index,item in enumerate(provenance,1):
        prefix=f'Shader provenance {index}: '
        if item.get('omitted'):
            gaps.append(prefix+'earlier descriptor events omitted by the eight-event budget')
        if item.get('slot_evictions'):
            gaps.append(prefix+'global program slots were evicted; earlier coverage is bounded')
        if item.get('mismatched_event_program'):
            gaps.append(prefix+'interleaved program event excluded')
        if not any(e.get('kind')==1 for e in item['events']):
            gaps.append(prefix+'no address-register write retained for this descriptor')
    if report['current']['cpu']['dump_state'] != 'complete':
        gaps.append('CPU history absent or partial'+('; independent committed per-site journal available' if
            report.get('pre_failure_journals',{}).get('cpu',{}).get('retained') else ''))
    for path in previous:
        try:
            item = summarize(path.read_text(errors='replace'))
            report['recent_runs'].append(dict(log=path.name, **{k:item[k] for k in ('stops','fatal_reporting','guest_fault_sites','shader_decode_failures','exit','shaders')},
                cpu_dump_state=item['cpu']['dump_state'], retained_cpu_span_ms=item['cpu']['retained_span_ms']))
        except OSError as error:
            gaps.append(f'{path.name}: {type(error).__name__}')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--state', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to((ROOT/'work').resolve()):
        parser.error('Private diagnostic reports must stay under work/.')
    if args.state and not args.state.resolve().is_relative_to((ROOT/'work').resolve()):
        parser.error('Private state must stay under work/.')
    previous = sorted((p for p in args.log.parent.glob('graphics-game-*.log')
                       if p.name < args.log.name), reverse=True)[:8]
    report = collect(args.log, args.state, previous)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('Private combined diagnostic report: '+str(args.output))


if __name__ == '__main__':
    main()
