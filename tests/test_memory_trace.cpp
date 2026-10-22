// Original CPU metadata fixtures: no GPU, guest memory or sound is accessed.
#include "gpu_memory_trace.h"
#include <cassert>
#include <memory>
#include <thread>
#include <vector>
using namespace SotcMemory;
int main(int argc, char** argv) {
    assert(argc == 2);
    ImageTable<4> table;
    for (U64 id = 1; id <= 4; ++id) assert(table.Insert(Image{.id=id}));
    assert(!table.Insert(Image{.id=5}) && !table.Insert(Image{.id=1}));
    assert(table.Erase(2) && !table.Erase(2));
    assert(table.Insert(Image{.id=5}) && table.Find(5));
    {
        BindingScope outer{{11,2,3,4,5,true,false}};
        { BindingScope inner{{22,6,7,8,9,false,true}}; assert(binding.shader == 22); }
        assert(binding.shader == 11);
    }
    assert(binding.shader == 0);
    if (!std::strcmp(argv[1], "errors")) Observe([](Recorder&) { throw 1; });
    if (!std::strcmp(argv[1], "stress")) {
        auto recorder = std::make_unique<Recorder>(Limits{4096,4096,32768});
        std::vector<std::thread> threads;
        for (unsigned worker=0; worker<4; ++worker) threads.emplace_back([&,worker] {
            for (unsigned i=1; i<=100; ++i) {
                const U64 id = worker*1000+i;
                recorder->Allocate(id, 4096, {});
                recorder->Owner(id,id,0,0,0,1,3);
                recorder->Free(id,4096);
            }
        });
        for (auto& thread:threads) thread.join();
        for (unsigned i=0; i<100; ++i) recorder->Buffer("copy_begin", {.size=Large});
        for (unsigned i=0; i<100; ++i) recorder->Failure(8,100,200);
        return 0;
    }
    const ImageDescription description{640,360,1,1,1,1,37,16,1};
    ImageAllocated(1,64ull<<20,description);
    ImageOwner(1,101,4096,1024,64,10,1);
    ImageOwner(1,101,4096,1024,0,11,2);
    ImageOwner(1,101,4096,1024,0,12,3);
    ImageAllocated(2,32ull<<20,description);
    ImageOwner(2,102,8192,1024,64,13,1);
    ImageFreed(2,32ull<<20);
    ImageAllocated(3,8ull<<20,description);
    ImageOwner(3,103,12288,1024,64,14,1);
    {
        BindingScope scope{{123,4,4096,0xfffffff0,3ull<<30,false,false}};
        Buffer("pool_window_cross", {.address=4096,.size=3ull<<30,.physical=1ull<<30,
            .mapping_end=(3ull<<30)+4096,.window_start=0,.window_size=3584ull<<20});
        Buffer("copy_begin", {.address=4096,.size=3ull<<30,.tick=17});
        Buffer("staging_allocate_begin", {.size=3ull<<30,.allocation=3ull<<30,.created=true});
        Buffer("staging_selected", {.size=3ull<<30,.buffer=99,.allocation=3ull<<30,.tick=17,.created=true});
        Buffer("copy_complete", {.address=4096,.size=3ull<<30,.buffer=99,.allocation=3ull<<30,.elapsed_ns=77});
    }
    Collection({100,200,300,400,21,22,23,24,true,false});
    Keep(true); Keep(false);
    Keep(false, true);
    Failure(8,111,222);
    return 0;
}
