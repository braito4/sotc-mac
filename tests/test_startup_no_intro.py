"""Original startup string relocation fixtures; no game or GPU execution."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from check_startup_profile import check


class StartupTests(unittest.TestCase):
    def test_profile_checks_exact_image_instruction_and_literal(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            folder=Path(folder);image=folder/'boot.bin';profile=folder/'profile.json'
            data=bytearray(512);data[:7]=bytes.fromhex('488d35')+struct.pack('<i',128-7)
            literal=b'+VSyncWindow=10%\0';data[128:128+len(literal)]=literal
            image.write_bytes(struct.pack('<8s6Q3Q',b'BBPROBE2',512,0,1,0,0,0,0,512,7)+data)
            spec=dict(schema=1,site=0,expected=data[:7].hex(),image_sha256=hashlib.sha256(image.read_bytes()).hexdigest())
            profile.write_text(json.dumps(spec));self.assertEqual(check(image,profile)['status'],'matched')
            for changed in [dict(spec,site=511),dict(spec,expected='00'*7),dict(spec,image_sha256='0'*64)]:
                profile.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):check(image,profile)

    def test_native_bounded_redirection_and_refusals(self):
        source=r'''
#include "startup_no_intro.h"
#include <assert.h>
int main(void) {
    unsigned char data[1024]={0},saved[7];
    data[0]=0x48;data[1]=0x8d;data[2]=0x35;
    int32_t old=128-7;memcpy(data+3,&old,4);
    memcpy(data+128,"+VSyncWindow=10%",16);memcpy(saved,data,7);
    assert(sotc_startup_no_intro(data,256,255,data+512,128)==-1);
    assert(sotc_startup_no_intro(data,256,0,data+512,1)==-1);
    data[128]='x';assert(sotc_startup_no_intro(data,256,0,data+512,128)==-1);data[128]='+';
    assert(!memcmp(saved,data,7));
    assert(!sotc_startup_no_intro(data,256,0,data+512,128));
    int32_t now;memcpy(&now,data+3,4);assert(now==512-7);
    assert(!strcmp((char *)data+7+now,"+VSyncWindow=10% +nointro"));
    assert(!strcmp((char *)data+128,"+VSyncWindow=10%"));
    return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            folder=Path(folder);c=folder/'fixture.c';c.write_text(source);binary=folder/'fixture'
            subprocess.run(['clang','-Wall','-Wextra','-Werror','-I'+str(ROOT/'runtime/src'),str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)


if __name__=='__main__':unittest.main()
