#include "ui/ui.h"
#include <stdio.h>
// 根据设置缩放倍率调节位置
void obj_set_pos(lv_obj_t *obj, int32_t x, int32_t y)
{
    lv_obj_set_pos(obj, x * SCALE, y * SCALE);
}

// 根据设置缩放倍率调节大小
void obj_set_size(lv_obj_t *obj, int32_t w, int32_t h)
{
    lv_obj_set_size(obj, w * SCALE, h * SCALE);
}

// 根据缩放倍率 生成grid布局器的dsc数组 row 和 col
void grid_dsc_array(int32_t *dsc_array, int32_t *val, int32_t len)
{
    for (int i = 0; i < len; i++)
    {
        dsc_array[i] = val[i] * SCALE;
        printf("dsc:%d,val:%d", dsc_array[i], val[i]);
    }
    dsc_array[len] = LV_GRID_TEMPLATE_LAST;
}

// 创建一个基础控件容器
lv_obj_t *add_win()
{
    lv_obj_t *widget = lv_obj_create(lv_screen_active());
    obj_set_size(widget, WIDGET_H, WIDGET_V);
    lv_obj_set_style_pad_all(widget, 0, 0); // 去除内边距
    return widget;
}
// 添加带标题按钮
lv_obj_t *add_button(lv_obj_t *parent, char *title, int32_t w, int32_t h, const lv_font_t *font)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_size(btn, w, h);
    // lv_obj_set_size(label,w,h);
    lv_label_set_text(label, title);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    if (font != NULL)
    {
        lv_obj_set_style_text_font(label, font, 0);
    }
    return btn;
}

// 添加列表按钮,bg_color = -1则无颜色
lv_obj_t *add_list_obj(lv_obj_t *list, char *content, lv_event_cb_t cb, const lv_font_t *font, lv_palette_t bg_color)
{
    lv_obj_t *btn = lv_list_add_button(list, NULL, content);
    // lv_obj_set_style_pad_all(btn, 1, 0);

    // lv_obj_set_size(btn,lv_pct(90),15);
    if (font != NULL)
    {
        lv_obj_set_style_text_font(btn, font, 0);
    }
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    if (bg_color != none)
    {
        lv_obj_set_style_bg_color(btn, lv_palette_main(bg_color), 0);
    }
    return btn;
}

// 将指定分组绑定到所有编码器输入设备
void bind_group_to_all_encoders(lv_group_t *g)
{
    lv_indev_t *indev = NULL;
    while ((indev = lv_indev_get_next(indev)) != NULL)
    {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER)
        {
            lv_indev_set_group(indev, g);
        }
    }
}
