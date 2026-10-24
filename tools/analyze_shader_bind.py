"""Compare bounded bind-time prefixes with one retained write/lookup pair."""


def review(origin,journal):
    result=dict(state=journal.get('state','absent'),limitation=
        'Separate eight-byte CPU observations, not atomic shader snapshots, allocation identity, original writer or GPU execution. Binding is after register update, not before enqueue.')
    if journal.get('state')!='readable':return result
    write=origin.get('address_write');lookup=origin.get('lookup')
    if not write or not lookup or not write.get('ns') or not lookup.get('ns') or write.get('address')!=lookup.get('address'):
        result['state']='write_or_lookup_unavailable';return result
    candidates=[e for e in journal['events'] if
        e.get('program')==lookup['program'] and e.get('address')==lookup['address'] and
        e.get('packet')==write['packet'] and e.get('raw_address_register')==write.get('hash') and
        e.get('actor')==write.get('actor') and e.get('thread')==lookup.get('thread')==write.get('thread') and
        e.get('thread') and write['ns']<=e.get('ns',0)<=lookup['ns']]
    if len(candidates)!=1:
        result.update(state='matching_bind_absent' if not candidates else 'ambiguous_bind',matches=len(candidates));return result
    event=candidates[0];result.update(state='observed',bind=event,
        bind_to_lookup_us=(lookup['ns']-event['ns'])/1000)
    if not event['head_available']:
        result['state']='bind_prefix_unavailable';return result
    same=event['head']==lookup.get('head')
    result.update(bind_prefix_matches_lookup=same,bind_prologue_present=event['prologue_present'],
        observation=('same_prefix_without_recognized_prologue' if not event['prologue_present'] else 'same_prefix')
            if same else 'sampled_prefix_changed')
    return result
