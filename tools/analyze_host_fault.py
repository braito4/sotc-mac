"""Map private native register candidates to startup binary ranges, offline.

No memory reads, execution or stack-unwinding guesses. A matching range identifies
a loaded image candidate, not a symbol, validated caller or corruption cause.
"""
import argparse
import json
from pathlib import Path
import re


def analyze(text):
    images=[]
    for match in re.finditer(r'^Host image: load=0x([0-9a-f]+) text=0x([0-9a-f]+) size=0x([0-9a-f]+) uuid=(\S+) name=(.+)$',text,re.M):
        load,start,size,uuid,name=match.groups()
        images.append(dict(load=int(load,16),start=int(start,16),size=int(size,16),uuid=uuid,name=name))
    context=text.rsplit('Host ARM64 context: private',1)
    registers={}
    complete=False
    if len(context)==2:
        body=context[1].split('Host ARM64 context: end',1)
        complete=len(body)==2
        for name,value in re.findall(r'\b(x\d+|pc|lr|fp|sp)=0x([0-9a-f]+)',body[0]):
            registers[name]=int(value,16)
    candidates=[]
    for name in ('pc','lr','x16','x17'):
        if name not in registers:continue
        value=registers[name]
        matches=[i for i in images if value>=i['start'] and value-i['start']<i['size']]
        candidates.append(dict(register=name,value=value,image_matches=[
            dict(name=i['name'],uuid=i['uuid'],load=i['load'],text_offset=value-i['start']) for i in matches]))
    return dict(host_context_present=bool(registers),host_context_complete=complete,
                startup_images=len(images),register_candidates=candidates,
                limitation='Startup image/range candidates only; LR is untrusted. No symbol, caller, writer or fault cause established.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();report=analyze(args.log.read_text(errors='replace'))
    data=json.dumps(report,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(data)
    print(data,end='')


if __name__=='__main__':main()
