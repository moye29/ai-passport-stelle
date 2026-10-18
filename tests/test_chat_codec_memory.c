#include "chat_codec_memory.h"
#include "freertos/task.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int voice_task,other_task;
static TaskHandle_t current;
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current; }

int main(void)
{
    current=&voice_task;
    chat_codec_memory_bind();
    assert(chat_codec_memory_reserve());
    for (unsigned turn=0;turn<1000;++turn) {
        void *wrapper=media_lib_module_calloc("opus",1,64);
        unsigned char *codec=media_lib_module_malloc("opus",24512);
        assert(wrapper && codec);
        assert(!chat_codec_memory_release()); /* never free an in-use codec */
        current=&other_task;
        unsigned char *other=media_lib_module_calloc("other",1,1024);
        assert(other && !chat_codec_memory_contains(other));
        current=&voice_task;
        media_lib_free(codec);media_lib_free(wrapper);
        void *decoder=media_lib_module_malloc("opus",18400);
        assert(decoder && chat_codec_memory_contains(decoder));
        media_lib_free(decoder);
        current=&other_task;
        other=media_lib_module_realloc("other",other,2048);
        assert(other);
        media_lib_free(other);
        current=&voice_task;
    }
    assert(chat_codec_memory_release());
    assert(chat_codec_memory_reserve());
    assert(chat_codec_memory_release());
    puts("Chat codec memory: 1000 turns, task isolation, live-release guard and reentry: PASS");
}
