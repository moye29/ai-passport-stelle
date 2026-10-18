#include "anniv_image.h"
#include "anniv_art.h"
#include "anniv_pack.h"
#include "src/draw/lv_image_decoder_private.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    lv_init();
    assert(anniv_image_frame(8)==NULL);
    lv_image_decoder_dsc_t a,b;
    assert(lv_image_decoder_open(&a,anniv_image_frame(0),NULL)==LV_RESULT_OK);
    assert(lv_image_decoder_open(&b,anniv_image_frame(7),NULL)==LV_RESULT_OK);
    lv_area_t full={17,9,103,11};
    lv_area_t aa={LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN},bb=aa;
    uint16_t expected[216],saved[87];
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_OK);
    assert(aa.x1==17 && aa.x2==103 && aa.y1==9 && aa.y2==9);
    assert(anniv_art_decode_row(&anniv_art_pack,0,9,expected,216));
    assert(!memcmp(a.decoded->data,expected+17,sizeof(saved)));
    memcpy(saved,a.decoded->data,sizeof(saved));
    assert(lv_image_decoder_get_area(&b,&full,&bb)==LV_RESULT_OK);
    assert(anniv_art_decode_row(&anniv_art_pack,7,9,expected,216));
    assert(!memcmp(b.decoded->data,expected+17,sizeof(saved)));
    assert(!memcmp(a.decoded->data,saved,sizeof(saved))); // independent open sessions
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_OK && aa.y1==10);
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_OK && aa.y1==11);
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_INVALID);
    lv_image_decoder_close(&a);lv_image_decoder_close(&b);
    puts("Anniversary LVGL decoder: clipped rows, concurrent sessions, close: PASS");
}
