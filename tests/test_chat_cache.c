#include "chat_cache.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    chat_cache_t c={0};assert(!chat_cache_reusable(&c,0,true,true,true));
    chat_cache_touch(&c,1000);assert(chat_cache_reusable(&c,1001,true,true,true));
    assert(chat_cache_reusable(&c,120999,true,true,true));
    assert(!chat_cache_reusable(&c,121000,true,true,true));
    assert(!chat_cache_reusable(&c,1001,false,true,true));
    assert(!chat_cache_reusable(&c,1001,true,false,true));
    assert(!chat_cache_reusable(&c,1001,true,true,false));
    chat_cache_touch(&c,UINT32_MAX-99);assert(chat_cache_reusable(&c,100,true,true,true));
    assert(!chat_cache_reusable(&c,120000,true,true,true));
    chat_cache_clear(&c);assert(!chat_cache_reusable(&c,100,true,true,true));
    assert(chat_cache_accept_session(true,"new","new"));
    assert(chat_cache_accept_session(true,"new",""));
    assert(!chat_cache_accept_session(true,"new","old"));
    assert(!chat_cache_accept_session(false,"new","new"));
    puts("Chat warm transport: reuse, idle expiry, leave/screen-off, disconnect, clock wrap: PASS");
}
