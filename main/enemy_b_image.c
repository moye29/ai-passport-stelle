#include "enemy_b_image.h"
#include "enemy_b_pack.h"
#include "src/draw/lv_image_decoder_private.h"
#include <string.h>

typedef union {
    struct {uint16_t color[ENEMY_B_W];uint8_t alpha[ENEMY_B_W];} plane;
    uint8_t bytes[ENEMY_B_W*3];
} row_t;
_Static_assert(sizeof(row_t)==ENEMY_B_W*3,"RGB565A8 row planes must be adjacent");
typedef struct {lv_draw_buf_t draw;_Alignas(4) row_t row;unsigned pose;} work_t;
static const uint8_t ids[2]={0,1};
#define POSE(i) {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=ENEMY_B_W,.h=ENEMY_B_H},.data_size=1,.data=&ids[i]}
static const lv_image_dsc_t poses[2]={POSE(0),POSE(1)};
static lv_image_decoder_t *s_decoder;
static int pose_id(const lv_image_decoder_dsc_t *dsc)
{
    if(dsc->src_type!=LV_IMAGE_SRC_VARIABLE)return -1;
    for(unsigned i=0;i<2;i++)if(dsc->src==&poses[i])return (int)i;
    return -1;
}
static lv_result_t info(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,lv_image_header_t *h)
{
    (void)decoder;if(pose_id(dsc)<0)return LV_RESULT_INVALID;
    memset(h,0,sizeof(*h));h->magic=LV_IMAGE_HEADER_MAGIC;h->cf=LV_COLOR_FORMAT_RGB565A8;
    h->w=ENEMY_B_W;h->h=ENEMY_B_H;h->stride=ENEMY_B_W*2;
    return LV_RESULT_OK;
}
static lv_result_t open_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc)
{
    (void)decoder;int pose=pose_id(dsc);if(pose<0)return LV_RESULT_INVALID;
    work_t *w=lv_malloc(sizeof(*w));if(!w)return LV_RESULT_INVALID;
    memset(w,0,sizeof(*w));w->pose=(unsigned)pose;
    dsc->user_data=w;dsc->decoded=NULL;dsc->args.no_cache=true;
    return LV_RESULT_OK;
}
static lv_result_t get_area(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc,const lv_area_t *full,lv_area_t *area)
{
    (void)decoder;work_t *w=dsc->user_data;
    int32_t y=area->y1==LV_COORD_MIN ? full->y1 : area->y2+1;
    if(!w || full->x1<0 || full->x2>=(int32_t)ENEMY_B_W || full->x2<full->x1 ||
       y<0 || y>full->y2 || y>=(int32_t)ENEMY_B_H)return LV_RESULT_INVALID;
    if(!enemy_b_decode_row(&enemy_b_art_pack,w->pose,(unsigned)y,w->row.plane.color,w->row.plane.alpha,ENEMY_B_W))return LV_RESULT_INVALID;
    unsigned width=(unsigned)(full->x2-full->x1+1);
    if(full->x1){
        memmove(w->row.plane.color,w->row.plane.color+full->x1,width*2);
        memmove(w->row.plane.alpha,w->row.plane.alpha+full->x1,width);
    }
    if(lv_draw_buf_init(&w->draw,width,1,LV_COLOR_FORMAT_RGB565A8,ENEMY_B_W*2,w->row.bytes,sizeof(w->row.bytes))!=LV_RESULT_OK)return LV_RESULT_INVALID;
    *area=(lv_area_t){full->x1,y,full->x2,y};dsc->decoded=&w->draw;
    return LV_RESULT_OK;
}
static void close_image(lv_image_decoder_t *decoder,lv_image_decoder_dsc_t *dsc)
{
    (void)decoder;lv_free(dsc->user_data);dsc->user_data=NULL;dsc->decoded=NULL;
}
const lv_image_dsc_t *enemy_b_image(unsigned pose)
{
    if(pose>=2)return NULL;
    if(!s_decoder){
        s_decoder=lv_image_decoder_create();if(!s_decoder)return NULL;
        lv_image_decoder_set_info_cb(s_decoder,info);lv_image_decoder_set_open_cb(s_decoder,open_image);
        lv_image_decoder_set_get_area_cb(s_decoder,get_area);lv_image_decoder_set_close_cb(s_decoder,close_image);
    }
    return &poses[pose];
}
