#pragma once
#include "chat_codec_pool.h"

/* Call only from the Chat worker. Reserve before networking; release only
   after both Opus handles have closed. Other tasks retain normal heap use. */
void chat_codec_memory_bind(void);
bool chat_codec_memory_reserve(void);
bool chat_codec_memory_release(void);
bool chat_codec_memory_contains(const void *ptr);
const chat_codec_pool_t *chat_codec_memory_stats(void);

/* Weak allocator ABI in the pinned esp_audio_codec 2.5.0 library.
   The module name is informational; standard libc allocators are untouched. */
void *media_lib_module_malloc(const char *module,size_t bytes);
void *media_lib_module_calloc(const char *module,size_t count,size_t bytes);
void *media_lib_module_realloc(const char *module,void *ptr,size_t bytes);
void media_lib_free(void *ptr);
