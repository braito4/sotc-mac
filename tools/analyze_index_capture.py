"""Label retained numeric reader/index observations from a validated private profile.

No guest offsets are compiled in. Separate callbacks are not atomic snapshots or
an exhaustive writer history. This report deliberately does not join observations
solely by equal values, numeric addresses or nearby timestamps.
"""
import hashlib

SCALAR_ROLES=('fast_cursor','fast_end','fast_buffer','fast_output',
              'read_output','read_cursor','read_buffer','before_scale',
              'after_scale','object_base','effective_address')
REGISTER_ROLES=('fast_value','index_value')


def validate(config,cpu_count,pointer_count):
    if not isinstance(config,dict) or set(config)!= {'schema','scalar','register','stride','bias','width'}:
        raise ValueError('invalid index capture annotations')
    if type(config['schema'])!=int or config['schema']!=1 or config['width']!=32:
        raise ValueError('unsupported index capture schema/width')
    for key,names,count in [('scalar',SCALAR_ROLES,cpu_count),('register',REGISTER_ROLES,pointer_count)]:
        rows=config[key]
        if not isinstance(rows,dict) or set(rows)!=set(names):raise ValueError('missing index capture roles')
        if any(type(n)!=int or not 0<=n<count for n in rows.values()) or len(set(rows.values()))!=len(names):
            raise ValueError('invalid index capture channels')
    if any(type(config[key])!=int or not 0<=config[key]<=65536 for key in ('stride','bias')) or not config['stride']:
        raise ValueError('invalid index address formula')
    return config


def review(checked,cpu,pointer,selection,fields):
    config=checked.get('index_capture')
    if config is None:return None
    result=dict(state='unverified',roles={},limitation=__doc__.split('\n\n',1)[1].strip())
    try:
        validate(config,len(fields.split(',')) if fields else 0,len(selection.split(',')) if selection else 0)
        if checked.get('status')!='matched' or any(
            checked.get(key)!=hashlib.sha256(value.encode()).hexdigest()
            for key,value in [('selection_sha256',selection),('field_selection_sha256',fields)]):
            raise ValueError('index capture profile differs from child settings')
    except (ValueError,TypeError,KeyError) as error:
        result.update(state='rejected_annotations',reason=str(error));return result
    result.update(state='validated_configuration',formula=dict(stride=config['stride'],bias=config['bias'],width=32))
    for kind,names,journal in [('scalar',SCALAR_ROLES,cpu),('register',REGISTER_ROLES,pointer)]:
        for role in names:
            channel=config[kind][role]
            channels=journal.get('channels',[])
            counter=next((c for c in channels if c.get('channel')==channel),None)
            events=[e for e in journal.get('events',[]) if e.get('channel')==channel]
            if kind=='register':events=[e for e in events if e.get('actor')==1 and e.get('probe_kind')=='register_value']
            observations=[]
            for event in events[-16:]:
                row={key:event[key] for key in ('seq','ns','site','object','field','value','status','thread','host_thread') if key in event}
                row['host_thread']=event.get('host_thread') if kind=='scalar' else event.get('thread')
                row['value_readable']=bool(event['status']&(2 if kind=='scalar' else 8))
                if role in ('read_output','fast_value','index_value') and row['value_readable']:
                    row['uint32']=event['value']&0xffffffff
                    if kind=='register':row['upper_bits_zero']=event['value']>>32==0
                observations.append(row)
            result['roles'][role]=dict(channel=channel,journal=kind,
                state='journal_unavailable' if journal.get('state')!='readable' else
                      'retained' if events else 'unreached_or_not_retained',
                counter=counter,observations=observations,omitted_from_view=max(0,len(events)-16))
    result['notes']=[
        'fast_value is captured after the four-byte input load, not by rereading its buffer.',
        'fast_output identifies the output address; its value precedes the reader store.',
        'read_output is an eight-byte protected observation; uint32 labels only its low four bytes.',
        'Missing fast observations do not prove refill: filtering, overwritten records and read faults can leave gaps.',
        'host_thread is a numeric join key within this process, not an allocation or ownership identity.',
        'Formula annotations do not establish valid index bounds or an original writer.']
    return result
