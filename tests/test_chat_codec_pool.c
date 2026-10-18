#include "chat_codec_pool.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    _Alignas(max_align_t) unsigned char memory[CHAT_CODEC_POOL_BYTES];
    chat_codec_pool_t pool;
    chat_codec_pool_init(&pool,memory,sizeof(memory));
    /* The encoder and decoder must reuse one reservation for every turn,
       even when other system-heap allocations remain alive between turns. */
    for (unsigned turn=0; turn<1000; ++turn) {
        unsigned char *wrapper=chat_codec_pool_alloc(&pool,64);
        unsigned char *encoder=chat_codec_pool_alloc(&pool,24512);
        assert(wrapper && encoder);
        assert((uintptr_t)encoder % _Alignof(max_align_t)==0);
        memset(encoder,0xa5,24512);
        chat_codec_pool_free(&pool,encoder);
        chat_codec_pool_free(&pool,wrapper);
        assert(pool.used==0 && pool.live==0);
        wrapper=chat_codec_pool_alloc(&pool,64);
        unsigned char *decoder=chat_codec_pool_alloc(&pool,18304);
        assert(wrapper && decoder);
        chat_codec_pool_free(&pool,wrapper);
        chat_codec_pool_free(&pool,decoder);
        assert(pool.used==0 && pool.live==0);
    }
    unsigned char *a=chat_codec_pool_calloc(&pool,8,32);
    assert(a);
    for (unsigned i=0;i<256;++i) assert(a[i]==0);
    memset(a,0x3c,256);
    assert(!chat_codec_pool_alloc(&pool,CHAT_CODEC_POOL_BYTES));
    assert(!chat_codec_pool_calloc(&pool,SIZE_MAX,2));
    assert(!chat_codec_pool_alloc(&pool,SIZE_MAX));
    assert(!chat_codec_pool_realloc(&pool,a,CHAT_CODEC_POOL_BYTES));
    for (unsigned i=0;i<256;++i) assert(a[i]==0x3c);
    unsigned char *grown=chat_codec_pool_realloc(&pool,a,512);
    assert(grown);
    for (unsigned i=0;i<256;++i) assert(grown[i]==0x3c);
    assert(chat_codec_pool_realloc(&pool,grown,32)==grown);
    assert(!chat_codec_pool_realloc(&pool,grown,0));
    assert(pool.used==0 && pool.live==0);
    assert(!chat_codec_pool_alloc(&pool,0));
    chat_codec_pool_free(&pool,NULL);
    assert(!chat_codec_pool_owns(&pool,memory+sizeof(memory)));
    chat_codec_pool_init(&pool,memory+1,sizeof(memory)-1);
    void *unaligned=chat_codec_pool_alloc(&pool,64);
    assert(unaligned && (uintptr_t)unaligned % _Alignof(max_align_t)==0);
    chat_codec_pool_free(&pool,unaligned);
    chat_codec_pool_init(&pool,NULL,0);
    assert(!chat_codec_pool_alloc(&pool,1));
    puts("Chat codec pool: 1000 switches, alignment, exhaustion, overflow, realloc and cleanup: PASS");
    return 0;
}
