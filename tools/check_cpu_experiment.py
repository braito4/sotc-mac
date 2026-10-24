"""Validate one private, exact-input CPU comparison; never execute guest code."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct

VARIANTS=('base','caller','buffer','table')


def profile(path,variant):
    data=Path(path).read_bytes()
    if len(data)>32768:raise ValueError('profile too large')
    spec=json.loads(data)
    if not isinstance(spec,dict) or spec.get('schema')!=1 or not isinstance(spec.get('variants'),dict) or set(spec['variants'])!=set(VARIANTS[1:]):
        raise ValueError('unexpected comparison profile')
    if not re.fullmatch('[0-9a-f]{64}',spec.get('image_sha256','')):
        raise ValueError('missing exact input hash')
    for row in spec['variants'].values():
        if not isinstance(row,dict):raise ValueError('invalid variant record')
        begin,end=row.get('begin'),row.get('end')
        if type(begin)!=int or type(end)!=int or not 0<=begin<end<=512*1024*1024 or end-begin>65536:
            raise ValueError('invalid bounded function interval')
        if not re.fullmatch('[0-9a-f]{64}',row.get('sha256','')):
            raise ValueError('missing function hash')
    if variant not in VARIANTS:raise ValueError('unknown comparison variant')
    row=None if variant=='base' else spec['variants'][variant]
    interval='' if row is None else f'0x{row["begin"]:x}:0x{row["end"]:x}'
    return spec,row,interval,hashlib.sha256(data).hexdigest()


def payload(stream):
    magic=stream.read(8)
    if magic not in (b'BBPROBE2',b'BBPROBE5'):raise ValueError('unsupported prepared input')
    def q():
        data=stream.read(8)
        if len(data)!=8:raise ValueError('truncated prepared header')
        return struct.unpack('<Q',data)[0]
    size,entry,ns,nr,ni,cap=[q() for _ in range(6)]
    if not 0<size<=512*1024*1024 or not 0<ns<=64 or nr>1000000 or ni>100000:
        raise ValueError('invalid prepared limits')
    if magic==b'BBPROBE5':
        metadata=[q() for _ in range(6)]
        if not 0<metadata[-1]<=8:raise ValueError('invalid linked modules')
        stream.seek(metadata[-1]*56,1);nb=q()
        if nb>ni:raise ValueError('invalid bindings')
        stream.seek(nb*24,1)
    segments=[]
    for _ in range(ns):
        at,length,flags=q(),q(),q()
        if at>size or length>size-at or flags>7:raise ValueError('invalid segment')
        segments.append((at,length,flags))
    stream.seek(ni*128+nr*32,1)
    start=stream.tell()
    if os.fstat(stream.fileno()).st_size!=start+size:raise ValueError('prepared payload size differs')
    return start,segments


def check(path,variant,image=None,expected_range=None):
    spec,row,interval,digest=profile(path,variant)
    if expected_range is not None and expected_range!=interval:
        raise ValueError('selected range differs from reviewed variant')
    result=dict(status='configuration_only',variant=variant,range=interval,
                profile_sha256=digest,image_sha256=spec['image_sha256'])
    if image is not None:
        with Path(image).open('rb') as source:
            before=os.fstat(source.fileno())
            actual=hashlib.file_digest(source,'sha256').hexdigest()
            if actual!=spec['image_sha256']:raise ValueError('prepared input hash differs')
            source.seek(0);start,segments=payload(source)
            if row is not None:
                begin,end=row['begin'],row['end']
                if not any(flags&1 and begin>=at and end<=at+length for at,length,flags in segments):
                    raise ValueError('range is not contained in an executable segment')
                source.seek(start+begin);code=source.read(end-begin)
                if hashlib.sha256(code).hexdigest()!=row['sha256']:
                    raise ValueError('reviewed function bytes differ')
                result['function_sha256']=row['sha256']
            after=os.fstat(source.fileno())
            if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):
                raise ValueError('prepared input changed during validation')
        result['status']='matched'
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',type=Path,required=True)
    parser.add_argument('--variant',choices=VARIANTS,required=True)
    parser.add_argument('--range',dest='interval')
    parser.add_argument('--image',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--print-range',action='store_true')
    args=parser.parse_args()
    try:
        if args.output and not args.image:raise ValueError('saved validation requires an input')
        result=check(args.profile,args.variant,args.image,args.interval)
        if args.output:
            fd=os.open(args.output,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
            with os.fdopen(fd,'w') as out:json.dump(result,out,indent=2);out.write('\n')
    except (OSError,ValueError,KeyError,TypeError,AttributeError,struct.error) as error:
        parser.exit(24,'CPU comparison rejected: '+str(error)+'\n')
    if args.print_range:print(result['range'])
    else:print(f'PASS: CPU comparison {args.variant}: {result["status"]}; no guest execution.')


if __name__=='__main__':main()
