"""Compare selected descriptor, CPU alias/backing samples and ordered GPU snapshots."""
import argparse,json,os
from pathlib import Path


def review(folder):
    p=Path(folder);meta=json.loads((p/'bundle-1.json').read_text())
    if meta.get('sequence')!=1 or not meta.get('completed'):raise ValueError('Completed first bundle required')
    path=p/'bundle-1.bin'
    if path.stat().st_size>32*1024*1024:raise ValueError('Bundle exceeds budget')
    raw=path.read_bytes();rows=[];missing=[];descriptors=[]
    for b in meta['bindings']:
        d=b.get('binding_audit',{})
        if not d.get('matched'):
            missing.append({'slot':b['slot'],'reason':d.get('reason','no exact selected descriptor/native association')});continue
        sharp=bytes.fromhex(d['sharp_hex'])
        if len(sharp)!=16:raise ValueError('Descriptor extent')
        address=int.from_bytes(sharp[:8],'little')&((1<<40)-1)
        descriptors.append({'slot':b['slot'],'raw_address':address,'recorded_guest':d['guest'],
                            'address_matches':address==d['guest'],'requested_bytes':d['requested_bytes'],
                            'clamped_bytes':d['bytes'],'native_bytes':b['bytes'],'adjust':d['adjust']})
        for phase in ('before','after'):
            f=p/f"binding-audit-1-slot-{b['slot']}-{phase}.json"
            if not f.exists():missing.append({'slot':b['slot'],'phase':phase,'reason':'host report unavailable'});continue
            h=json.loads(f.read_text())
            if h['sequence']!=1 or h['slot']!=b['slot'] or h['guest']!=d['guest'] or h['phase']!=(phase=='after') or h['clock']!='CLOCK_MONOTONIC':raise ValueError('Report association mismatch')
            reference='after' if phase=='after' and b.get('after_bytes') else 'before'
            extent=b['after_bytes'] if reference=='after' else b['copied']
            for s in h['samples']:
                row={'slot':b['slot'],'phase':phase,'guest':s['guest'],'physical':s['physical'],
                     'generation':h['generation'],'guest_read':bool(s['guest_read']),
                     'backing_read':bool(s['backing_read']),'gpu_reference':reference,
                     'sample_begin_ns':s['begin_ns'],'sample_end_ns':s['end_ns'],
                     'report_truncated':h['truncated'],'cpu_views_equal':None,
                     'guest_equals_gpu':None,'backing_equals_gpu':None}
                n=s['bytes'];offset=s['guest']-d['guest']+d['adjust']
                if (s['guest_read'] or s['backing_read']) and not 0<n<=64:raise ValueError('Sample extent')
                values={}
                for key in ('guest','backing'):
                    if s[key+'_read']:
                        values[key]=bytes.fromhex(s[key+'_hex'])
                        if len(values[key])!=n:raise ValueError('Sample length mismatch')
                if len(values)==2:row['cpu_views_equal']=values['guest']==values['backing']
                if values and offset>=0 and offset+n<=extent:
                    at=b[reference]+offset
                    if at<0 or at+n>len(raw):raise ValueError('GPU snapshot extent')
                    gpu=raw[at:at+n]
                    for key,value in values.items():row[key+'_equals_gpu']=value==gpu
                rows.append(row)
    return {'schema':1,'descriptors':descriptors,'comparisons':rows,'missing':missing,
            'limits':'Different observation times. CPU alias/backing samples are sequential, not atomic or GPU synchronized. After samples follow the existing completion wait; inputs without post snapshots are compared with GPU-before. Equal bytes are not allocation ownership or initialization proof. Missing/protected samples remain unknown.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture');parser.add_argument('--output');args=parser.parse_args()
    result=review(args.capture);data=json.dumps(result,indent=2)+'\n'
    if args.output:
        fd=os.open(args.output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'w') as f:f.write(data)
        r=result['comparisons'];print('Binding audit comparison: descriptors='+str(len(result['descriptors']))+' CPU view differences='+str(sum(x['cpu_views_equal'] is False for x in r))+' backing/GPU differences='+str(sum(x['backing_equals_gpu'] is False for x in r))+'; sampled observations, different instants')
    else:print(data,end='')

if __name__=='__main__':main()
