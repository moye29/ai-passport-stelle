#include "enemy_b_image.h"
#include "enemy_b_pack.h"
#include "src/draw/lv_image_decoder_private.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    lv_init();assert(enemy_b_image(2)==NULL);
    lv_image_decoder_dsc_t a,b;
    assert(lv_image_decoder_open(&a,enemy_b_image(0),NULL)==LV_RESULT_OK);
    assert(lv_image_decoder_open(&b,enemy_b_image(1),NULL)==LV_RESULT_OK);
    lv_area_t full={101,80,200,82},aa={LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN,LV_COORD_MIN},bb=aa;
    uint16_t c[216];uint8_t alpha[216],saved[648];
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_OK && aa.y1==80);
    assert(enemy_b_decode_row(&enemy_b_art_pack,0,80,c,alpha,216));
    assert(!memcmp(a.decoded->data,c+101,200));
    assert(!memcmp(a.decoded->data+432,alpha+101,100));
    memcpy(saved,a.decoded->data,648);
    assert(lv_image_decoder_get_area(&b,&full,&bb)==LV_RESULT_OK);
    assert(!memcmp(a.decoded->data,saved,648));
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_OK && aa.y1==81);
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_OK && aa.y1==82);
    assert(lv_image_decoder_get_area(&a,&full,&aa)==LV_RESULT_INVALID);
    lv_image_decoder_close(&a);lv_image_decoder_close(&b);
    puts("Enemy B LVGL: clipped color/alpha rows, separate sessions and close: PASS");
}
