"""Read bounded pre-failure CPU/shader journals offline, without guest access.

Designed for collection after child termination. Commit guards reject interrupted
or stale slots. Not a live atomic snapshot, exhaustive writers, allocation
identity, verified ownership or GPU execution. MAP_SHARED survives process exit;
no power-loss/kernel-failure durability is claimed.
"""
from pathlib import Path
import struct

HEADER_WORDS=512
RECORD_WORDS=16
LIMITATION=__doc__.split('\n\n',1)[1].strip()

def pointer_event(values):
    row=dict(zip(('site','object','source_field','header','field','value','status','write_source',
                  'write_address','write_bytes','thread'),values))
    row['actor']=row['status']>>16;row['status']&=0xffff
    row['actor_name']={1:'chain_probe',2:'memcpy_before',3:'memcpy_after',4:'memset_before',5:'memset_after'}.get(row['actor'],'unknown')
    row['source_kind']='fill_byte' if row['actor'] in (4,5) else 'source_pointer' if row['actor'] in (2,3) else 'none'
    row['caller_is_candidate']=bool(row['status']&256)
    row['probe_kind']='pointer_field' if row['status']&512 else 'header_chain'
    row['watch_policy']='persistent' if row['status']&1024 else 'observe_only' if row['status']&2048 else 'rotating'
    row['indexed_chain']=bool(row['status']&4096)
    row['watch_admitted']=bool(row['status']&8192) if row['status']&16384 else None
    if row['actor']==1 and row['indexed_chain']:
        row['index']=dict(register=row['write_bytes'],value=row['write_source'],scale=row['write_address'])
    if row['actor']==1 and row['status']&512:row['actor_name']='pointer_field_probe'
    return row


def analyze(path, kind):
    path=Path(path)
    report=dict(file=path.name,state='absent',events=[],channels=[],limitation=LIMITATION)
    if not path.is_file(): return report
    try:
        size=path.stat().st_size
        if not 4096<=size<=4096+28*512*128: raise ValueError('invalid size')
        data=path.read_bytes()
        if len(data)!=size or data[:8]!=b'SOTCEV01': raise ValueError('invalid header')
        header=struct.unpack_from('<512Q',data)
        version,channels,capacity,words=header[1:5]
        if version!=1 or not 1<=channels<=28 or not 1<=capacity<=512 or words!=16:
            raise ValueError('unsupported layout')
        if size!=4096+channels*capacity*128: raise ValueError('truncated or excess records')
        if kind not in ('cpu','shader','pointer','task') or kind in ('shader','task') and channels!=2 or kind=='pointer' and not 3<=channels<=14:
            raise ValueError('invalid journal kind/channels')
        total,drops,suppressed=header[5:8]
        if kind=='task':
            report.update(mapped_budget=header[9],host_page_bytes=header[10],tracking_slots=header[11],quarantine_frames=header[12])
        if kind=='pointer':
            report.update(watches_armed=header[9],watch_busy_drops=header[10],watch_slot_replacements=header[11])
            if header[183]==1:
                report['admission_refusals_by_probe']=[header[192+c] for c in range(channels-2)]
            report['focus_criterion']='First readable link whose low 56 bits exceed the 40-bit guest domain; top-byte tag ignored for selection only. Heuristic, not actual access failure, object type or lifetime validation.'
            if header[160]==1:
                report['first_out_of_range_link']=dict(pointer_event(header[162:173]),ns=header[161])
                if header[179]==1:
                    report['first_out_of_range_link']['previous_observation']=dict(available=bool(header[173]),value=header[174],ns=header[175],
                        limitation='Previous completed observation of this numeric field, not ownership, allocation identity, atomic change or exclusive writer evidence.')
            elif header[160]:report['first_out_of_range_link_incomplete']=True
            report['writer_coverage']='Imported memcpy/memset only, overlapping currently armed fields; direct scalar/vector guest stores, GPU writes, aliases and earlier writes are not intercepted. No allocation identity or exclusive writer proof.'
            if header[179]==1:
                report['persistent_watches']=dict(capacity=header[180],storage_bytes=header[181],admitted=header[176],
                    admission_refusals=header[177],history_contention_drops=header[178],copy_candidate_skips=header[182],
                    limitation='Admitted numeric fields are never evicted. Capacity/hash conflicts, concurrent admission/history/copy reads and pre-admission writes can leave gaps; no allocation generation is tracked.')
        terminal=header[128]
        if terminal==1:
            values=header[129:151]
            if values[0] not in (1,2) or values[1] not in (0,1):
                report['terminal_context_invalid']=True
            else:
                report['terminal_context']=dict(zip(('kind','available','guest_rip','fs_base','stack_top','thread'),values[:6]))
                report['terminal_context']['registers']=dict(zip(
                    ('rax','rcx','rdx','rbx','rsp','rbp','rsi','rdi','r8','r9','r10','r11','r12','r13','r14','r15'),values[6:]))
                report['terminal_context']['source']='raw_stack_protector_import' if values[0]==1 else 'translated_fault'
                report['terminal_context']['limitation']='First committed numeric snapshot; no validated caller, canary location, allocation lifetime or writer.'
        elif terminal:
            report['terminal_context_incomplete']=True
        if sum(header[16+4*c] for c in range(channels))!=total:
            # Initialization or an interrupted reservation, not a trustworthy
            # complete footer. Continue with individually committed records.
            report['counter_mismatch']=True
        invalid=0
        for channel in range(channels):
            attempted,busy_drops=header[16+4*channel:18+4*channel]
            kept=[];rejected=0
            start=max(0,attempted-capacity)
            for ticket in range(start,attempted):
                offset=4096+(channel*capacity+ticket%capacity)*128
                row=struct.unpack_from('<16Q',data,offset)
                commit,busy,observed,seq,ns=row[:5]
                if not commit or busy or commit!=seq or observed!=ticket+1 or seq>total:
                    rejected+=1;continue
                values=row[5:]
                if kind=='cpu':
                    event=dict(zip(('site','object','field','value','status','thread'),values[:6]))
                elif kind=='pointer':
                    event=pointer_event(values)
                    if event['actor'] not in (1,2,3,4,5):rejected+=1;continue
                elif kind=='task':
                    event=dict(zip(('address','bytes','requested','generation','event','live_bytes','quarantined_bytes','mapped_bytes','fallbacks','reserved','thread'),values))
                    event['event_name']={1:'allocated',2:'quarantined',3:'unmapped',4:'protection_failed',5:'unguarded_fallback',6:'unmap_failed',7:'quarantine_queue_stalled'}.get(event['event'],'unknown')
                    if event['event'] not in range(1,8) or channel!=(0 if event['event'] in (1,5) else 1):rejected+=1;continue
                else:
                    event=dict(zip(('program','address','bank','packet','first','words','hash'),values[:7]))
                    event.update(kind=values[7]&0xffffffff,actor=values[7]>>32,
                                 head=[values[8]&0xffffffff,values[8]>>32],
                                 length=values[9]&0xffffffff,found=(values[9]>>32)&1,
                                 required=(values[9]>>33)&1,thread=values[10])
                    if event['kind']!=channel+1:
                        rejected+=1;continue
                event.update(channel=channel,seq=seq,ticket=ticket+1,ns=ns)
                kept.append(event)
            invalid+=rejected
            report['channels'].append(dict(channel=channel,attempted=attempted,retained=len(kept),
                overwritten=max(0,attempted-capacity),busy_drops=busy_drops,incomplete_or_stale=rejected))
            report['events'].extend(kept)
        report['events'].sort(key=lambda e:e['seq'])
        times=[e['ns'] for e in report['events'] if e['ns']]
        report.update(state='readable',bytes=size,producer_pid=header[8],reservations=total,busy_drops=drops,
                      suppressed=suppressed,incomplete_or_stale=invalid,
                      retained=len(report['events']),
                      retained_span_ms=(max(times)-min(times))/1e6 if times else None)
    except (OSError,ValueError,struct.error) as error:
        report.update(state='invalid',reason=str(error),events=[],channels=[])
    return report
