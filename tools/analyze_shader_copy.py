"""Bounded imported-copy candidates for one pre-enqueue shader observation."""


def activity(journal):
    if journal.get('state')!='readable':return dict(state=journal.get('state','absent'))
    counters=journal.get('shader_prefix_watches')
    if not counters:return dict(state='watch_unavailable')
    events=[e for e in journal['events'] if e.get('actor') in (6,7,8,9)]
    return dict(state='enabled',watch_counters=counters,retained_candidates=len(events),
        actors={str(a):sum(e['actor']==a for e in events) for a in (6,7,8,9)},tail=events[-8:],
        limitation='Retained overlaps only, not exhaustive writes or an association with the terminal failure. Shared finite copy channels may omit earlier events.')


def review(origin,submission,journal):
    result=dict(state='unavailable',limitation=
        'Overlapping imported memcpy/memset observations only, not exhaustive stores, allocation identity, original writer or causal before/after pairing. A copy can race other writers; absent candidates do not exclude corruption.')
    sample=submission.get('sample');lookup=origin.get('lookup')
    if journal.get('state')!='readable':result['state']=journal.get('state','absent');return result
    result['watch_counters']=journal.get('shader_prefix_watches')
    if not result['watch_counters']:
        result['state']='watch_unavailable';return result
    if not result['watch_counters'].get('registrations'):
        result['state']='no_registered_prefixes';return result
    if not sample or not lookup or submission.get('state')!='observed':
        result['state']='pre_enqueue_capture_unavailable';return result
    candidates=[]
    for event in journal['events']:
        context=event.get('registration_context',{})
        if event.get('actor') not in (6,7,8,9) or event.get('field')!=lookup['address'] or context.get('packet')!=sample['packet'] or context.get('capture_id')!=sample['capture_id']:continue
        if not sample.get('submit_ns') or not sample['submit_ns']<=event.get('ns',0)<=lookup.get('ns',0):continue
        row=dict(event)
        row['prefix_readable']=bool(event['status']&8)
        if row['prefix_readable']:
            row['head']=[event['value']&0xffffffff,event['value']>>32]
            row['matches_pre_enqueue']=row['head']==sample['head']
            row['matches_lookup']=row['head']==lookup['head']
        candidates.append(row)
    result.update(state='overlapping_copy_candidates' if candidates else 'matching_writes_absent',
                  total=len(candidates),events=candidates[-16:],omitted=max(0,len(candidates)-16))
    return result
