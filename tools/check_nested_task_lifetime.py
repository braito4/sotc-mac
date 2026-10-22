"""Compile production coroutine promise/loops with synthetic suspended children; no GPU."""
from pathlib import Path
import re
import subprocess
import tempfile
from build_gpu import source_path
from check_buffer_copy_chunks import function

ROOT = Path(__file__).resolve().parents[1]


def fixture(tree):
    base=tree/'gpu/shadps4/video_core/amdgpu'
    source=(base/'liverpool.cpp').read_text()
    header=(base/'liverpool.h').read_text()
    task=function(header,'    struct Task {')+';'
    task=task.replace('struct promise_type {',r'''struct promise_type {
            static void* operator new(std::size_t size) {++allocated;return ::operator new(size);}
            static void operator delete(void* pointer,std::size_t) {++freed;::operator delete(pointer);}
''',1)
    pattern=r'while \(!task\.handle\.done\(\)\) \{\n\s+(?:YIELD_CE\(\);\n\s+RESUME_CE\(task\);|YIELD_GFX\(\);\n\s+RESUME_GFX\(task\);|YIELD_ASC\(vqid\);\n\s+RESUME_ASC\(task, vqid\);)\n\s*\}(?:\n\s*// (?:AjncSystem|Preserve completed)[^\n]*\n\s*(?:task\.handle\.destroy\(\);|RetireCompletedTask\(rasterizer, task.handle\);))?'
    loops=re.findall(pattern,source)
    if len(loops)!=5:
        raise RuntimeError('Unexpected nested command/self-test task layout; review before adapting.')
    deferred='static void RetireCompletedTask(' in source
    adapted=all('task.handle.destroy();' in loop or 'RetireCompletedTask(' in loop for loop in loops)
    retirement=function(source,'static void RetireCompletedTask(') if deferred else ''
    parents='\n'.join('Task Parent'+str(i)+'(){[[maybe_unused]] unsigned vqid=0;auto task=Child();'
                       'task.handle.resume();'+loop+'co_return;}' for i,loop in enumerate(loops))
    return r'''
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <cstring>
using u8=unsigned char;
#define ASSERT(x) assert(x)
namespace BbToggle { [[maybe_unused]] constexpr unsigned PipelinedTasks=1; }
namespace Vulkan {
struct Rasterizer {
    template<class F> void RunInOrder(F f,const void* data,unsigned,unsigned,bool) {
        f(*this,static_cast<const u8*>(data));
    }
};
}
Vulkan::Rasterizer* rasterizer=nullptr;
@RETIREMENT@
unsigned allocated=0,freed=0;
#define UNREACHABLE_MSG(...) std::abort()
@TASK@
#define YIELD_CE() co_yield Task::promise_type::empty{}
#define YIELD_GFX() YIELD_CE()
#define YIELD_ASC(id) do {(void)(id);YIELD_CE();}while(false)
#define RESUME_CE(task) task.handle.resume()
#define RESUME_GFX(task) RESUME_CE(task)
#define RESUME_ASC(task,id) do {(void)(id);RESUME_CE(task);}while(false)
__attribute__((noinline)) Task Child(){YIELD_CE();YIELD_CE();co_return;}
@PARENTS@
int main(){
    constexpr bool adapted=@ADAPTED@;
    Task (*parents[])()={Parent0,Parent1,Parent2,Parent3,Parent4};
    for(auto parent:parents)for(unsigned i=0;i<100;++i){
        const unsigned before=freed;
        auto task=parent();task.handle.resume();
        assert(!task.handle.done() && freed==before); // Parent and child are suspended/live.
        task.handle.resume();assert(!task.handle.done() && freed==before);
        task.handle.resume();assert(task.handle.done());
        assert(freed==before+(adapted?1:0)); // Child released exactly once, only on completion.
        task.handle.destroy();assert(freed==before+(adapted?2:1));
    }
    assert(allocated==1000 && freed==(adapted?1000:500));
    std::printf("nested frames still live=%u; all five production completion sites exercised\n",allocated-freed);
    return adapted?0:2;
}
'''.replace('@TASK@',task).replace('@PARENTS@',parents).replace('@ADAPTED@','true' if adapted else 'false').replace('@RETIREMENT@',retirement)


def main():
    with tempfile.TemporaryDirectory(prefix='nested-task-cpu-',dir=ROOT/'work') as directory:
        root=Path(directory)
        for name,tree,expected in (('upstream',ROOT/'runtime/upstream',2),('adapted',source_path(ROOT),0)):
            cpp=root/(name+'.cpp');binary=root/name;cpp.write_text(fixture(tree))
            subprocess.run(['clang++','-std=c++20','-O0','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=15)
            if result.returncode!=expected:
                raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: '+result.stderr)
            print(name+': '+result.stdout.strip())
        tree=source_path(ROOT)
        base=tree/'gpu/shadps4/video_core/amdgpu'
        source=(base/'liverpool.cpp').read_text()
        header=(base/'liverpool.h').read_text()
        retirement=function(source,'static void RetireCompletedTask(')
        task=function(header,'    struct Task {')+';'
        task=task.replace('struct promise_type {',r'''struct promise_type {
            static void* operator new(std::size_t size) {++allocated;return ::operator new(size);}
            static void operator delete(void* pointer,std::size_t) {++freed;::operator delete(pointer);}
''',1)
        # Every actual release site must use the reviewed recording-consumer boundary.
        if source.count('RetireCompletedTask(rasterizer,') != 7:
            raise RuntimeError('Review all five nested, CE and top-level frame releases.')
        template=(ROOT/'tests/task_retirement_fixture.cpp.in').read_text()
        for label,release,expected in (
            ('immediate-control','task.handle.destroy();',2),
            ('ordered-retirement','RetireCompletedTask(&recorder, task.handle);',0),
        ):
            text=(template.replace('@TASK@',task).replace('@RETIREMENT@',retirement)
                  .replace('@RELEASE@',release).replace('@ADAPTED@','true' if expected==0 else 'false'))
            cpp=root/(label+'.cpp'); binary=root/label; cpp.write_text(text)
            subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined',str(cpp),'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=15)
            if result.returncode!=expected:
                raise RuntimeError(label+': '+result.stdout+result.stderr)
            print(label+': '+result.stdout.strip())
    print('PASS: completed child frames reclaimed; suspended frames preserved; CPU-only synthetic workload')


if __name__=='__main__':
    main()
