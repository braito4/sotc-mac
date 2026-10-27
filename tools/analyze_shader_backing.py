"""Numeric watched-prefix candidates around runtime backing writes."""


def review(origin,submission,journal):
    result=dict(state=journal.get('state','absent'),limitation=
        'Separate runtime backing overlaps, bounded writable binding candidates, and successful scalar/vector stores emitted by the checked ARM64 JIT overlay. CPU events carry translated instruction RIP and register value after the store. They do not cover interpreter stores, atomics, alignment-fault emulation, recompiled code, aliases or non-CPU writers; a numeric match is not allocation identity, exclusive ownership or a complete write history.')
    if journal.get('state')!='readable':return result
    lookup=origin.get('lookup',{});sample=submission.get('sample',{})
    result['counters']=journal.get('observation_counters',{})
    result['eligible_call_counters']=journal.get('eligible_call_counters')
    result['prior_binding_history']=journal.get('prior_binding_history')
    if not lookup.get('address'):
        result['state']='lookup_unavailable';return result
    candidates=[];scans=[]
    for event in journal['events']:
        if event['field']!=lookup['address']:continue
        if lookup.get('ns') and event['ns']>lookup['ns']:continue
        registration_match=(event['capture_id']==sample['capture_id'] and
            event['packet']==sample['packet']) if sample else None
        if event.get('phase')=='prior_binding_scan':
            if registration_match:scans.append(event)
            continue
        prior=event.get('phase')=='prior_writable_buffer_binding'
        if sample.get('submit_ns') and event['ns']<sample['submit_ns'] and not (prior and registration_match):continue
        row=dict(event,registration_matches_pre_enqueue=(event['capture_id']==sample['capture_id'] and
            event['packet']==sample['packet']) if sample else None)
        if event['head_available']:row['head_matches_lookup']=event['head']==lookup.get('head')
        candidates.append(row)
    result.update(state='numeric_overlap_candidates' if candidates else 'matching_candidates_absent',
        time_window_available=bool(sample.get('submit_ns') and lookup.get('ns')),
        events=candidates[-16:],total=len(candidates),omitted=max(0,len(candidates)-16),
        prior_binding_scan=scans[-1] if scans else None)
    return result
