#include "chat_cache.h"
#include <string.h>
void chat_cache_touch(chat_cache_t *c,uint32_t now) { c->valid=true;c->touched_ms=now; }
void chat_cache_clear(chat_cache_t *c) { c->valid=false; }
bool chat_cache_reusable(const chat_cache_t *c,uint32_t now,bool foreground,bool network_up,bool mqtt_up)
{ return c->valid && foreground && network_up && mqtt_up && (uint32_t)(now-c->touched_ms)<CHAT_CACHE_IDLE_MS; }
bool chat_cache_accept_session(bool open,const char *current,const char *message)
{ return open && (!*message || strcmp(current,message)==0); }
