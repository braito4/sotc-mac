"""Combine private CPU/GPU evidence offline, keeping distinct stops and gaps explicit."""
import argparse
import hashlib
import json
from pathlib import Path
import re

from analyze_cpu_field_trace import analyze as cpu_analysis
from analyze_shader_failures import analyze as shader_analysis
from analyze_host_fault import analyze as host_analysis
from analyze_gpu_submission import analyze as gpu_analysis
from analyze_event_journal import analyze as event_journal

ROOT = Path(__file__).resolve().parents[1]


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


def collect(log, state, previous=()):
    report = dict(schema=1, log=log.name, current=summarize(log.read_text(errors='replace')),
                  evidence_gaps=[], recent_runs=[],
                  limitations='Different stop sites and counts are not proof of a common cause or a fix. '
                  'CPU history is sampled, not an exhaustive writer/lifetime trace. GPU notes are CPU '
                  'recording candidates, not per-command GPU progress. Missing output is not an empty ring. '
                  'Presentations are API events, not validated pixels; native samples are not fault-time memory.')
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
        journals={kind:event_journal(state/(kind+'-events.bin'),kind) for kind in ('cpu','shader','pointer','task')}
        report['pre_failure_journals']=journals
        child_pid=re.search(r'^GPU supervisor: child_pid=(\d+)$',log.read_text(errors='replace'),re.M)
        for kind,item in journals.items():
            if child_pid and item['state']=='readable' and item['producer_pid']!=int(child_pid[1]):
                item.update(state='foreign_process',events=[],channels=[],retained=0)
                item.pop('terminal_context',None)
                item.pop('first_out_of_range_link',None)
                gaps.append(f'{kind} journal producer PID differs from logged child; stale evidence excluded')
            if item['state']=='invalid': gaps.append(f'{kind} pre-failure journal invalid: '+item['reason'])
            if item.get('counter_mismatch'): gaps.append(f'{kind} journal reservation counters interrupted/inconsistent')
            if item.get('incomplete_or_stale'): gaps.append(f'{kind} journal excludes incomplete/stale records')
            if item.get('terminal_context_incomplete'): gaps.append(f'{kind} terminal context publication interrupted')
            if item.get('terminal_context_invalid'): gaps.append(f'{kind} terminal context has invalid kind/availability')
            if item.get('first_out_of_range_link_incomplete'): gaps.append(f'{kind} first out-of-range link publication interrupted')
            if item.get('watch_slot_replacements'): gaps.append(f'{kind} watched fields replaced in bounded table; earlier writer coverage lost')
            persistent=item.get('persistent_watches',{})
            if persistent.get('admission_refusals'):gaps.append('persistent pointer watch admissions refused; some observed fields were not watched')
            if persistent.get('history_contention_drops'):gaps.append('persistent/rotating pointer previous-observation history lost updates to contention')
            if persistent.get('copy_candidate_skips'):gaps.append('pointer copy candidates skipped concurrent watch updates; overlap coverage incomplete')
            if item.get('busy_drops'): gaps.append(f'{kind} journal dropped contended/delayed reservations')
            if any(c['overwritten'] for c in item['channels']):
                gaps.append(f'{kind} journal earlier events overwritten within independent bounded channels')
        if journals['cpu']['state']=='readable':
            journals['cpu']['fault_register_history']=fault_register_history(log.read_text(errors='replace'),journals['cpu']['events'])
            terminal=journals['cpu'].get('terminal_context',{})
            if terminal.get('kind')==2 and terminal.get('available'):
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
            pointer_trace_configured=bool(settings.get('SOTC_CPU_POINTER_TRACE')),
            pointer_trace_required=settings.get('SOTC_REQUIRE_POINTER_TRACE')=='1',
            pointer_journal=journals['pointer']['state'],
            task_guard_requested=settings.get('SOTC_TASK_GUARD')=='1',task_journal=journals['task']['state'],
            limitation='Configuration and retained evidence only, not guaranteed coverage of each future failure or all writers.')
        if settings.get('SOTC_DIAGNOSTIC_JOURNAL')=='1':
            for kind,item in journals.items():
                expected=(settings.get('SOTC_TASK_GUARD')=='1' if kind=='task' else bool(settings.get('SOTC_CPU_FIELD_TRACE')) if kind=='cpu' else
                          bool(settings.get('SOTC_CPU_POINTER_TRACE')) if kind=='pointer' else settings.get('SOTC_SHADER_PROVENANCE')=='1')
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
            pointers['target_probe_hits']=[c['attempted'] for c in pointers['channels'][:-2]]
            pointers['overlap_events']=sum(e['actor']!=1 for e in pointers['events'])
            if not any(pointers['target_probe_hits']):gaps.append('pointer target probes never observed; configured coverage was not reached')
            pointers['fault_register_history']=fault_register_history(log.read_text(errors='replace'),[
                dict(e,status=(1 if e['status']&4 else 0)|(2 if e['status']&8 else 0)) for e in pointers['events']])
            fault_addresses={e['address'] for e in report['current']['fatal_reporting']['minimal_snapshots']}
            if len(fault_addresses)==1:
                address=next(iter(fault_addresses))
                matches=[e for e in pointers['events'] if e['actor']==1 and e['status']&8 and e['value']==address]
                objects={e['object'] for e in matches}
                threads={e['thread'] for e in matches}
                refs=[e for e in pointers['events'] if e['actor']==1 and e['status']&8 and
                    e['value'] in objects and e['thread'] in threads and e['object'] not in objects]
                report['fault_pointer_review']=dict(fault_address=address,
                    matching_source_observations=matches[-16:],omitted_sources=max(0,len(matches)-16),
                    reference_candidates=refs[-16:],omitted_references=max(0,len(refs)-16),
                    related_cpu_fields=[e for e in journals['cpu'].get('events',[]) if e['object'] in objects][-32:],
                    limitation='Exact numeric stored-value/fault-address matches only, separate from the first heuristic focus. Reference candidates share an observed source thread, not validated call order, type, ownership, writer or allocation lifetime. Missing matches do not exclude pointer faults.')
                if any(e['watch_policy']=='persistent' and e['watch_admitted'] is False for e in matches):
                    gaps.append('terminal fault source observed but persistent watch admission refused; writer history incomplete')
            focus=pointers.get('first_out_of_range_link')
            if focus:
                references=[e for e in pointers['events'] if e['actor']==1 and e['status']&8 and e['value']==focus['object'] and e['field']!=focus['field']]
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
            pointers['object_counts_by_channel']=[dict(channel=c['channel'],retained_objects=len({e['object'] for e in pointers['events'] if e['channel']==c['channel']})) for c in pointers['channels'][:-2]]
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
        snapshot=state/'shader-decode-failure/code.bin'
        if snapshot.is_file():
            data=snapshot.read_bytes()
            report['shader_decode_snapshot']=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),
                limitation='Owned decode bytes only; context identifies saved word range. Does not validate the initial source or its writer.')
            context=snapshot.parent/'context.txt'
            if context.exists():
                report['shader_decode_snapshot']['context']=dict(line.split('=',1) for line in context.read_text().splitlines() if '=' in line)
            else: gaps.append('shader decode snapshot context absent')
        elif report['current']['shader_decode_failures']:
            gaps.append('shader decode snapshot absent: borrowed input or older capture')
    if any(s['message']=='STOP: guest stack protector detected corruption' for s in report['current']['stops']):
        terminal=report.get('pre_failure_journals',{}).get('cpu',{}).get('terminal_context',{})
        if not report['current']['stack_protector_failures'] and terminal.get('kind')!=1:
            gaps.append('stack protector stop has no saved import register context; caller/canary and writer unknown')
    provenance=report['current']['shader_provenance']
    if not provenance:
        provenance=report.get('shader_metadata_snapshot',{}).get('provenance',[])
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
