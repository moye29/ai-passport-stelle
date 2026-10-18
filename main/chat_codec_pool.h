#pragma once
#include <stdbool.h>
#include <stddef.h>

#define CHAT_CODEC_POOL_BYTES (25U * 1024U)

/* One task owns this reservation. Opus 2.5.0 allocates at open, frees at close.
   Reset only after every allocation has been freed, including failed opens. */
typedef struct {
    unsigned char *data;
    size_t capacity, used, live, peak, failed_request;
} chat_codec_pool_t;

void chat_codec_pool_init(chat_codec_pool_t *pool,void *memory,size_t bytes);
bool chat_codec_pool_owns(const chat_codec_pool_t *pool,const void *ptr);
void *chat_codec_pool_alloc(chat_codec_pool_t *pool,size_t bytes);
void *chat_codec_pool_calloc(chat_codec_pool_t *pool,size_t count,size_t bytes);
void *chat_codec_pool_realloc(chat_codec_pool_t *pool,void *ptr,size_t bytes);
void chat_codec_pool_free(chat_codec_pool_t *pool,void *ptr);
