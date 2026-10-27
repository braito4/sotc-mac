"""Read failure-time numeric prefix-watch state, without guest access."""
from pathlib import Path

FIELDS=('watched_address','packet','capture_id','registered_ns','last_readable_available',
        'last_readable_value','last_readable_ns','stamp')


def review(path,origin,submission):
    path=Path(path)
    result=dict(state='absent',limitation='Failure-time numeric watch state only. Not proof of continuous coverage, allocation identity or original writer. No match can mean never registered or replaced.')
    if not path.is_file():return result
    try:
        if path.stat().st_size>4096:raise ValueError('oversized watch snapshot')
        rows=dict(line.split('=',1) for line in path.read_text().splitlines() if '=' in line)
        if rows.get('schema')!='1':raise ValueError('unknown watch snapshot schema')
        address=int(rows['address']);status=int(rows['state']);values={key:int(rows[key]) for key in FIELDS}
        if status not in range(-1,6) or any(not 0<=v<(1<<64) for v in values.values()):raise ValueError('invalid watch snapshot fields')
        lookup=origin.get('lookup',{});write=origin.get('address_write',{})
        if address!=lookup.get('address'):raise ValueError('watch query differs from failed lookup')
        if values['last_readable_available'] not in (0,1):raise ValueError('invalid watch sample availability')
        if status==5 and values['watched_address']!=address:raise ValueError('matched watch address differs')
        result.update(state={-1:'bridge_unavailable',0:'disabled_or_zero_origin',1:'empty_bucket',2:'address_not_watched',3:'busy',4:'changed_during_snapshot',5:'matched'}[status],
            address=address,numeric=values)
        if status==5:
            result['packet_matches_lookup_write']=values['packet']==write.get('packet') if write else None
            sample=submission.get('sample')
            result['capture_matches_pre_enqueue']=values['capture_id']==sample['capture_id'] if sample else None
            result['registered_before_lookup']=(bool(values['registered_ns'] and values['registered_ns']<=lookup['ns'])
                                                if lookup.get('ns') else None)
            if values['last_readable_available']:
                result['latest_head']=[values['last_readable_value']&0xffffffff,values['last_readable_value']>>32]
                result['latest_observation_before_lookup']=(values['last_readable_ns']<=lookup['ns'] if lookup.get('ns') else None)
    except (OSError,ValueError,KeyError) as error:result.update(state='invalid',reason=str(error))
    return result


def transition_review(journal,address):
    """Offline registration history only; never certify allocation continuity."""
    result=dict(state=journal.get('state','absent'),address=address,events=[],
        continuity='unknown',allocation_identity='unknown',
        limitation='Bounded numeric registration transitions, not memory writes or allocation lifetime. Missing events do not prove continuous coverage; times precede publication while the watch slot is busy.')
    if journal.get('state')!='readable':return result
    key=address>>8;key^=key>>17
    channels=[c for c in journal.get('channels',[]) if c['channel'] in (key&7,(key&7)+8,(key&7)+16)]
    result['relevant_channel_losses']=sum(c.get('overwritten',0)+c.get('busy_drops',0)+c.get('incomplete_or_stale',0) for c in channels)
    result['events']=[e for e in journal.get('events',[]) if e['address']==address]
    result['state']='observed' if result['events'] else 'not_retained'
    result['known_evictions']=sum(e['action']=='evict' for e in result['events'])
    result['registration_drops']=sum(e['action']=='registration_dropped' for e in result['events'])
    if result['known_evictions'] or result['registration_drops']:result['continuity']='known_gap_or_registration_failure'
    elif result['relevant_channel_losses']:result['continuity']='history_incomplete'
    return result
