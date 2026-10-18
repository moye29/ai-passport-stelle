#include "anniv_image.h"
#include "anniv_pack.h"
#include "src/draw/lv_image_decoder_private.h"
#include <string.h>

typedef struct {
    lv_draw_buf_t draw;
    _Alignas(4) uint16_t row[ANNIV_FRAME_W];
    unsigned frame;
} anniv_decode_t;

static const uint8_t ids[ANNIV_FRAME_COUNT]={0,1,2,3,4,5,6,7};
#define FRAME(i) { .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RAW,.w=ANNIV_FRAME_W,.h=ANNIV_FRAME_H},.data_size=1,.data=&ids[i] }
static const lv_image_dsc_t images[ANNIV_FRAME_COUNT]={FRAME(0),FRAME(1),FRAME(2),FRAME(3),FRAME(4),FRAME(5),FRAME(6),FRAME(7)};
static lv_image_decoder_t *s_decoder;

static int frame_id(const lv_image_decoder_dsc_t *dsc)
{
    if (dsc->src_type!=LV_IMAGE_SRC_VARIABLE) return -1;
    for (unsigned i=0;i<ANNIV_FRAME_COUNT;i++) if (dsc->src==&images[i]) return (int)i;
    return -1;
}

static lv_result_t info(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc, lv_image_header_t *header)
{
    (void)decoder;
    if (frame_id(dsc)<0) return LV_RESULT_INVALID;
    memset(header,0,sizeof(*header));
    header->magic=LV_IMAGE_HEADER_MAGIC;header->cf=LV_COLOR_FORMAT_RGB565;
    header->w=ANNIV_FRAME_W;header->h=ANNIV_FRAME_H;header->stride=ANNIV_FRAME_W*2;
    return LV_RESULT_OK;
}

static lv_result_t open_image(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc)
{
    (void)decoder;
    int frame=frame_id(dsc);
    if (frame<0) return LV_RESULT_INVALID;
    anniv_decode_t *work=lv_malloc(sizeof(*work));
    if (!work) return LV_RESULT_INVALID;
    memset(work,0,sizeof(*work));work->frame=(unsigned)frame;
    dsc->user_data=work;dsc->decoded=NULL;dsc->args.no_cache=true;
    return LV_RESULT_OK;
}

static lv_result_t get_area(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc,
                            const lv_area_t *full, lv_area_t *decoded)
{
    (void)decoder;
    anniv_decode_t *work=dsc->user_data;
    int32_t y=decoded->y1==LV_COORD_MIN ? full->y1 : decoded->y2+1;
    if (!work || full->x1<0 || full->x2>=(int32_t)ANNIV_FRAME_W || full->x2<full->x1 ||
        y<0 || y>full->y2 || y>=(int32_t)ANNIV_FRAME_H) return LV_RESULT_INVALID;
    if (!anniv_art_decode_row(&anniv_art_pack,work->frame,(unsigned)y,work->row,ANNIV_FRAME_W)) return LV_RESULT_INVALID;
    unsigned width=(unsigned)(full->x2-full->x1+1);
    if (full->x1) memmove(work->row,work->row+full->x1,width*sizeof(work->row[0]));
    if (lv_draw_buf_init(&work->draw,width,1,LV_COLOR_FORMAT_RGB565,ANNIV_FRAME_W*2,
        work->row,sizeof(work->row))!=LV_RESULT_OK) return LV_RESULT_INVALID;
    *decoded=(lv_area_t){full->x1,y,full->x2,y};
    dsc->decoded=&work->draw;
    return LV_RESULT_OK;
}

static void close_image(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc)
{
    (void)decoder;
    lv_free(dsc->user_data);dsc->user_data=NULL;dsc->decoded=NULL;
}

const lv_image_dsc_t *anniv_image_frame(unsigned frame)
{
    if (frame>=ANNIV_FRAME_COUNT) return NULL;
    if (!s_decoder) {
        s_decoder=lv_image_decoder_create();
        if (!s_decoder) return NULL;
        lv_image_decoder_set_info_cb(s_decoder,info);
        lv_image_decoder_set_open_cb(s_decoder,open_image);
        lv_image_decoder_set_get_area_cb(s_decoder,get_area);
        lv_image_decoder_set_close_cb(s_decoder,close_image);
    }
    return &images[frame];
}
