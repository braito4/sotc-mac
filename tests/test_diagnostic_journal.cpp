// Original cross-failure process-exit and isolated-channel controls; no game/GPU.
#include "diagnostic_event_journal.h"
#include "gpu_shader_provenance.h"
#include <cassert>
#include <csignal>
#include <thread>
#include <vector>

int main(int argc,char** argv) {
    assert(argc==2);const char* mode=argv[1];
    SotcEventJournal cpu{};sotc_event_journal_init(&cpu,"cpu-events.bin",3,8);
    if(!strcmp(mode,"disabled")) {assert(!cpu.map);return 0;}
    if(!strcmp(mode,"refuse")) {assert(!cpu.map);return 0;}
    assert(cpu.map);
    const uint64_t data[SOTC_JOURNAL_DATA_WORDS]={0x40,0x1234,0x1240,0x4567,7,1};
    sotc_event_journal_record(&cpu,0,1,data);
    for(unsigned i=0;i<100;++i)sotc_event_journal_record(&cpu,1,100+i,data);
    if(!strcmp(mode,"threads")) {
        std::vector<std::thread> threads;
        for(unsigned i=0;i<8;++i)threads.emplace_back([&] {
            for(unsigned j=0;j<1000;++j)sotc_event_journal_record(&cpu,2,1000+j,data);
        });
        for(auto& thread:threads)thread.join();
    }
    if(!strcmp(mode,"interrupted")) {
        // A writer that reserved but never committed must not be reported.
        __atomic_store_n(cpu.map+16+8,1,__ATOMIC_RELAXED);
        __atomic_fetch_add(cpu.map+5,1,__ATOMIC_RELAXED);
        uint64_t* row=cpu.map+SOTC_JOURNAL_HEADER_WORDS+2*8*16;
        row[1]=1;row[2]=1;row[3]=102;row[5]=0xdead;
    }
    if(!strcmp(mode,"delayed")) {
        // Delayed ticket 1 cannot overwrite a newer lap already in slot zero.
        uint64_t* row=cpu.map+SOTC_JOURNAL_HEADER_WORDS+2*8*16;
        row[2]=17;
        sotc_event_journal_record(&cpu,2,3,data);
        assert(row[2]==17);
    }
    auto& shader=SotcShaderProvenance::Get();
    shader.Record({.program=0x80,.address=0x400,.bank=0x50,.packet=0x60,.first=7,.words=2,.kind=1,.actor=1});
    for(unsigned i=0;i<620;++i)
        shader.Record({.program=0x80,.address=0x400,.hash=i,.kind=2,.head0=i,.head1=2,.found=1,.length=32,.required=1});
    // Unchanged lookup suppression must not hide a changed code head.
    shader.Record({.program=0x80,.address=0x400,.hash=619,.kind=2,.head0=619,.head1=2,.found=1,.length=32,.required=1});
    if(!strcmp(mode,"signal")) {signal(SIGSEGV,SIG_DFL);raise(SIGSEGV);}
    if(!strcmp(mode,"shader"))_Exit(23);
    if(!strcmp(mode,"fiber"))_Exit(23);
    if(!strcmp(mode,"watchdog"))_Exit(21);
    _Exit(0); // Deliberately no destructor, stdio flush or journal close hook.
}
