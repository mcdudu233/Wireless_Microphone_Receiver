#include "ui/ui.h"

// 创建一个基础控件容器
lv_obj_t *add_win()
{
    lv_obj_t *widget = lv_obj_create(lv_screen_active());
    lv_obj_set_size(widget, WIDGET_H, WIDGET_V);
    lv_obj_set_style_pad_all(widget, 0, 0); // 去除内边距
    return widget;
}
// 添加带标题按钮
lv_obj_t *add_button(lv_obj_t *parent, std::string title, int32_t w, int32_t h, const lv_font_t *font)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_size(btn, w, h);
    // lv_obj_set_size(label,w,h);
    lv_label_set_text(label, title.c_str());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    if (font != NULL)
    {
        lv_obj_set_style_text_font(label, font, 0);
    }
    return btn;
}

// 添加列表按钮
lv_obj_t *add_list_obj(lv_obj_t *list, std::string content, lv_event_cb_t cb, const lv_font_t *font, std::optional<lv_color_t> bg_color)
{
    lv_obj_t *btn = lv_list_add_button(list, NULL, content.c_str());
    // lv_obj_set_style_pad_all(btn, 1, 0);

    // lv_obj_set_size(btn,lv_pct(90),15);
    if (font != NULL)
    {
        lv_obj_set_style_text_font(btn, font, 0);
    }
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    if (bg_color.has_value())
    {
        lv_obj_set_style_bg_color(btn, bg_color.value(), 0);
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

void info_msgbox(std::string title, std::string content)
{
    lv_obj_t *btn;
    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_obj_set_size(mbox, WIDGET_H / 2, WIDGET_V / 2);
    lv_msgbox_add_title(mbox, title.c_str());
    lv_msgbox_add_text(mbox, content.c_str());

    btn = lv_msgbox_add_footer_button(mbox, "确定");

    lv_group_add_obj(lv_group_get_default(), btn);
    lv_obj_add_event_cb(btn, [](lv_event_t *e)
                        {
        lv_obj_t* mbox = (lv_obj_t*)lv_event_get_user_data(e);
        lv_msgbox_close(mbox); }, LV_EVENT_CLICKED, mbox);
}
