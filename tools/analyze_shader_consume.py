"""Conservative fresh-consumption prefix association; separate CPU reads only."""


def review(origin,journal):
    result=dict(state=journal.get('state','absent'),limitation=
        'Separate eight-byte CPU observations. No atomic shader snapshot, allocation identity, continuous watch or original writer. A missing event is a coverage gap.')
    if journal.get('state')!='readable':return result
    lookup=origin.get('lookup',{});write=origin.get('address_write',{})
    if write.get('actor') not in (1,2,5,6,7) or not lookup.get('ns') or not write.get('ns'):
        result['state']='write_or_lookup_unavailable';return result
    compute=write['actor'] in (6,7)
    candidates=[]
    for event in journal['events']:
        index=event['register_index']+(0 if compute else 0x2c00)
        if event['origin']!=lookup.get('address') or event['packet']!=write.get('packet') or event['compute']!=compute:continue
        if not write.get('first',0)<=index or index-write['first']+2>write.get('words',0):continue
        if not event.get('thread') or event['thread']!=lookup.get('thread') or event['thread']!=write.get('thread'):continue
        if not 0<event.get('ns',0)<=write['ns']<=lookup['ns']:continue
        candidates.append(event)
    if len(candidates)!=1:
        result.update(state='matching_consumption_absent' if not candidates else 'ambiguous_consumption',matches=len(candidates));return result
    event=candidates[0]
    result.update(state='repeated_packet_ambiguous' if event['ambiguous_packet'] else 'observed',sample=event,
        consume_to_lookup_us=(lookup['ns']-event['ns'])/1000)
    if event['ambiguous_packet']:return result
    if not event['registration_eligible']:coverage='sample_not_eligible_for_registration'
    elif event['watch_state_after_scan']!=5:coverage='watch_not_confirmed_after_registration_attempt'
    elif event['watch_state_at_consume'] in (1,2):coverage='watch_present_after_scan_missing_at_consume'
    elif event['watch_state_at_consume']==5:
        coverage='same_registration_at_consume' if (event['watch_packet'],event['watch_capture_id'])==(event['packet'],event['capture_id']) else 'different_registration_at_consume'
    else:coverage='watch_state_unavailable_at_consume'
    result['watch_coverage_observation']=coverage
    if event['head_available']:
        result['consume_prefix_matches_lookup']=event['head']==lookup.get('head')
        result['pre_enqueue_to_consume_changed']=event['sampled_prefix_changed'] if event['pre_enqueue_available'] else None
    return result


def mapping_review(consumption,journal):
    result=dict(state=journal.get('state','absent'),limitation=
        'Separate runtime pool-offset observations. Same offset does not prove allocation identity, continuous mapping or absence of free/reuse; a different offset is an observed backing change, not its cause.')
    sample=consumption.get('sample')
    if journal.get('state')!='readable':return result
    if consumption.get('state')!='observed' or not sample or not sample.get('sampled_prefix_changed'):
        result['state']='changed_consumption_unavailable';return result
    matches=[e for e in journal['events'] if all(e.get(k)==sample.get(k) for k in ('capture_id','packet','origin','thread','ns'))
        and e.get('head')==sample.get('head') and e.get('pre_enqueue_head')==sample.get('pre_enqueue_head')]
    if len(matches)!=1:
        result.update(state='matching_mapping_absent' if not matches else 'ambiguous_mapping',matches=len(matches));return result
    event=matches[0];result['sample']=event
    if not event['physical_before_available'] or not event['physical_at_consume_available']:
        result['state']='pool_offset_unavailable';return result
    result['state']='same_observed_pool_offset' if event['physical_before']==event['physical_at_consume'] else 'observed_pool_offset_changed'
    return result
