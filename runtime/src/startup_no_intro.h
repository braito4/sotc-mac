/* Original bounded adapter for the engine's existing +nointro command option.
 * Follows the command-line redirection method in the SotC reference loader.
 * No game offsets or instruction bytes are embedded here. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

static inline int sotc_startup_no_intro(unsigned char *image,size_t size,size_t site,
                                       unsigned char *text,size_t capacity) {
    static const char original[]="+VSyncWindow=10%";
    static const char command[]="+VSyncWindow=10% +nointro";
    if(!image||!text||site>size||size-site<7||capacity<sizeof(command))return -1;
    unsigned char *at=image+site;
    if(at[0]!=0x48||at[1]!=0x8d||at[2]!=0x35)return -1; /* lea rsi,[rip+disp32] */
    int32_t old;memcpy(&old,at+3,4);
    const int64_t target=(int64_t)site+7+old;
    if(target<0||(uint64_t)target>size||sizeof(original)>size-(size_t)target||
       memcmp(image+target,original,sizeof(original)))return -1;
    const uintptr_t next=(uintptr_t)(at+7),replacement=(uintptr_t)text;
    int64_t distance;
    if(replacement>=next){
        if(replacement-next>INT32_MAX)return -1;
        distance=(int64_t)(replacement-next);
    }else{
        if(next-replacement>(uint64_t)INT32_MAX+1)return -1;
        distance=-(int64_t)(next-replacement);
    }
    const int32_t displacement=(int32_t)distance;
    memcpy(text,command,sizeof(command));memcpy(at+3,&displacement,4);
    return 0;
}
