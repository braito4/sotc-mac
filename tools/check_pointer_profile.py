"""Check private probe signatures in the actual prepared input, without execution."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from analyze_index_capture import validate as validate_index_capture


def check(image,profile,selection,field_selection=None):
    spec=json.loads(Path(profile).read_text())
    if selection!=spec['selection']:raise ValueError('pointer selection differs from reviewed profile')
    if 'field_selection' in spec and field_selection!=spec['field_selection']:
        raise ValueError('field selection differs from reviewed profile')
    scenarios=spec.get('scenarios',[])
    if 'index_capture' in spec:
        validate_index_capture(spec['index_capture'],len(spec.get('field_selection','').split(',')) if spec.get('field_selection') else 0,len(selection.split(',')) if selection else 0)
    if not isinstance(scenarios,list):raise ValueError('invalid scenario list')
    for scenario in scenarios:
        if not isinstance(scenario,dict) or not isinstance(scenario.get('name'),str):
            raise ValueError('invalid scenario name')
        for kind,config in [('cpu',spec.get('field_selection','')),('pointer',selection)]:
            indices=scenario.get(kind,[]);count=len(config.split(',')) if config else 0
            if not isinstance(indices,list) or any(type(i)!=int or not 0<=i<count for i in indices):
                raise ValueError('invalid scenario target indices')
    with Path(image).open('rb') as source:
        magic=source.read(8)
        if magic not in (b'BBPROBE2',b'BBPROBE5'):raise ValueError('unsupported input')
        def q():
            data=source.read(8)
            if len(data)!=8:raise ValueError('truncated input')
            return struct.unpack('<Q',data)[0]
        size,entry,ns,nr,ni,cap=[q() for _ in range(6)]
        if not 0<size<=512*1024*1024 or ns>64 or nr>1000000 or ni>100000:raise ValueError('invalid input limits')
        if magic==b'BBPROBE5':
            metadata=[q() for _ in range(6)]
            if metadata[-1]>8:raise ValueError('invalid modules')
            source.seek(metadata[-1]*56,1);nb=q()
            if nb>ni:raise ValueError('invalid bindings')
            source.seek(nb*24,1)
        source.seek(ns*24+ni*128+nr*32,1);start=source.tell()
        if Path(image).stat().st_size!=start+size:raise ValueError('input payload size mismatch')
        expected_sites={int(row.split(':')[0],0) for row in selection.split(',')}
        if expected_sites!={int(row['site'],0) for row in spec['signatures']}:raise ValueError('missing probe signatures')
        signatures=list(spec['signatures'])
        if 'field_selection' in spec:
            fields={int(row.split(':')[0],0) for row in spec['field_selection'].split(',')}
            if fields!={int(row['site'],0) for row in spec['field_signatures']}:
                raise ValueError('missing field probe signatures')
            signatures+=spec['field_signatures']
        for row in signatures:
            at=int(row['site'],0);expected=bytes.fromhex(row['bytes'])
            if not expected or len(expected)>64 or at>size or len(expected)>size-at:raise ValueError('invalid signature span')
            source.seek(start+at)
            if source.read(len(expected))!=expected:raise ValueError('reviewed pointer probe code differs from prepared input')
    return len(expected_sites)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,required=True);parser.add_argument('--profile',type=Path,required=True)
    parser.add_argument('--selection',required=True);parser.add_argument('--output',type=Path)
    parser.add_argument('--field-selection');args=parser.parse_args()
    try:
        count=check(args.image,args.profile,args.selection,args.field_selection)
        if args.output:
            digest=hashlib.sha256()
            with args.image.open('rb') as source:
                for block in iter(lambda:source.read(1024*1024),b''):digest.update(block)
            result=dict(status='matched',sites=count,validated_before_launch=True,
                profile_sha256=hashlib.sha256(args.profile.read_bytes()).hexdigest(),
                selection_sha256=hashlib.sha256(args.selection.encode()).hexdigest(),
                image_sha256=digest.hexdigest())
            spec=json.loads(args.profile.read_text())
            if 'field_selection' in spec:
                result['field_selection_sha256']=hashlib.sha256(args.field_selection.encode()).hexdigest()
                result['scenarios']=spec.get('scenarios',[])
            if 'index_capture' in spec:result['index_capture']=spec['index_capture']
            with args.output.open('x') as out:json.dump(result,out,indent=2);out.write('\n')
    except (OSError,ValueError,KeyError,TypeError,struct.error) as error:parser.exit(24,'Pointer profile rejected: '+str(error)+'\n')
    print(f'PASS: {count} private pointer probe signatures match actual prepared input; no guest execution.')


if __name__=='__main__':main()
