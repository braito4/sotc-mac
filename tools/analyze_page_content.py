"""Compare retained CPU backing samples with first producer binding snapshots.

Private capture directory in, JSON report out. Matching bytes are not proof of
allocation ownership, an exclusive writer, or coherent CPU/GPU observation.
"""
import argparse,json
from pathlib import Path


def review(folder):
    folder=Path(folder)
    meta=json.loads((folder/'bundle-1.json').read_text())
    if meta.get('sequence')!=1 or not meta.get('completed'):
        raise ValueError('A completed first producer bundle is required')
    binary=folder/'bundle-1.bin'
    if binary.stat().st_size>32*1024*1024:raise ValueError('Bundle exceeds capture budget')
    raw=binary.read_bytes();rows=[];missing=[];history=[]
    for f in folder.glob('history-*.json'):
        h=json.loads(f.read_text())
        if h.get('clock')=='CLOCK_MONOTONIC':history.append(h)
    for binding in meta['bindings']:
        matching=[h for h in history if h['slot']==binding['slot'] and h['nodes'] and
                  all(h['nodes'][0]['span'][k]==binding[k] for k in ('metal','offset','bytes'))]
        if not matching:
            missing.append({'slot':binding['slot'],'reason':'no matching binding history'});continue
        h=min(matching,key=lambda x:x['report']);root=h['nodes'][0]['span']
        # Mapping observations remain explicitly tentative; never merge unrelated ranges.
        segments=[]
        if binding.get('fallback_observed') and binding.get('full_span') and binding.get('generation_before_copy')==binding.get('generation_after_copy'):
            for seg in binding.get('mapping',[]):
                if seg.get('mapped'):
                    relative=seg['start']-binding['copy_guest']
                    length=min(seg['end']-seg['start'],binding['copied']-relative)
                    if relative>=0 and length>0:segments.append((seg['physical'],length,relative,seg['start']))
        elif root.get('pool_known'):
            segments=[(root['physical'],binding['copied'],0,None)]
        if not segments:
            missing.append({'slot':binding['slot'],'reason':'no stable full-span mapping or direct physical identity'});continue
        seen=set()
        for f in sorted(folder.glob('page-history-'+str(h['report'])+'-slot-'+str(binding['slot'])+'-node-*.json')):
            page=json.loads(f.read_text())
            if page.get('schema')!=3 or page.get('clock')!='CLOCK_MONOTONIC':continue
            events=[e for bucket in page['buckets'] for e in bucket['events']]
            for event in events:
                sample=event.get('sample',{});length=sample.get('bytes',0);physical=sample.get('physical',0)
                key=(event['seq'],physical,length)
                if key in seen or sample.get('status')!=1 or not 0<length<=64:continue
                seen.add(key);data=bytes.fromhex(sample['hex'])
                if len(data)!=length:raise ValueError('Invalid sample extent')
                for start,size,relative,guest in segments:
                    if physical<start or physical+length>start+size:continue
                    offset=relative+physical-start
                    if offset+length>binding['copied']:continue
                    at=binding['before']+offset
                    if at<0 or at+length>len(raw):raise ValueError('Invalid bundle extent')
                    current=raw[at:at+length]
                    maps=[e for e in events if e['kind']=='map_locked' and e['physical']<=physical and physical+length<=e['physical']+e['bytes']]
                    latest=max((e['seq'] for e in maps),default=0)
                    expected_guest=None if guest is None else guest+physical-start
                    event_guest=event['guest']+physical-event['physical'] if event['guest'] else None
                    rows.append({'slot':binding['slot'],'report':h['report'],'node':page['node'],
                        'event_sequence':event['seq'],'kind':event['kind'],'event_ns':event['ns'],
                        'guest_return':event.get('guest_return',0),'sample_physical':physical,
                        'binding_relative_offset':offset,'sample_bytes':length,
                        'sample_begin_ns':sample['begin_ns'],'sample_end_ns':sample['end_ns'],
                        'sample_hex':data.hex(),'producer_before_hex':current.hex(),
                        'equal':data==current,'changed_bytes':sum(a!=b for a,b in zip(data,current)),
                        'latest_retained_covering_map_sequence':latest,
                        'precedes_latest_retained_map':bool(latest and event['seq']<latest),
                        'guest_alias_matches_binding':None if expected_guest is None else event_guest==expected_guest})
                    break
    return {'schema':1,'bundle_sequence':1,'comparisons':rows,'missing':missing,
            'limits':'Sparse samples only. Equal bytes do not prove no intervening writes. CPU samples are not atomic or GPU synchronized. Binding mappings are earlier observations; physical reuse and aliases remain possible. Old-lifetime writes are not current ownership. Absence of a sample does not prove initialization or no writer.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture');parser.add_argument('--output')
    args=parser.parse_args();result=review(args.capture);data=json.dumps(result,indent=2)+'\n'
    if args.output:
        # Evidence is private and never overwritten by an accidental second analysis.
        import os
        fd=os.open(args.output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'w') as f:f.write(data)
        maps=[r for r in result['comparisons'] if r['kind']=='map_locked' and not r['precedes_latest_retained_map']]
        print('Page content comparison: sampled map ranges='+str(len(maps))+' equal='+str(sum(r['equal'] for r in maps))+' changed='+str(sum(not r['equal'] for r in maps))+'; sparse observations, not proven writers')
    else:print(data,end='')


if __name__=='__main__':main()
