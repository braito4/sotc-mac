/* Optional SotC persistent saves, GPL-2.0-or-later.
 * ABI structures adapted from bloodborne-native/bloodborne-runtime
 * 994846bb7bb5 src/runtime_savedata.c. MountInfo and backup semantics informed
 * by AjncSystem/ShadowOfTheColossus-PC 75b48137bc33 src/runtime_savedata.c.
 * Foundations: bbport and shadPS4. No game data included.
 * One user/title; file writes commit through the pinned runtime_file backend.
 * All guest entry points serialize with file operations, including unmount.
 */
#include "bootstrap_savedata_trial.h"
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <time.h>
#define ERR_PARAMETER ((int32_t)0x809f0000)
#define ERR_NOT_INITIALIZED ((int32_t)0x809f0001)
#define ERR_BUSY ((int32_t)0x809f0003)
#define ERR_NOT_MOUNTED ((int32_t)0x809f0004)
#define ERR_EXISTS ((int32_t)0x809f0007)
#define ERR_NOT_FOUND ((int32_t)0x809f0008)
#define ERR_INTERNAL ((int32_t)0x809f000b)
#define ERR_MOUNT_FULL ((int32_t)0x809f000c)
#define ERR_BAD_MOUNTED ((int32_t)0x809f000d)
#define ERR_INVALID_USER ((int32_t)0x809f0011)
#define SLOTS 8
typedef struct { char data[10]; char pad[6]; } TitleId;
typedef struct { char data[32]; } DirName;
typedef struct { char data[16]; } MountPoint;
typedef struct {
    char title[128], subtitle[128], detail[1024];
    uint32_t user_param; int32_t pad;
    int64_t mtime;
    uint8_t reserved[32];
} Param;
typedef struct { int32_t user; int32_t pad; const TitleId *title; const DirName *dir; const void *fingerprint;
                 uint64_t blocks; uint32_t mode; uint8_t reserved[32]; } Mount1;
typedef struct { int32_t user; int32_t pad; const DirName *dir; uint64_t blocks; uint32_t mode;
                 uint8_t reserved[32]; int32_t pad2; } Mount2;
typedef struct { MountPoint point; uint64_t required_blocks; uint32_t unused, status; uint8_t reserved[28]; int32_t pad; } MountResult;
typedef struct { int32_t user; int32_t pad; const TitleId *title; const DirName *dir; uint32_t unused;
                 uint8_t reserved[32]; int32_t pad2; } Delete;
typedef struct { int32_t user; int32_t pad; const TitleId *title; const DirName *dir; uint32_t key, order;
                 uint8_t reserved[32]; } SearchCond;
typedef struct { uint64_t blocks, free_blocks; uint8_t reserved[32]; } SearchInfo;
typedef struct { uint32_t hits; int32_t pad; DirName *names; uint32_t names_capacity, set_count;
                 Param *params; SearchInfo *infos; uint8_t reserved[12]; int32_t pad2; } SearchResult;
typedef struct { const void *buffer; uint64_t buffer_size, data_size; uint8_t reserved[32]; } Icon;
_Static_assert(sizeof(Param)==1328,"OrbisSaveDataParam layout");
_Static_assert(sizeof(Mount1)==80 && sizeof(Mount2)==64,"OrbisSaveDataMount layouts");
_Static_assert(sizeof(MountResult)==64,"OrbisSaveDataMountResult layout");
_Static_assert(sizeof(SearchCond)==64 && sizeof(SearchResult)==56,"dir name search layouts");

static int initialized, root_lock=-1;
static char save_root[320], base[400];
static struct { int used, writable, fds; char host[512], meta[512]; } slots[SLOTS];
static struct { int slot_plus_one, writable; } descriptors[1024];
int sotc_savedata_trial_enabled(void) { const char *p=getenv("SOTC_SAVE_ROOT"); return p && *p; }
/* Reject redirects in the private save subtree. The chosen root is canonicalized
 * once; guest paths never select another title, user, or host directory. */
static int plain_path(const char *path) {
    char part[1024]; size_t n=strnlen(path,sizeof(part));
    if (!n || n==sizeof(part)) return 0;
    memcpy(part,path,n+1);
    for (size_t i=1;i<=n;++i) if (part[i]=='/' || !part[i]) {
        char c=part[i]; part[i]=0; struct stat st;
        int r=lstat(part,&st);
        if ((!r && S_ISLNK(st.st_mode)) || (r && errno!=ENOENT)) return 0;
        part[i]=c;
    }
    return 1;
}
static int valid_name(const char *p, size_t cap) {
    size_t n=strnlen(p,cap); if (!n || n==cap) return 0;
    for (size_t i=0;i<n;++i) if (!((p[i]>='a'&&p[i]<='z') || (p[i]>='A'&&p[i]<='Z') ||
        (p[i]>='0'&&p[i]<='9') || p[i]=='_' || p[i]=='-')) return 0;
    return 1;
}
static int make_dir(const char *p) {
    if (!plain_path(p)) return -1;
    if (mkdir(p,0700) && errno!=EEXIST) return -1;
    struct stat st; return stat(p,&st) || !S_ISDIR(st.st_mode) ? -1 : 0;
}
static int write_atomic(const char *path, const void *data, size_t size) {
    if (!plain_path(path)) return -1;
    char temp[600]; if (snprintf(temp,sizeof(temp),"%s.XXXXXX",path)>=(int)sizeof(temp)) return -1;
    int fd=mkstemp(temp); if (fd<0) return -1;
    size_t done=0; int error=0;
    while (done<size) { ssize_t n=write(fd,(const char *)data+done,size-done);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) { error=1; break; } done+=(size_t)n; }
    if (fsync(fd)) error=1;
    if (close(fd)) error=1;
    if (!error && rename(temp,path)) error=1;
    if (error) unlink(temp);
    /* Persist the directory entry as well as the file contents. */
    if (!error) { char parent[600]; snprintf(parent,sizeof(parent),"%s",path);
        char *slash=strrchr(parent,'/'); if (!slash) return -1; *slash=0;
        int d=open(parent,O_RDONLY|O_DIRECTORY); if (d<0) return -1;
        error=fsync(d)!=0; close(d); }
    return error ? -1 : 0;
}
static int read_param(const char *meta, Param *p) {
    char path[600]; snprintf(path,sizeof(path),"%s/param.bin",meta);
    if (!plain_path(path)) return -1;
    FILE *f=fopen(path,"rb"); if (!f) return -1;
    int ok=fread(p,1,sizeof(*p),f)==sizeof(*p); fclose(f); return ok ? 0 : -1;
}
static int write_param(const char *meta, const Param *p) {
    char path[600]; snprintf(path,sizeof(path),"%s/param.bin",meta);
    Param copy=*p; copy.mtime=time(NULL); return write_atomic(path,&copy,sizeof(copy));
}
static int read_quota(const char *meta,uint64_t *blocks) {
    char path[600]; snprintf(path,sizeof(path),"%s/blocks",meta);
    if (!plain_path(path)) return -1;
    FILE *f=fopen(path,"r"); if (!f) return -1;
    unsigned long long n=0; int ok=fscanf(f,"%llu",&n)==1 && n>=96 && n<=UINT64_MAX/32768;
    fclose(f); if (!ok) return -1; *blocks=n; return 0;
}
static int write_quota(const char *meta,uint64_t blocks) {
    char path[600],data[32]; snprintf(path,sizeof(path),"%s/blocks",meta);
    int n=snprintf(data,sizeof(data),"%llu\n",(unsigned long long)blocks);
    return write_atomic(path,data,(size_t)n);
}
static int32_t initialize(const void *unused) {
    (void)unused;
    if (initialized) return 0;
    const char *requested=getenv("SOTC_SAVE_ROOT"); char canonical[PATH_MAX];
    if (!requested || requested[0]!='/' || !realpath(requested,canonical) || strlen(canonical)>=sizeof(save_root)) return ERR_PARAMETER;
    snprintf(save_root,sizeof(save_root),"%s",canonical);
    if (make_dir(save_root)) return ERR_INTERNAL;
    char path[400]; snprintf(path,sizeof(path),"%s/session.lock",save_root);
    if (!plain_path(path)) return ERR_INTERNAL;
    root_lock=open(path,O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC,0600);
    if (root_lock<0) return ERR_INTERNAL;
    if (flock(root_lock,LOCK_EX|LOCK_NB)) { close(root_lock); root_lock=-1; return ERR_BUSY; }
    snprintf(base,sizeof(base),"%s/CUSA08809",save_root);
    if (make_dir(base)) { close(root_lock); root_lock=-1; return ERR_INTERNAL; }
    initialized=1;
    printf("Save trial: initialized persistent root %s (user 1, CUSA08809)\n",base);
    return 0;
}
static int title_ok(const TitleId *title) {
    return !title || (strnlen(title->data,10)==9 && !memcmp(title->data,"CUSA08809",9));
}
static int slot_of(const MountPoint *p) {
    if (!p || strnlen(p->data,sizeof(p->data))==sizeof(p->data)) return -1;
    for (int i=0;i<SLOTS;++i) { char point[16]; snprintf(point,sizeof(point),"/savedata%d",i);
        if (slots[i].used && !strcmp(p->data,point)) return i; }
    return -1;
}
static int path_slot(const char *p) {
    if (!p || strncmp(p,"/savedata",9)) return -1;
    for (int i=0;i<SLOTS;++i) { char point[16]; snprintf(point,sizeof(point),"/savedata%d",i);
        size_t n=strlen(point);
        if (slots[i].used && !strncmp(p,point,n) && (!p[n] || p[n]=='/')) return i; }
    return -1;
}
int sotc_save_path_check(const char *path, int writing) {
    int i=path_slot(path); if (i<0) return -ENOENT;
    if (writing && !slots[i].writable) return -EROFS;
    const char *tail=strchr(path+1,'/'); if (!tail) return writing ? -EISDIR : 0;
    if (strlen(tail)>240) return -ENAMETOOLONG;
    /* No traversal, ambiguous components or guest-visible backend temp names. */
    for (const char *s=tail+1; *s;) {
        const char *end=strchr(s,'/'); size_t n=end ? (size_t)(end-s) : strlen(s);
        if (!n || (n==1 && *s=='.') || (n==2 && !memcmp(s,"..",2)) ||
            memchr(s,'\\',n) || (n>=6 && !memcmp(s+n-6,".bbtmp",6))) return -EINVAL;
        if (!end) break; s=end+1;
    }
    char host[800]; snprintf(host,sizeof(host),"%s%s",slots[i].host,tail);
    return plain_path(host) ? 0 : -EACCES;
}
void sotc_save_fd_opened(int fd, const char *path, int flags) {
    int i=path_slot(path);
    if (fd>=3 && fd<1024 && i>=0) { descriptors[fd].slot_plus_one=i+1;
        descriptors[fd].writable=(flags&3)!=0; ++slots[i].fds; }
}
void sotc_save_fd_closed(int fd) {
    if (fd>=3 && fd<1024 && descriptors[fd].slot_plus_one) {
        --slots[descriptors[fd].slot_plus_one-1].fds;
        memset(&descriptors[fd],0,sizeof(descriptors[fd])); }
}
int sotc_save_fd_writable(int fd) {
    return fd>=3 && fd<1024 && descriptors[fd].slot_plus_one && descriptors[fd].writable;
}
static int32_t mount_save(int user,const TitleId *title,const DirName *dir,uint64_t blocks,unsigned mode,MountResult *out) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (user!=1) return ERR_INVALID_USER;
    if (blocks>UINT64_MAX/32768) return ERR_PARAMETER;
    if (!title_ok(title) || !dir || !out || !valid_name(dir->data,32)) return ERR_PARAMETER;
    /* Only documented read-only/read-write and create/create2 modes in this trial. */
    if ((mode&~(1u|2u|4u|32u)) || ((mode&3)!=1 && (mode&3)!=2) ||
        ((mode&1) && (mode&(4|32))) || ((mode&4) && (mode&32))) return ERR_PARAMETER;
    char host[512],meta[512];
    snprintf(host,sizeof(host),"%s/%s",base,dir->data);
    snprintf(meta,sizeof(meta),"%s/%s.sce_sys",base,dir->data);
    if (!plain_path(host) || !plain_path(meta)) return ERR_INTERNAL;
    struct stat st; int exists=!stat(host,&st);
    if (!exists && errno!=ENOENT) return ERR_INTERNAL;
    if (exists && !S_ISDIR(st.st_mode)) return ERR_INTERNAL;
    if (exists && (mode&4)) return ERR_EXISTS;
    if (!exists && !(mode&(4|32))) return ERR_NOT_FOUND;
    int slot=-1;
    for (int i=0;i<SLOTS;++i) {
        if (slots[i].used && !strcmp(slots[i].host,host)) return ERR_BAD_MOUNTED;
        if (!slots[i].used && slot<0) slot=i;
    }
    if (slot<0) return ERR_MOUNT_FULL;
    if (!exists) {
        if (make_dir(host)) return ERR_INTERNAL;
        if (make_dir(meta)) { rmdir(host); return ERR_INTERNAL; }
        Param p={0}; if (write_param(meta,&p)) { rmdir(meta); rmdir(host); return ERR_INTERNAL; }
    }
    uint64_t quota=0;
    if (exists && read_quota(meta,&quota)) return ERR_INTERNAL;
    if (!exists || ((mode&2) && blocks>quota)) {
        quota=blocks>96 ? blocks : 96;
        if (write_quota(meta,quota)) return ERR_INTERNAL;
    }
    Param check; if (read_param(meta,&check)) return ERR_INTERNAL;
    char point[16]; snprintf(point,sizeof(point),"/savedata%d",slot);
    if (runtime_file_mount(point,host)) return ERR_MOUNT_FULL;
    slots[slot].used=1; slots[slot].writable=(mode&2)!=0;
    snprintf(slots[slot].host,sizeof(slots[slot].host),"%s",host);
    snprintf(slots[slot].meta,sizeof(slots[slot].meta),"%s",meta);
    MountResult result={0}; memcpy(result.point.data,point,strlen(point)+1); result.status=!exists;
    *out=result;
    printf("Save trial: mounted %s at %s writable=%d created=%d\n",dir->data,point,slots[slot].writable,!exists);
    return 0;
}
static int32_t mount1(const Mount1 *p,MountResult *r) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!p) return ERR_PARAMETER;
    return mount_save(p->user,p->title,p->dir,p->blocks,p->mode,r);
}
static int32_t mount2(const Mount2 *p,MountResult *r) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!p) return ERR_PARAMETER;
    return mount_save(p->user,NULL,p->dir,p->blocks,p->mode,r);
}
static int32_t set_param(const MountPoint *point,unsigned type,const void *buffer,uint64_t size) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!buffer) return ERR_PARAMETER;
    int i=slot_of(point); if (i<0) return ERR_NOT_MOUNTED;
    if (!slots[i].writable) return ERR_BAD_MOUNTED;
    Param p; if (read_param(slots[i].meta,&p)) return ERR_INTERNAL;
    char *target=NULL; size_t capacity=0;
    switch (type) {
    case 0: if (size!=sizeof(p)) return ERR_PARAMETER; memcpy(&p,buffer,sizeof(p)); break;
    case 1: target=p.title; capacity=sizeof(p.title); break;
    case 2: target=p.subtitle; capacity=sizeof(p.subtitle); break;
    case 3: target=p.detail; capacity=sizeof(p.detail); break;
    case 4: if (size!=4) return ERR_PARAMETER; memcpy(&p.user_param,buffer,4); break;
    default: return ERR_PARAMETER;
    }
    if (target) { if (!size || size>capacity) return ERR_PARAMETER;
        memset(target,0,capacity); memcpy(target,buffer,size); target[capacity-1]=0; }
    return write_param(slots[i].meta,&p) ? ERR_INTERNAL : 0;
}
static int32_t save_icon(const MountPoint *point,const Icon *icon) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!icon || !icon->buffer || icon->data_size>icon->buffer_size || icon->data_size>16*1024*1024) return ERR_PARAMETER;
    int i=slot_of(point); if (i<0) return ERR_NOT_MOUNTED;
    if (!slots[i].writable) return ERR_BAD_MOUNTED;
    char path[600]; snprintf(path,sizeof(path),"%s/icon0.png",slots[i].meta);
    return write_atomic(path,icon->buffer,(size_t)icon->data_size) ? ERR_INTERNAL : 0;
}
/* SotC sums per-save quotas; reporting the entire disk for every slot makes
 * it refuse saves. Follow the reference port's persisted 32-KiB block count. */
static int used_bytes(const char *path,uint64_t *total,unsigned depth) {
    if (depth>8) return -1;
    DIR *d=opendir(path); if (!d) return -1;
    int result=0;
    for (;;) { errno=0; struct dirent *e=readdir(d); if (!e) { if (errno) result=-1; break; }
        if (!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
        size_t n=strlen(e->d_name); if (n>=6 && !strcmp(e->d_name+n-6,".bbtmp")) continue;
        char child[1024]; if (snprintf(child,sizeof(child),"%s/%s",path,e->d_name)>=(int)sizeof(child)) { result=-1; break; }
        struct stat st; if (lstat(child,&st)) { result=-1; break; }
        if (S_ISDIR(st.st_mode)) { if (used_bytes(child,total,depth+1)) { result=-1; break; } }
        else if (S_ISREG(st.st_mode) && st.st_size>=0 && (uint64_t)st.st_size<=UINT64_MAX-*total) *total+=(uint64_t)st.st_size;
        else { result=-1; break; }
    }
    closedir(d); return result;
}
static int space(const char *host,const char *meta,SearchInfo *out) {
    uint64_t blocks=0,total=0; if (read_quota(meta,&blocks) || used_bytes(host,&total,0)) return -1;
    uint64_t used=total/32768+(total%32768!=0);
    SearchInfo info={0}; info.blocks=blocks; info.free_blocks=blocks>used ? blocks-used : 0;
    struct statvfs st; if (statvfs(host,&st)) return -1;
    uint64_t available=(uint64_t)st.f_bavail*st.f_frsize/32768;
    if (info.free_blocks>available) info.free_blocks=available;
    *out=info; return 0;
}
static int32_t mount_info(const MountPoint *point,SearchInfo *out) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!out) return ERR_PARAMETER;
    int i=slot_of(point); if (i<0) return ERR_NOT_MOUNTED;
    return space(slots[i].host,slots[i].meta,out) ? ERR_INTERNAL : 0;
}
/* Linear wildcard matcher for PS4's SQL-LIKE '%' and '_' patterns. */
static int like(const char *text,const char *pattern) {
    const char *star=NULL,*retry=NULL;
    while (*text) {
        if (*pattern=='_' || *pattern==*text) { ++pattern; ++text; }
        else if (*pattern=='%') { star=pattern++; retry=text; }
        else if (star) { pattern=star+1; text=++retry; }
        else return 0;
    }
    while (*pattern=='%') ++pattern;
    return !*pattern;
}
typedef struct { DirName name; Param param; SearchInfo info; } Entry;
static unsigned sort_key,sort_order;
static int compare(const void *a,const void *b) {
    const Entry *x=a,*y=b; int c;
    if (sort_key==1) c=(x->param.user_param>y->param.user_param)-(x->param.user_param<y->param.user_param);
    else if (sort_key==3) c=(x->param.mtime>y->param.mtime)-(x->param.mtime<y->param.mtime);
    else c=strcmp(x->name.data,y->name.data);
    return sort_order ? -c : c;
}
static int32_t search(const SearchCond *cond,SearchResult *out) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!cond || !out || !title_ok(cond->title) || (cond->key!=0 && cond->key!=1 && cond->key!=3) ||
        cond->order>1 || (cond->dir && strnlen(cond->dir->data,32)==32) ||
        (out->names_capacity && !out->names)) return ERR_PARAMETER;
    if (cond->user!=1) return ERR_INVALID_USER;
    DIR *dir=opendir(base); if (!dir) return ERR_INTERNAL;
    Entry *entries=NULL; size_t count=0; int error=0;
    for (;;) { errno=0; struct dirent *e=readdir(dir); if (!e) { error=errno!=0; break; }
        if (!valid_name(e->d_name,32) || (cond->dir && cond->dir->data[0] && !like(e->d_name,cond->dir->data))) continue;
        char host[512],meta[512]; snprintf(host,sizeof(host),"%s/%s",base,e->d_name);
        snprintf(meta,sizeof(meta),"%s/%s.sce_sys",base,e->d_name);
        struct stat st; if (lstat(host,&st) || !S_ISDIR(st.st_mode)) continue;
        Entry entry={0}; snprintf(entry.name.data,32,"%s",e->d_name);
        if (read_param(meta,&entry.param) || space(host,meta,&entry.info) || count>=4096) { error=1; break; }
        Entry *next=realloc(entries,(count+1)*sizeof(*entries)); if (!next) { error=1; break; }
        entries=next; entries[count++]=entry;
    }
    closedir(dir);
    if (error) { free(entries); return ERR_INTERNAL; }
    sort_key=cond->key; sort_order=cond->order; if (count) qsort(entries,count,sizeof(*entries),compare);
    unsigned n=count<out->names_capacity ? (unsigned)count : out->names_capacity;
    for (unsigned i=0;i<n;++i) { out->names[i]=entries[i].name;
        if (out->params) out->params[i]=entries[i].param;
        if (out->infos) out->infos[i]=entries[i].info; }
    out->hits=(unsigned)count; out->set_count=n; free(entries);
    printf("Save trial: directory search hits=%u returned=%u\n",out->hits,n);
    return 0;
}
/* Immutable backup generations. Incomplete copies are never marked complete.
 * Copies run only after all guest descriptors on this mount have closed. */
static int copy_tree(const char *from,const char *to,unsigned depth) {
    if (depth>8 || !plain_path(from) || make_dir(to)) return -1;
    DIR *d=opendir(from); if (!d) return -1; int result=0;
    for (;;) { errno=0; struct dirent *e=readdir(d); if (!e) { if (errno) result=-1; break; }
        if (!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
        size_t n=strlen(e->d_name); if (n>=6 && !strcmp(e->d_name+n-6,".bbtmp")) continue;
        char a[1024],b[1024];
        if (snprintf(a,sizeof(a),"%s/%s",from,e->d_name)>=(int)sizeof(a) ||
            snprintf(b,sizeof(b),"%s/%s",to,e->d_name)>=(int)sizeof(b)) { result=-1; break; }
        struct stat st; if (lstat(a,&st)) { result=-1; break; }
        if (S_ISDIR(st.st_mode)) { if (copy_tree(a,b,depth+1)) { result=-1; break; } }
        else if (S_ISREG(st.st_mode)) {
            int input=open(a,O_RDONLY|O_NOFOLLOW),output=open(b,O_CREAT|O_EXCL|O_WRONLY,0600);
            if (input<0 || output<0) { if (input>=0) close(input); if (output>=0) close(output); result=-1; break; }
            char buffer[65536]; ssize_t got;
            while ((got=read(input,buffer,sizeof(buffer)))!=0) {
                if (got<0) { if (errno==EINTR) continue; result=-1; break; }
                ssize_t sent=0;
                while (sent<got) { ssize_t nwrite=write(output,buffer+sent,(size_t)(got-sent));
                    if (nwrite<0 && errno==EINTR) continue;
                    if (nwrite<=0) { result=-1; break; } sent+=nwrite; }
                if (result) break;
            }
            if (fsync(output)) result=-1;
            if (close(output)) result=-1;
            close(input); if (result) break;
        } else { result=-1; break; }
    }
    closedir(d); return result;
}
static int32_t unmount_save(const MountPoint *point,int backup) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    int i=slot_of(point); if (i<0) return ERR_NOT_MOUNTED;
    if (slots[i].fds) return ERR_BUSY;
    if (backup && slots[i].writable) {
        char folder[400],stage[480],path[600]; snprintf(folder,sizeof(folder),"%s/backups",save_root);
        if (make_dir(folder)) return ERR_INTERNAL;
        snprintf(stage,sizeof(stage),"%s/save-XXXXXX",folder);
        if (!mkdtemp(stage)) return ERR_INTERNAL;
        snprintf(path,sizeof(path),"%s/data",stage); if (copy_tree(slots[i].host,path,0)) return ERR_INTERNAL;
        snprintf(path,sizeof(path),"%s/meta",stage); if (copy_tree(slots[i].meta,path,0)) return ERR_INTERNAL;
        snprintf(path,sizeof(path),"%s/complete",stage);
        if (write_atomic(path,slots[i].host,strlen(slots[i].host))) return ERR_INTERNAL;
        printf("Save trial: completed backup %s\n",stage);
    }
    runtime_file_unmount(point->data); memset(&slots[i],0,sizeof(slots[i]));
    printf("Save trial: unmounted %.16s backup=%d\n",point->data,backup); return 0;
}
static int32_t unmount_plain(const MountPoint *p) { return unmount_save(p,0); }
static int32_t unmount_backup(const MountPoint *p) { return unmount_save(p,1); }
static int32_t delete_unsupported(const void *p) { (void)p; return initialized ? ERR_INTERNAL : ERR_NOT_INITIALIZED; }
/* File gate is shared with read/open/write/close. Internal helpers never relock. */
#define WRAP(name, params, args) \
 static int32_t wrapped_##name params { bootstrap_files_lock(); int32_t trial_result=name args; bootstrap_files_unlock(); \
 printf("Save trial: " #name " result=0x%08x\n",(unsigned)trial_result); return trial_result; }
WRAP(initialize,(const void *p),(p))
WRAP(mount1,(const Mount1 *p,MountResult *r),(p,r))
WRAP(mount2,(const Mount2 *p,MountResult *r),(p,r))
WRAP(search,(const SearchCond *p,SearchResult *r),(p,r))
WRAP(set_param,(const MountPoint *p,unsigned t,const void *b,uint64_t n),(p,t,b,n))
WRAP(save_icon,(const MountPoint *p,const Icon *i),(p,i))
WRAP(mount_info,(const MountPoint *p,SearchInfo *r),(p,r))
WRAP(unmount_plain,(const MountPoint *p),(p))
WRAP(unmount_backup,(const MountPoint *p),(p))
WRAP(delete_unsupported,(const void *p),(p))
#undef WRAP
uintptr_t sotc_savedata_trial_resolve(const char *name) {
    if (!sotc_savedata_trial_enabled()) return 0;
    static const struct { const char *nid; void *fn; } table[]={
        {"TywrFKCoLGY#u#d",wrapped_initialize}, {"32HQAQdwM2o#u#d",wrapped_mount1},
        {"0z45PIH+SNI#u#d",wrapped_mount2}, {"dyIhnXq-0SM#u#d",wrapped_search},
        {"85zul--eGXs#u#d",wrapped_set_param}, {"c88Yy54Mx0w#u#d",wrapped_save_icon},
        {"65VH0Qaaz6s#u#d",wrapped_mount_info}, {"BMR4F-Uek3E#u#d",wrapped_unmount_plain},
        {"VwadwBBBJ80#u#d",wrapped_unmount_backup}, {"S1GkePI17zQ#u#d",wrapped_delete_unsupported},
    };
    for (size_t i=0;i<sizeof(table)/sizeof(*table);++i) if (!strcmp(name,table[i].nid)) return (uintptr_t)table[i].fn;
    return 0;
}
