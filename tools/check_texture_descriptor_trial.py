"""Native layer policy and bounded diagnostic tests; no GPU/game execution."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=r'''
#include "texture_descriptor_diagnostic.h"
#include <cassert>
int main(int argc,char** argv){
 using namespace SotcTextureDescriptor;
 if(argc>1){CheckMetal(2,64,64,1,1,8192,37);return 99;}
 (void)argv;
 for(uint64_t n: {1ULL,2ULL,2048ULL}){assert(!InvalidLayers(2,n));CheckMetal(2,1,1,1,1,n,37);}
 assert(InvalidLayers(2,0)&&InvalidLayers(2,2049)&&InvalidLayers(2,8192));
 assert(!InvalidLayers(3,1)&&InvalidLayers(3,2));
 unsigned char raw[256];memset(raw,0xa5,sizeof raw);
 for(int n=0;n<64;++n)Record("fixture",2,1,1,1,1,8192,0,0,raw,sizeof raw);
 for(auto c:raw)assert(c==0xa5);
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'runtime/src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 valid=subprocess.run([str(p/'test')],capture_output=True,text=True);assert valid.returncode==0,valid.stderr
 assert valid.stderr.count('Texture descriptor candidate:')==32
 assert valid.stderr.count('raw_bytes=128')==32
 assert 'STOP_' not in valid.stderr
 bad=subprocess.run([str(p/'test'),'invalid'],capture_output=True,text=True)
 assert bad.returncode==23 and 'STOP_GPU_TEXTURE_DESCRIPTOR:' in bad.stderr and '8192' in bad.stderr
print('PASS: layer boundaries, 3D contract, unchanged raw bytes, bounded 32-record/128-byte capture and explicit invalid-descriptor exit')
