"""Validate the private engine startup-option site against the actual input."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from check_cpu_experiment import payload


def check(image,profile):
    raw=Path(profile).read_bytes()
    if len(raw)>4096:raise ValueError('startup profile too large')
    spec=json.loads(raw);site=spec.get('site')
    if spec.get('schema')!=1 or type(site)!=int or not 0<=site<=512*1024*1024:
        raise ValueError('invalid startup profile')
    with Path(image).open('rb') as source:
        digest=hashlib.file_digest(source,'sha256').hexdigest()
        if digest!=spec.get('image_sha256'):raise ValueError('startup input hash mismatch')
        source.seek(0);start,segments=payload(source)
        if not any(flags&1 and at<=site and site+7<=at+length for at,length,flags in segments):
            raise ValueError('startup site is not executable')
        source.seek(start+site);code=source.read(7)
        if len(code)!=7 or code[:3]!=bytes.fromhex('488d35') or code.hex()!=spec.get('expected'):
            raise ValueError('startup instruction mismatch')
        target=site+7+struct.unpack_from('<i',code,3)[0]
        literal=b'+VSyncWindow=10%\0'
        if not any(at<=target and target+len(literal)<=at+length for at,length,flags in segments):
            raise ValueError('startup string outside mapped segment')
        source.seek(start+target)
        if source.read(len(literal))!=literal:raise ValueError('startup original string mismatch')
    return dict(status='matched',option='+nointro',site=site,image_sha256=digest,
                profile_sha256=hashlib.sha256(raw).hexdigest())


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--image',type=Path,required=True);p.add_argument('--profile',type=Path,required=True)
    p.add_argument('--output',type=Path);p.add_argument('--print-site',action='store_true');a=p.parse_args()
    try:result=check(a.image,a.profile)
    except (ValueError,OSError,KeyError) as error:p.error(str(error))
    if a.output:a.output.write_text(json.dumps(result,indent=2)+'\n')
    if a.print_site:print(hex(result['site']))
