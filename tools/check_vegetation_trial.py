"""Synthetic actual-header regression; no game data or GPU execution."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=r'''
#include "vegetation_parameter_scale.h"
#include <cassert>
#include <limits>
int main(){
using namespace SotcFlatScale;
std::vector<Rule> rules;std::vector<uint32_t> out;
assert(Parse(nullptr,rules)&&rules.empty());
for(auto text:{"1:0:1:0","1:0:1:nan","1:0:1:inf","1:4096:2:1","1:0:17:1","1:0:1:1;","1:0:2:2;1:1:1:2","junk"})assert(!Parse(text,rules)&&rules.empty());
assert(Parse("a:1:2:0.5;a:4:1:2",rules));
float values[]={9,8,6,4,2,1};uint32_t words[6];memcpy(words,values,sizeof words);
assert(!Apply(11,words,6,rules,out)&&out.empty());
assert(!Apply(10,words,4,rules,out)&&out.empty());
assert(Apply(10,words,6,rules,out));
float changed[6];memcpy(changed,out.data(),sizeof changed);
assert(changed[0]==9&&changed[1]==4&&changed[2]==3&&changed[3]==4&&changed[4]==4&&changed[5]==1);
assert(!memcmp(words,values,sizeof words));
assert(Apply(10,words,6,rules,out));memcpy(changed,out.data(),sizeof changed);assert(changed[1]==4);
values[1]=std::numeric_limits<float>::quiet_NaN();memcpy(words,values,sizeof words);
assert(!Apply(10,words,6,rules,out)&&out.empty());
assert(Parse("a:0:1:1",rules));assert(!Apply(10,words,6,rules,out));
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
    folder=Path(folder);(folder/'test.cpp').write_text(source)
    subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'runtime/src'),str(folder/'test.cpp'),'-o',str(folder/'test')],check=True)
    subprocess.run([str(folder/'test')],check=True)
print('PASS: bounded scaling, identity, unknown shader, short input, nonfinite data, source preservation and repeat binding')
