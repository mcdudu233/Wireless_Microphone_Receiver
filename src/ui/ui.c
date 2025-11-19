#include "ui/ui.h"
//#include <stdio.h>
// 根据设置缩放倍率调节位置
void obj_set_pos(lv_obj_t * obj, int32_t x, int32_t y)
{
    lv_obj_set_pos(obj,x*SCALE,y*SCALE);
}

// 根据设置缩放倍率调节大小
void obj_set_size(lv_obj_t * obj, int32_t w, int32_t h)
{
    lv_obj_set_size(obj, w*SCALE, h*SCALE);
}

// 根据缩放倍率 生成grid布局器的dsc数组 row 和 col
void grid_dsc_array(int32_t* dsc_array, int32_t* val, int32_t len)
{
    for(int i = 0; i < len; i++) {
        dsc_array[i] = val[i] * SCALE;
        //printf("dsc:%d,val:%d", dsc_array[i], val[i]);
    }
    dsc_array[len] = LV_GRID_TEMPLATE_LAST;
}
