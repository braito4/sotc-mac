/* Synthetic fixture: uses the real save adapter and upstream file backend. */
#include "bootstrap_contracts.h"
#include "runtime.h"
#include "bootstrap_savedata_trial.c"
#include <assert.h>

static int guest_errno;
int *runtime_errno(void) { return &guest_errno; }
void runtime_memory_note_write(uintptr_t p,uint64_t n) { (void)p;(void)n; }
static const struct { const char *nid,*symbol; } names[]={
#include "import_names.inc"
};
const char *runtime_symbol(const char *name) {
    for (size_t i=0;i<sizeof(names)/sizeof(*names);++i) if (!strcmp(name,names[i].nid)) return names[i].symbol;
    return name; /* Tests may request a symbol directly. */
}
uintptr_t runtime_lookup(const RuntimeExport *table,size_t n,const char *nid) {
    const char *name=runtime_symbol(nid);
    for (size_t i=0;i<n;++i) if (!strcmp(name,table[i].name)) return (uintptr_t)table[i].function;
    return 0;
}
#define FILE_FN(type,name,symbol) type name=(void *)bootstrap_files_resolve(symbol); assert(name)
typedef int64_t (*Open)(const char *,int,int);
typedef int64_t (*Close)(int);
typedef int64_t (*Read)(int,void *,uint64_t);
typedef int64_t (*Write)(int,const void *,uint64_t);
typedef int64_t (*Mkdir)(const char *,int);
typedef int64_t (*Truncate)(int,int64_t);
typedef int64_t (*Pwrite)(int,const void *,uint64_t,int64_t);
int main(int argc,char **argv) {
    assert(argc==3);
    assert(!bootstrap_mount_game_root(argv[2]));
    if (!strcmp(argv[1],"disabled")) {
        unsetenv("SOTC_SAVE_ROOT");
        assert(!sotc_savedata_trial_resolve("TywrFKCoLGY#u#d"));
        assert(!bootstrap_files_resolve("sceKernelWrite"));
        int32_t (*init)(const void *)=(void *)bootstrap_savedata_resolve("TywrFKCoLGY#u#d");
        assert(init && init(NULL)==ERR_INTERNAL); puts("PASS disabled"); return 0;
    }
    assert(wrapped_mount2((void *)1,(void *)1)==ERR_NOT_INITIALIZED);
    int32_t code=wrapped_initialize(NULL);
    if (!strcmp(argv[1],"locked")) { assert(code==ERR_BUSY); puts("PASS locked"); return 0; }
    assert(!code);
    if (!strcmp(argv[1],"hold")) { puts("READY"); fflush(stdout); getchar(); return 0; }
    DirName name={"SOTC_TEST"}; Mount2 request={.user=1,.dir=&name,.mode=2|32,.blocks=1024};
    MountResult result;
    if (!strcmp(argv[1],"read")) request.mode=1;
    assert(!wrapped_mount2(&request,&result));
    FILE_FN(Open,open_file,"sceKernelOpen"); FILE_FN(Close,close_file,"sceKernelClose");
    FILE_FN(Read,read_file,"sceKernelRead"); FILE_FN(Write,write_file,"sceKernelWrite");
    FILE_FN(Pwrite,pwrite_file,"sceKernelPwrite");
    FILE_FN(Mkdir,mkdir_file,"sceKernelMkdir"); FILE_FN(Truncate,truncate_file,"sceKernelFtruncate");
    assert(open_file("/app0/forbidden",0x202,0600)<0);
    assert(open_file("/savedata0/../escape",0x202,0600)<0);
    assert(open_file("/savedata00/escape",0x202,0600)<0);
    char evil[600]; snprintf(evil,sizeof(evil),"%s/evil",slots[0].host);
    assert(!symlink(argv[2],evil));
    assert(open_file("/savedata0/evil",0x202,0600)<0);
    assert(!unlink(evil));
    assert(wrapped_unmount_plain(&(MountPoint){"/savedata0junk"})==ERR_NOT_MOUNTED);
    SearchInfo info; assert(!wrapped_mount_info(&result.point,&info)); assert(info.blocks==1024 && info.free_blocks<=1024);
    const char *file="/savedata0/progress.dat";
    if (!strcmp(argv[1],"read")) {
        assert(open_file(file,2,0)<0);
        assert(mkdir_file("/savedata0/forbidden",0700)<0);
        assert(wrapped_set_param(&result.point,1,"bad",4)==ERR_BAD_MOUNTED);
        int fd=(int)open_file(file,0,0); assert(fd>=3); char data[9]={0};
        assert(read_file(fd,data,8)==8);
        assert(!memcmp(data,"CHECK001",8));
        assert(write_file(fd,"bad",3)<0);
        assert(truncate_file(fd,0)<0);
        assert(!close_file(fd));
    } else {
        int fd=(int)open_file(file,2|0x200|0x400,0600); assert(fd>=3);
        assert(wrapped_unmount_backup(&result.point)==ERR_BUSY);
        const char *data=!strcmp(argv[1],"interrupt") ? "BROKEN!!" : "CHECK000";
        assert(write_file(fd,data,8)==8);
        if (!strcmp(argv[1],"interrupt")) { puts("PASS simulated interruption before close"); _exit(0); }
        assert(!truncate_file(fd,8));
        assert(pwrite_file(fd,"1",1,7)==1);
        assert(pwrite_file(fd,"x",1,-1)<0);
        assert(!close_file(fd));
        assert(!mkdir_file("/savedata0/subdir",0700));
        int empty=(int)open_file("/savedata0/subdir/empty.dat",2|0x200|0x400,0600);
        assert(empty>=3 && !close_file(empty));
        assert(!wrapped_set_param(&result.point,1,"Synthetic checkpoint",21));
        char icon[4]={1,2,3,4}; Icon in={.buffer=icon,.buffer_size=4,.data_size=5};
        assert(wrapped_save_icon(&result.point,&in)==ERR_PARAMETER); in.data_size=4;
        assert(!wrapped_save_icon(&result.point,&in));
    }
    assert(!wrapped_unmount_backup(&result.point));
    DirName found[4]; Param params[4]; SearchInfo infos[4];
    SearchCond condition={.user=1,.key=0};
    SearchResult out={.names=found,.names_capacity=4,.params=params,.infos=infos};
    assert(!wrapped_search(&condition,&out)); assert(out.hits==1 && out.set_count==1);
    assert(!strcmp(found[0].data,"SOTC_TEST"));
    assert(!strcmp(params[0].title,"Synthetic checkpoint"));
    assert(wrapped_delete_unsupported(NULL)==ERR_INTERNAL);
    puts("PASS savedata persistence contracts"); return 0;
}
/* Host allocation substitutes only; no guest low-address ABI is exercised here. */
#undef malloc
#undef calloc
#undef realloc
#undef free
void *bb_low_malloc(size_t n) { return malloc(n); }
void *bb_low_calloc(size_t n,size_t s) { return calloc(n,s); }
void *bb_low_realloc(void *p,size_t n) { return realloc(p,n); }
void bb_low_free(void *p) { free(p); }
