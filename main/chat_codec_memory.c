#include "chat_codec_memory.h"
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static TaskHandle_t s_owner;
static chat_codec_pool_t s_pool;
static void *s_reservation;

static bool owner(void)
{
    return s_owner && xTaskGetCurrentTaskHandle()==s_owner;
}
void chat_codec_memory_bind(void) { s_owner=xTaskGetCurrentTaskHandle(); }
bool chat_codec_memory_reserve(void)
{
    if (!owner()) return false;
    if (s_pool.data) return true;
    void *memory=malloc(CHAT_CODEC_POOL_BYTES);
    if (!memory) return false;
    chat_codec_pool_init(&s_pool,memory,CHAT_CODEC_POOL_BYTES);
    s_reservation=memory;
    return true;
}
bool chat_codec_memory_release(void)
{
    if (!owner() || s_pool.live) return false;
    if (s_pool.data) memset(s_pool.data,0,s_pool.capacity);
    free(s_reservation);s_reservation=NULL;
    chat_codec_pool_init(&s_pool,NULL,0);
    return true;
}
bool chat_codec_memory_contains(const void *ptr)
{
    return owner() && chat_codec_pool_owns(&s_pool,ptr);
}
const chat_codec_pool_t *chat_codec_memory_stats(void) { return &s_pool; }

void *media_lib_module_malloc(const char *module,size_t bytes)
{
    (void)module;
    return owner() ? chat_codec_pool_alloc(&s_pool,bytes) : malloc(bytes);
}
void *media_lib_module_calloc(const char *module,size_t count,size_t bytes)
{
    (void)module;
    return owner() ? chat_codec_pool_calloc(&s_pool,count,bytes) : calloc(count,bytes);
}
void *media_lib_module_realloc(const char *module,void *ptr,size_t bytes)
{
    (void)module;
    if (owner() && (!ptr || chat_codec_pool_owns(&s_pool,ptr)))
        return chat_codec_pool_realloc(&s_pool,ptr,bytes);
    return realloc(ptr,bytes);
}
void media_lib_free(void *ptr)
{
    if (chat_codec_memory_contains(ptr)) chat_codec_pool_free(&s_pool,ptr);
    else free(ptr);
}
