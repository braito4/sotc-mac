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

def shader_write_observation(event):
    if event.get('kind')!=1 or not event.get('required'):return None
    # The current producer stores the first eight bytes of Program. ComputeProgram
    # starts with dispatch fields; its address is later in the object. Those bytes
    # cannot be interpreted using the graphics address-at-zero contract.
    if event.get('actor') in (3,4):
        return dict(raw_program_prefix=event['hash'],raw_address_register=None,
            register_origin=None,packet_header=event['length'],packet_address_words=None,
            packet_origin=None,packet_matches_register=None,register_matches_observed_address=None,
            layout='compute_address_not_at_object_start',
            limitation='Legacy compute observation contains dispatch fields, not a validated address register. Packet/register comparisons unavailable; do not infer corruption from these bytes.')
    raw=event['hash'];mask=(1<<40)-1
    payload=(event['head'][0]|(event['head'][1]<<32)) if event['found'] else None
    register_origin=(raw&mask)<<8
    packet_origin=((payload&mask)<<8) if payload is not None else None
    return dict(raw_address_register=raw,register_origin=register_origin,
        packet_header=event['length'],packet_address_words=event['head'] if event['found'] else None,
        packet_origin=packet_origin,
        packet_matches_register=(packet_origin==register_origin) if packet_origin is not None else None,
        register_matches_observed_address=register_origin==event['address'],
        limitation='Original bounded SET_SH_REG payload and separate post-copy register observation. Partial/other-layout packets have no full pair. Neither observation is an atomic ownership or guest-code snapshot.')

def pointer_event(values):
    row=dict(zip(('site','object','source_field','header','field','value','status','write_source',
                  'write_address','write_bytes','thread'),values))
    row['watch_group']=(row['status']>>24)&3 if row['status']&(1<<26) else None
    row['actor']=(row['status']>>16)&255;row['status']&=0xffff
    row['actor_name']={1:'chain_probe',2:'memcpy_before',3:'memcpy_after',4:'memset_before',5:'memset_after',
        6:'shader_memcpy_before',7:'shader_memcpy_after',8:'shader_memset_before',9:'shader_memset_after'}.get(row['actor'],'unknown')
    row['source_kind']='fill_byte' if row['actor'] in (4,5,8,9) else 'source_pointer' if row['actor'] in (2,3,6,7) else 'none'
    row['caller_is_candidate']=bool(row['status']&256)
    row['probe_kind']='pointer_field' if row['status']&512 else 'header_chain'
    if row['status']&32768:row['probe_kind']='register_value'
    row['watch_policy']='persistent' if row['status']&1024 else 'observe_only' if row['status']&2048 else 'rotating'
    row['indexed_chain']=bool(row['status']&4096)
    row['watch_admitted']=bool(row['status']&8192) if row['status']&16384 else None
    if row['actor']==1 and row['indexed_chain']:
        row['index']=dict(register=row['write_bytes'],value=row['write_source'],scale=row['write_address'])
    if row['actor']==1 and row['status']&512:row['actor_name']='pointer_field_probe'
    if row['actor']==1 and row['status']&32768:row['actor_name']='register_value_probe'
    if row['actor'] in (6,7,8,9):
        row['probe_kind']='shader_prefix'
        row['registration_context']=dict(packet=row['source_field'],capture_id=row['header'])
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
        if kind not in ('shader_watch','cpu','shader','shader_submit','shader_bind','shader_consume','shader_mapping','shader_backing','pointer','task') or kind in ('shader','shader_submit','shader_bind','shader_consume','shader_mapping','shader_backing','task') and channels!=2 or kind=='pointer' and not 3<=channels<=28:
            raise ValueError('invalid journal kind/channels')
        if kind=='shader_watch' and (channels!=24 or header[9]!=1 or header[15]!=8):
            raise ValueError('unsupported shader watch lifecycle layout')
        total,drops,suppressed=header[5:8]
        if kind=='shader_bind' and (header[9]!=1 or header[15]!=8):
            raise ValueError('unsupported shader binding prefix layout')
        if kind=='shader_consume':
            if header[9]!=1 or header[15]!=8:raise ValueError('unsupported fresh consumption prefix layout')
            report['observation_counters']=dict(zip(('observed','changed','unavailable'),header[256:259]))
        if kind=='shader_mapping' and (header[9]!=1 or header[15]!=8):
            raise ValueError('unsupported shader mapping observation layout')
        if kind=='shader_backing':
            if header[9] not in (1,2,3,4,5,6,7,8,9,10,11,12,13) or header[15]!=8:raise ValueError('unsupported backing-write observation layout')
            report['observation_counters']=dict(before=header[256],after=header[257])
            if header[9]>=2:report['observation_counters']['write_notifications']=header[258]
            if header[9]>=3:report['observation_counters']['gpu_write_intents']=header[259]
            if header[9]>=4:report['eligible_call_counters']=dict(before=header[260],after=header[261],
                write_notifications=header[262],gpu_by_source=dict(zip(('eop','eos','eos_gds','write_data',
                    'compute_write_data','release_mem','const_ram','dma_fill','dma_copy','buffer_download','image_download'),header[263:274])))
            if header[9]>=5:
                report['observation_counters']['writable_buffer_bindings']=header[275]
                report['eligible_call_counters']['writable_buffer_bindings']=header[274]
            if header[9]>=6:
                report['observation_counters']['writable_buffer_bindings']=header[275]+header[277]
                report['eligible_call_counters']['writable_buffer_bindings']=header[274]+header[276]
                report['observation_counters']['writable_buffer_bindings_by_route']=dict(classic=header[275],paged=header[277])
                report['eligible_call_counters']['writable_buffer_bindings_by_route']=dict(classic=header[274],paged=header[276])
            if header[9]>=8:
                report['observation_counters']['writable_buffer_bindings']+=header[286]
                report['eligible_call_counters']['writable_buffer_bindings']+=header[285]
                report['observation_counters']['writable_buffer_bindings_by_route']['storage_image']=header[286]
                report['eligible_call_counters']['writable_buffer_bindings_by_route']['storage_image']=header[285]
            if header[9]>=9:
                report['observation_counters']['writable_buffer_bindings']+=header[288]
                report['eligible_call_counters']['writable_buffer_bindings']+=header[287]
                report['observation_counters']['writable_buffer_bindings_by_route']['render_target']=header[288]
                report['eligible_call_counters']['writable_buffer_bindings_by_route']['render_target']=header[287]
            if header[9]>=10:
                report['observation_counters']['writable_buffer_bindings']+=header[290]
                report['eligible_call_counters']['writable_buffer_bindings']+=header[289]
                report['observation_counters']['writable_buffer_bindings_by_route']['storage_image_resolved']=header[290]
                report['eligible_call_counters']['writable_buffer_bindings_by_route']['storage_image_resolved']=header[289]
            if header[9]>=11:
                report['observation_counters']['cpu_direct_stores']=dict(scalar=header[291],vector=header[292])
            if header[9]>=12:
                report['eligible_call_counters']['cpu_direct_stores']=dict(scalar=header[293],vector=header[294])
            if header[9]>=13:
                report['eligible_call_counters']['cpu_direct_store_candidates']=dict(
                    shader_page= dict(scalar=header[295],vector=header[296]),
                    shader_bucket=dict(scalar=header[297],vector=header[298]))
            if header[9]>=7:
                if header[278]!=4096:raise ValueError('invalid prior writable-binding history layout')
                report['prior_binding_history']=dict(capacity=header[278],published=header[279],
                    busy_drops=header[280],replacements=header[281],prefix_scans=header[282],
                    unstable_records=header[283],retrospective_matches=header[284],
                    limitation='Fixed recent numeric binding history. Replacement, contention and finite time coverage leave explicit gaps; a descriptor binding is write intent, not proof that a GPU store occurred.')
        if kind=='shader_submit':
            if header[9] not in (1,2,3,4) or header[10:12]!=(16,512) or \
               header[12:16]!=((32768,4,262144,8) if header[9]>=4 else (4096,4,262144,8)):
                raise ValueError('unsupported shader submission capture layout')
            report['capture_limits']=dict(live_captures=header[10],samples_per_capture=header[11],
                packets_per_capture=header[12],indirect_depth=header[13],words_per_capture=header[14],prefix_bytes=header[15])
            if header[9]>=3:
                report['capture_limits'].update(head_samples=64,tail_samples=448)
            report['capture_counters']=dict(zip(('attempted','admitted','pool_refused','allocation_failed',
                'scan_failed','limited_captures','unreadable_heads'),header[256:263]))
            if header[9]>=2:
                report['capture_counters'].update(snapshot_fallback=header[263],snapshot_fallback_failed=header[264])
            if header[9]>=3:
                report['capture_counters']['tail_replacements']=header[265]
        if kind=='shader' and header[9]==1:
            if header[15]!=8:raise ValueError('invalid shader capture budget')
            report['lookup_capture']=dict(capacity=header[15],admitted=header[10],duplicates=header[11],
                refused=header[12],write_failures=header[13],completed=header[14])
        if kind=='task':
            report.update(mapped_budget=header[9],host_page_bytes=header[10],tracking_slots=header[11],quarantine_frames=header[12])
        if kind=='pointer':
            report.update(watches_armed=header[9],watch_busy_drops=header[10],watch_slot_replacements=header[11])
            if header[187] not in (0,1,2):raise ValueError('unsupported shader prefix watch version')
            if header[187] in (1,2):
                if (header[187]==1 and header[188]!=256) or (header[187]==2 and (header[188]!=2048 or header[245]!=4)):
                    raise ValueError('invalid shader prefix watch layout')
                report['shader_prefix_watches']=dict(layout=header[187],capacity=header[188],ways=header[245] if header[187]==2 else 1,registrations=header[189],
                    evictions=header[190],registration_busy_drops=header[191],
                    cumulative_copy_events=dict(zip(('6','7','8','9'),header[240:244])) if header[244]==1 else None,
                    limitation='Bounded rotating numeric prefix watches; capacity/collisions and busy updates lose coverage. Imported memcpy/memset only; no allocation identity, direct stores, aliases or GPU writes.')
            if header[183]==1:
                report['admission_refusals_by_probe']=[header[192+c] for c in range(channels-2)]
            if header[186]==1:
                if header[184]!=4 or header[185]!=2048:raise ValueError('invalid watch bank layout')
                report['watch_groups']=[dict(group=g,capacity=header[185],admitted=header[224+g*2],
                    admission_refusals=header[225+g*2]) for g in range(4)]
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
            if values[0] not in (1,2,3,4) or values[1] not in (0,1):
                report['terminal_context_invalid']=True
            else:
                report['terminal_context']=dict(zip(('kind','available','guest_rip','fs_base','stack_top','thread'),values[:6]))
                report['terminal_context']['registers']=dict(zip(
                    ('rax','rcx','rdx','rbx','rsp','rbp','rsi','rdi','r8','r9','r10','r11','r12','r13','r14','r15'),values[6:]))
                report['terminal_context']['source']={1:'raw_stack_protector_import',2:'translated_fault',3:'instruction_failure',4:'interpreted_fault'}[values[0]]
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
                    if values[6]:event['host_thread']=values[6]
                elif kind=='pointer':
                    event=pointer_event(values)
                    if event['actor'] not in range(1,10):rejected+=1;continue
                    if event['actor'] in (6,7,8,9) and header[187] not in (1,2):rejected+=1;continue
                elif kind=='shader_watch':
                    action,address,packet,capture,generation,prior_address,prior_packet,prior_capture,prior_generation,thread,extent=values
                    key=address>>8;key^=key>>17
                    if action not in (1,2,3,4) or not address or not capture or extent!=8 or channel!=(key&7)+(16 if action==3 else 8 if action==4 else 0):
                        rejected+=1;continue
                    event=dict(action={1:'arm',2:'refresh',3:'evict',4:'registration_dropped'}[action],
                        address=address,packet=packet,capture_id=capture,watch_generation=generation,
                        related_address=prior_address,related_packet=prior_packet,related_capture_id=prior_capture,
                        related_generation=prior_generation,thread=thread,watched_bytes=extent)
                elif kind=='shader_backing':
                    field,packet,capture,head,status,address,length,source,thread,site,caller=values
                    notification=bool(status&8);gpu_intent=bool(status&16);binding=bool(status&32)
                    cpu_route=status>>32
                    binding_route=((status>>6)&(7 if header[9]>=10 else 3)) if binding and header[9]>=8 else int(bool(status&64))
                    paged_binding=binding_route==1;image_binding=binding_route==2;target_binding=binding_route==3
                    resolved_image_binding=binding_route==4
                    historical=bool(status&(32768 if header[9]>=8 else 128));source_id=(status>>8)-1
                    if cpu_route:
                        if header[9]<11 or cpu_route not in (1,2) or status!=((cpu_route<<32)|int(bool(status&1))) or \
                           not capture or not length or length>16 or channel:
                            rejected+=1;continue
                        event=dict(field=field,packet=packet,capture_id=capture,head=[head&0xffffffff,head>>32],
                            head_available=bool(status&1),write_address=address,write_bytes=length,
                            value_low=source,value_high=thread,guest_instruction=site,
                            cpu_store_route='scalar' if cpu_route==1 else 'vector',
                            thread=caller,phase='cpu_direct_store',write_succeeded=True)
                    elif historical and not binding:
                        if header[9]<7 or status!=(32768 if header[9]>=8 else 128) or channel or not capture or not length:
                            rejected+=1;continue
                        event=dict(field=field,packet=packet,capture_id=capture,head=[0,0],
                            head_available=False,write_succeeded=None,history_oldest_ns=address,
                            history_newest_ns=length if length!=1 else 0,history_matches=source,
                            thread=thread,history_stable_records=site,history_total_attempted=caller,
                            phase='prior_binding_scan')
                    elif status&~0xffff or (historical and (header[9]<7 or not binding or status&1)) or \
                       (paged_binding and (not binding or header[9]<6)) or (image_binding and (not binding or header[9]<8)) or \
                       (target_binding and (not binding or header[9]<9)) or \
                       (resolved_image_binding and (not binding or header[9]<10)) or \
                       (binding and (header[9]<5 or binding_route>(4 if header[9]>=10 else 3) or status&~(1|32|64|128|(256 if header[9]>=10 else 0)|(32768 if header[9]>=8 else 0)) or channel!=int(bool(status&1) and head&0xffffffff!=0xbeeb03ff))) or \
                       (gpu_intent and (header[9]<3 or channel!=0 or status&46 or not 0<=source_id<=10)) or \
                       (not gpu_intent and not binding and status>>8) or (notification and (header[9]<2 or channel!=0 or status&22)) or \
                       (not notification and not gpu_intent and not binding and status&6 not in ((0,) if channel==0 else (2,4))) or not capture or not length:
                        rejected+=1;continue
                    else:
                        event=dict(field=field,packet=packet,capture_id=capture,head=[head&0xffffffff,head>>32],
                            head_available=bool(status&1),write_succeeded=bool(status&2) if channel and not binding else None,
                            write_address=address,write_bytes=length,source=source,thread=thread,
                            guest_site_candidate=site,host_caller_candidate=caller,
                            phase='prior_writable_buffer_binding' if historical else 'writable_buffer_binding' if binding else 'gpu_write_intent' if gpu_intent else 'write_notification' if notification else 'after' if channel else 'before')
                        if gpu_intent:
                            event['fence_sequence']=event.pop('guest_site_candidate')
                            event['gpu_write_source']=('eop','eos','eos_gds','write_data','compute_write_data','release_mem',
                                'const_ram','dma_fill','dma_copy','buffer_download','image_download')[source_id]
                        if binding:
                            event['shader']=event.pop('source');event['descriptor_slot']=event.pop('guest_site_candidate')
                            event['binding_address']=event.pop('write_address');event['binding_bytes']=event.pop('write_bytes')
                            event['declared_bytes']=event.pop('host_caller_candidate')
                            event['binding_route']=('classic','paged','storage_image','render_target','storage_image_resolved')[binding_route]
                elif kind=='shader_mapping':
                    capture,packet,address,before,after,sampled_ns,consume_ns,thread,old_head,new_head,status=values
                    if not capture or not packet or not 0<sampled_ns<=consume_ns==ns or status>3 or \
                       bool(status&1)!=(before!=(1<<64)-1) or bool(status&2)!=(after!=(1<<64)-1) or \
                       channel!=(0 if status==3 and before==after else 1):rejected+=1;continue
                    event=dict(capture_id=capture,packet=packet,origin=address,physical_before=before,
                        physical_at_consume=after,physical_before_available=bool(status&1),
                        physical_at_consume_available=bool(status&2),submit_ns=sampled_ns,
                        consume_ns=consume_ns,thread=thread,pre_enqueue_head=[old_head&0xffffffff,old_head>>32],
                        head=[new_head&0xffffffff,new_head>>32])
                elif kind=='shader_consume':
                    capture,packet,address,before,head,flags,thread,context,watch_packet,watch_capture,stamp=values
                    available=bool(flags&(1<<28));prologue=bool(flags&(1<<29));changed=bool(flags&(1<<30))
                    scan_state=((flags>>25)&7)-1;consume_state=((flags>>32)&7)-1
                    index=context>>32
                    if not capture or not packet or index not in (8,0x48,0x88,0xc8,0x108,0x148,0x20c) or \
                       context&0xc000ff00!=0xc0007600 or scan_state>5 or consume_state>5 or \
                       prologue!=(available and head&0xffffffff==0xbeeb03ff) or \
                       changed!=(bool(flags&1) and available and before!=head) or \
                       channel!=(1 if changed or not prologue else 0):rejected+=1;continue
                    event=dict(capture_id=capture,packet=packet,origin=address,thread=thread,
                        pre_enqueue_head=[before&0xffffffff,before>>32],pre_enqueue_available=bool(flags&1),
                        head=[head&0xffffffff,head>>32],head_available=available,prologue_present=prologue,
                        sampled_prefix_changed=changed,registration_eligible=bool(flags&(1<<24)),
                        watch_state_after_scan=scan_state,watch_state_at_consume=consume_state,
                        watch_packet=watch_packet,watch_capture_id=watch_capture,watch_stamp=stamp,
                        register_index=index,compute=bool(flags&2),ambiguous_packet=bool(flags&4))
                elif kind=='shader_bind':
                    event=dict(zip(('program','address','packet','raw_address_register','prefix','status','thread','actor','first','words','bank'),values))
                    if event['status']>3 or event['status']==2 or channel!=(0 if event['status']&2 else 1):rejected+=1;continue
                    event['head_available']=bool(event['status']&1)
                    event['prologue_present']=bool(event['status']&2)
                    event['head']=[event.pop('prefix')&0xffffffff,values[4]>>32]
                    if event['prologue_present']!=(event['head_available'] and event['head'][0]==0xbeeb03ff):rejected+=1;continue
                elif kind=='task':
                    event=dict(zip(('address','bytes','requested','generation','event','live_bytes','quarantined_bytes','mapped_bytes','fallbacks','reserved','thread'),values))
                    event['event_name']={1:'allocated',2:'quarantined',3:'unmapped',4:'protection_failed',5:'unguarded_fallback',6:'unmap_failed',7:'quarantine_queue_stalled'}.get(event['event'],'unknown')
                    if event['event'] not in range(1,8) or channel!=(0 if event['event'] in (1,5) else 1):rejected+=1;continue
                elif kind=='shader_submit':
                    capture,packet,raw,origin,head,flags,submit_ns,submitter,root,consumer,context=values
                    if not capture:rejected+=1;continue
                    if flags==1<<63:
                        if channel or raw>512 or origin>4096 or head>262144:rejected+=1;continue
                        reasons={1:'packet_budget',2:'word_budget',4:'sample_budget',8:'indirect_depth',
                            16:'unavailable_read',32:'incomplete_packet',64:'other_layout',128:'partial_address_pair',256:'repeated_packet'}
                        event=dict(event='summary',capture_id=capture,root=root,root_words=context,
                            samples=raw,packets=origin,words=head,started_ns=submit_ns,submitter_thread=submitter,
                            gap_mask=consumer,gaps=[name for bit,name in reasons.items() if consumer&bit])
                    else:
                        index=context>>32;packet_header=context&0xffffffff
                        if not packet or index not in (8,0x48,0x88,0xc8,0x108,0x148,0x20c) or \
                           bool(flags&2)!=(index==0x20c) or ((raw&((1<<40)-1))<<8)!=origin or \
                           packet_header>>30!=3 or (packet_header>>8)&255!=0x76 or (flags>>8)&255>4:
                            rejected+=1;continue
                        event=dict(event='sample',capture_id=capture,packet=packet,raw_address=raw,origin=origin,
                            head=[head&0xffffffff,head>>32],head_available=bool(flags&1),
                            compute=bool(flags&2),ambiguous_packet=bool(flags&4),depth=(flags>>8)&255,
                            queue=(flags>>16)&255,submit_ns=submit_ns,submitter_thread=submitter,
                            root=root,consumer_thread=consumer,register_index=index,packet_header=packet_header,
                            phase='submit' if channel==0 else 'consume')
                else:
                    event=dict(zip(('program','address','bank','packet','first','words','hash'),values[:7]))
                    event.update(kind=values[7]&0xffffffff,actor=values[7]>>32,
                                 head=[values[8]&0xffffffff,values[8]>>32],
                                 length=values[9]&0xffffffff,found=(values[9]>>32)&1,
                                 required=(values[9]>>33)&1,thread=values[10])
                    write=shader_write_observation(event)
                    if write:event['write_observation']=write
                    if event['kind']==2 and event['actor'] in (1,2):
                        event['metadata_context']=dict(offset=event['bank'],hint=event['packet'],scanned=event['first'])
                        if event['actor']==2:
                            event['metadata_association']={0:'unavailable',1:'bound_to_prologue',2:'unbound_missing_prologue',3:'unbound_invalid_hint',4:'unbound_hint_metadata'}.get(event['words'],'unknown')
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
