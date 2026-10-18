#include "chat_codec_pool.h"
#include <stdint.h>
#include <string.h>

typedef union { max_align_t alignment; size_t bytes; } allocation_t;

void chat_codec_pool_init(chat_codec_pool_t *p,void *memory,size_t bytes)
{
    const size_t alignment=_Alignof(max_align_t);
    size_t skip=(alignment-(uintptr_t)memory%alignment)%alignment;
    if (!memory || bytes<=skip) { *p=(chat_codec_pool_t){0};return; }
    *p=(chat_codec_pool_t){.data=(unsigned char *)memory+skip,.capacity=bytes-skip};
}
bool chat_codec_pool_owns(const chat_codec_pool_t *p,const void *ptr)
{
    uintptr_t address=(uintptr_t)ptr,base=(uintptr_t)p->data;
    return p->data && ptr && address>=base && address-base<p->capacity;
}
void *chat_codec_pool_alloc(chat_codec_pool_t *p,size_t bytes)
{
    const size_t alignment=_Alignof(max_align_t);
    if (!bytes) return NULL;
    if (bytes>SIZE_MAX-sizeof(allocation_t)-(alignment-1)) goto fail;
    size_t need=sizeof(allocation_t)+(bytes+alignment-1)/alignment*alignment;
    if (!p->data || need>p->capacity-p->used) goto fail;
    allocation_t *a=(allocation_t *)(p->data+p->used);
    a->bytes=bytes;p->used+=need;p->live++;
    if (p->used>p->peak) p->peak=p->used;
    return a+1;
fail:
    p->failed_request=bytes;
    return NULL;
}
void *chat_codec_pool_calloc(chat_codec_pool_t *p,size_t count,size_t bytes)
{
    if (bytes && count>SIZE_MAX/bytes) { p->failed_request=SIZE_MAX;return NULL; }
    void *ptr=chat_codec_pool_alloc(p,count*bytes);
    if (ptr) memset(ptr,0,count*bytes);
    return ptr;
}
void chat_codec_pool_free(chat_codec_pool_t *p,void *ptr)
{
    if (!chat_codec_pool_owns(p,ptr)) return;
    allocation_t *a=(allocation_t *)ptr-1;
    if (!a->bytes) return;
    a->bytes=0;
    if (--p->live==0) p->used=0;
}
void *chat_codec_pool_realloc(chat_codec_pool_t *p,void *ptr,size_t bytes)
{
    if (!ptr) return chat_codec_pool_alloc(p,bytes);
    if (!bytes) { chat_codec_pool_free(p,ptr);return NULL; }
    allocation_t *a=(allocation_t *)ptr-1;
    if (bytes<=a->bytes) { a->bytes=bytes;return ptr; }
    void *next=chat_codec_pool_alloc(p,bytes);
    if (next) { memcpy(next,ptr,a->bytes);chat_codec_pool_free(p,ptr); }
    return next;
}
