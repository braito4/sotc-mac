"""Review queued descriptor associations and possible prior encoded writers.

An encoded writable binding is a candidate, never proof of which bytes it wrote.
Native object epochs are not guest allocation generations. Private metadata only.
"""
import argparse,json,os,re
from pathlib import Path

def overlap(a,b):
    if not a.get('bytes') or not b.get('bytes'): return None
    if a.get('native_epoch') and a.get('native_epoch')==b.get('native_epoch') and a.get('metal')==b.get('metal'):
        start,end=a['offset'],a['offset']+a['bytes'];lo,hi=b['offset'],b['offset']+b['bytes'];kind='native_epoch'
    elif a.get('pool_known') and b.get('pool_known') and a.get('pool_epoch') and a['pool_epoch']==b.get('pool_epoch'):
        start,end=a['physical'],a['physical']+a['bytes'];lo,hi=b['physical'],b['physical']+b['bytes'];kind='physical_pool'
    else:return None
    n=max(0,min(end,hi)-max(start,lo))
    return {'kind':kind,'overlap_bytes':n,'covers_consumer':start<=lo and end>=hi} if n else None

def review(folder):
    records={};p=Path(folder)
    files=list(p.glob('route-*.json'))
    if len(files)>384:raise ValueError('Route report budget exceeded')
    for f in files:
        match=re.fullmatch(r'route-([1-9][0-9]*)-(before|encoded|completed)\.json',f.name)
        if not match:continue
        if f.stat().st_size>1024*1024:raise ValueError('Route metadata too large')
        d=json.loads(f.read_text());route=int(match[1]);phase=match[2]
        if d.get('schema')!=1 or d.get('route')!=route or d.get('phase')!=phase:raise ValueError('Route file identity mismatch')
        records.setdefault(route,{})[phase]=d
    rows=[];candidates=[];prior=[]
    for route,phases in sorted(records.items(),key=lambda x:x[1].get('before',{}).get('encoding_ns',0)):
        before=phases.get('before');encoded=phases.get('encoded');done=phases.get('completed')
        if before is None:raise ValueError('Route is missing before metadata')
        if encoded and (encoded['shader_request']!=before['shader_request'] or encoded['entry']!=before['entry']):raise ValueError('Dispatch identity changed')
        if done and (not encoded or done['command']!=encoded['command']):raise ValueError('Completion does not match encoded command')
        bindings=before['bindings'];reasons={}
        for b in bindings:
            reason=b.get('audit',{}).get('reason','unknown');reasons[reason]=reasons.get(reason,0)+1
        rows.append({'route':route,'shader_request':before['shader_request'],'entry':before['entry'],
                     'cross_thread':before['preparation_thread']!=before['encoder_thread'],
                     'matched':sum(bool(b.get('audit',{}).get('matched')) for b in bindings),
                     'bindings':len(bindings),'reasons':reasons,'encoded':bool(encoded),
                     'completed':done.get('success') if done else None,
                     'command_changed_by_diagnostic':bool(encoded and before['command']!=encoded['command'])})
        for b in bindings:
            audit=b.get('audit',{})
            if not audit.get('matched') or audit.get('written'):continue
            for old,writer,completion in prior:
                if old['encoding_ns']>=before['encoding_ns']:continue
                relation=overlap(writer,b)
                if not relation:continue
                candidates.append({'consumer_route':route,'consumer_slot':b['slot'],
                                   'candidate_route':old['route'],'candidate_slot':writer['slot'],
                                   'candidate_encoded_ns':old['encoding_ns'],
                                   'completion_observed_before_consumer':bool(completion and completion.get('success') and completion['ns']<=before['encoding_ns']),**relation})
                if len(candidates)>4096:raise ValueError('Candidate budget exceeded')
        if encoded:
            for b in encoded['bindings']:
                a=b.get('audit',{})
                if a.get('matched') and a.get('written'):prior.append((encoded,b,done))
    return {'schema':1,'routes':rows,'writer_candidates':candidates,
            'missing_coverage':not bool(rows),
            'limits':'Only selected large bindings, bounded queue packets and observed native epochs. Missing encoded/completed files are unknown, including guard termination. A completed writable binding is not byte-write proof or guest allocation ownership. Diagnostic guard substitutions can change the encoding command and native input bindings.'}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture');parser.add_argument('--output');args=parser.parse_args()
    result=review(args.capture);data=json.dumps(result,indent=2)+'\n'
    if args.output:
        fd=os.open(args.output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'w') as f:f.write(data)
        rows=result['routes'];print('Binding route: dispatches='+str(len(rows))+' matched_bindings='+str(sum(r['matched'] for r in rows))+' writer_candidates='+str(len(result['writer_candidates']))+'; bounded observations, not writer proof')
    else:print(data,end='')
if __name__=='__main__':main()
