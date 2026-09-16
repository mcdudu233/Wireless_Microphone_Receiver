#include "ui/ui.h"

lv_obj_t *pop_win;

typedef struct
{
    lv_group_t *g;
    lv_obj_t *obj;
    lv_group_t *popup_group;
    lv_group_t *button_group;
} back_val;

static void popup_delete_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    back_val *value = (back_val *)lv_obj_get_user_data(target);
    if (value)
    {
        if (value->button_group)
            lv_group_delete(value->button_group);
        if (value->popup_group)
            lv_group_delete(value->popup_group);
        lv_obj_set_user_data(target, nullptr);
        lv_free(value);
    }
    if (pop_win == target)
        pop_win = nullptr;
}

// 关闭当前弹窗(调用方需持有LVGL锁;供协议切换任务等非LVGL上下文使用)
void ui_close_popup()
{
    if (pop_win != nullptr && lv_obj_is_valid(pop_win))
    {
        // 必须复现弹窗"点击关闭"路径的分组恢复:先把编码器绑回创建弹窗时
        // 保存的分组并恢复焦点,再删除弹窗。若直接删除,弹窗自有分组被
        // lv_group_delete释放时会顺带把仍绑定它的输入设备置为无分组,
        // 编码器输入将彻底失效(界面无响应)。
        back_val *value = (back_val *)lv_obj_get_user_data(pop_win);
        if (value != nullptr)
        {
            if (value->g != nullptr)
            {
                lv_group_set_default(value->g);
                ui_bind_group_to_all_encoders(value->g);
            }
            if (value->obj != nullptr && lv_obj_is_valid(value->obj))
            {
                lv_group_focus_obj(value->obj);
            }
        }
        // popup_delete_cb 会清理弹窗自有分组并清空pop_win
        lv_obj_delete(pop_win);
    }
}
// 创建一个基础控件容器
lv_obj_t *ui_add_win()
{
    lv_obj_t *widget = lv_obj_create(lv_screen_active());
    lv_obj_set_size(widget, WIDGET_H, WIDGET_V);
    lv_obj_set_style_pad_all(widget, 0, 0); // 去除内边距
    lv_obj_set_style_border_width(widget, 0, 0);
    lv_obj_set_style_bg_color(widget, lv_color_hex(0xF4F5F7), 0);
    lv_obj_set_style_bg_opa(widget, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(widget, UI_FONT_BODY, 0);
    return widget;
}
// 添加带标题按钮
lv_obj_t *ui_add_button(lv_obj_t *parent, std::string title, int32_t w, int32_t h, const lv_font_t *font)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_size(btn, w, h);
    // Button default styling
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D6BDB), 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1D4ED8), LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn, lv_color_hex(0x2D6BDB), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn, 1, LV_STATE_FOCUSED);
    lv_label_set_text(label, title.c_str());
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    if (font != NULL)
    {
        lv_obj_set_style_text_font(label, font, 0);
    }
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    return btn;
}

// 添加列表按钮
lv_obj_t *ui_add_list_obj(lv_obj_t *list, std::string content, lv_event_cb_t cb, const lv_font_t *font, lv_palette_t bg_color)
{
    lv_obj_t *btn = lv_list_add_button(list, NULL, content.c_str());
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_height(btn, UI_LIST_ROW_HEIGHT);
    lv_obj_set_style_pad_hor(btn, 3, 0);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xDBEAFE), LV_STATE_FOCUSED);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS); // 焦点行自动滚动进视口

    // lv_obj_set_style_pad_all(btn, 1, 0);

    // lv_obj_set_size(btn,lv_pct(90),15);
    if (font != NULL)
    {
        lv_obj_set_style_text_font(btn, font, 0);
    }
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label != NULL)
    {
        const lv_font_t *label_font = font != NULL ? font : UI_FONT_BODY;
        lv_obj_set_height(label, lv_font_get_line_height(label_font));
        // 超长设备名循环滚动展示（恢复列表默认行为），不截断
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    }
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    if (bg_color != LV_PALETTE_NONE)
    {
        lv_obj_set_style_bg_color(btn, lv_palette_main(bg_color), 0);
    }
    return btn;
}

// 将指定分组绑定到所有编码器输入设备
void ui_bind_group_to_all_encoders(lv_group_t *g)
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
void ui_set_opa(void *obj, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, val, 0);
}
void ui_set_bar_val(void *bar, int32_t val)
{
    lv_bar_set_value((lv_obj_t *)bar, val, LV_ANIM_ON);
}

lv_obj_t **ui_popwin(bool has_bg, lv_group_t *g, lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(lv_screen_active());
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_flag(cont, LV_OBJ_FLAG_CLICK_FOCUSABLE, true);
    lv_obj_set_flag(cont, LV_OBJ_FLAG_SCROLLABLE, false);

    if (lv_obj_is_valid(pop_win))
    {
        lv_obj_del(pop_win);
        pop_win = nullptr;
    }
    pop_win = cont;

    lv_obj_t *win = lv_obj_create(cont);
    lv_obj_set_style_pad_all(win, 0, 0);
    lv_obj_set_size(win, lv_pct(60), lv_pct(67));
    lv_obj_align(win, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flag(win, LV_OBJ_FLAG_SCROLLABLE, false);
    if (!has_bg)
    {
        lv_obj_set_style_bg_opa(win, 0, 0);
        lv_obj_set_style_border_width(win, 0, 0);
    }

    back_val *v = (back_val *)lv_malloc(sizeof(back_val));
    if (g == nullptr)
    {
        g = lv_group_get_default();
    }
    v->g = g;
    v->obj = obj;
    v->popup_group = nullptr;
    v->button_group = nullptr;
    lv_obj_set_user_data(cont, v);
    lv_obj_add_event_cb(cont, popup_delete_cb, LV_EVENT_DELETE, NULL);

    lv_group_t *new_g = lv_group_create();
    v->popup_group = new_g;
    lv_group_add_obj(new_g, cont);
    // lv_group_set_default(new_g);
    ui_bind_group_to_all_encoders(new_g);
    lv_group_focus_obj(cont);

    lv_obj_add_event_cb(cont, [](lv_event_t *e)
                        {
        lv_obj_t* target = lv_event_get_target_obj(e);
        back_val* v = (back_val *)lv_obj_get_user_data(target);
        lv_group_set_default(v->g);
        ui_bind_group_to_all_encoders(v->g);
        if(lv_obj_is_valid(v->obj)) lv_group_focus_obj(v->obj);
        if (lv_obj_is_valid(target)) lv_obj_del(target);
        }, LV_EVENT_CLICKED, NULL);
    static lv_obj_t *ret[2];
    ret[0] = cont;
    ret[1] = win;

    return ret;
}

lv_obj_t *ui_popwin_msgbox(const char *text, lv_group_t *g, lv_obj_t *obj, const void *icon, const char *title, bool is_from_svg, const char *btn1_title, lv_event_cb_t event_cb1, void *user_data1, const char *btn2_title, lv_event_cb_t event_cb2, void *user_data2)
{
    lv_obj_t **ret = ui_popwin(true, g, obj);
    lv_obj_t *label;

    lv_obj_set_size(ret[1], 120, 60);
    lv_obj_set_style_bg_color(ret[1], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ret[1], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ret[1], 8, 0);
    lv_obj_set_style_shadow_width(ret[1], 15, 0);
    lv_obj_set_style_shadow_opa(ret[1], LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(ret[1], 4, 0);
    // 图标
    if (icon)
    {
        lv_obj_t *img = lv_image_create(ret[1]);
        lv_image_set_src(img, icon);
        if (is_from_svg)
        {
            lv_img_set_zoom(img, 64);
            lv_obj_set_size(img, 16, 16);
        }
        lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    // 标题
    label = lv_label_create(ret[1]);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_harmonyos_12, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, icon ? 21 : 3, 1);

    static lv_style_t style_line;
    static bool style_line_initialized = false;
    if (!style_line_initialized)
    {
        lv_style_init(&style_line);
        style_line_initialized = true;
    }
    lv_style_set_line_width(&style_line, 1);
    lv_style_set_line_color(&style_line, lv_color_hex(0xE5E7EB));
    lv_style_set_line_rounded(&style_line, true);
    static lv_point_precise_t line_points[] = {{3, 0}, {117, 0}};

    lv_obj_t *line;

    line = lv_line_create(ret[1]);
    lv_line_set_points(line, line_points, 2);
    lv_obj_add_style(line, &style_line, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, 17);

    // content
    label = lv_label_create(ret[1]);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 3);
    lv_label_set_text(label, text);
    lv_obj_set_size(label, lv_pct(90), 30);
    lv_obj_set_style_text_font(label, &lv_font_harmonyos_12, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x6B7280), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    if (btn1_title || btn2_title)
    {
        lv_obj_set_size(ret[1], 120, 75);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -2);

        lv_group_t *g = lv_group_create();
        back_val *popup_data = (back_val *)lv_obj_get_user_data(ret[0]);
        if (popup_data)
            popup_data->button_group = g;
        ui_bind_group_to_all_encoders(g);

        lv_obj_t *btn;
        lv_obj_t *btn1;
        lv_obj_t *btn2;
        if (btn1_title)
        {
            btn = ui_add_button(ret[1], btn1_title, 30, 18, UI_FONT_BODY);
            lv_obj_set_style_radius(btn, 4, 0);
            lv_obj_set_style_pad_all(btn, 2, 0);
            lv_group_add_obj(g, btn);
            lv_group_focus_obj(btn);
            lv_obj_add_event_cb(btn, [](lv_event_t *e)
                                {
                                    lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
                                    if (target && lv_obj_is_valid(target))
                                        lv_obj_send_event(target, LV_EVENT_CLICKED, NULL); }, LV_EVENT_CLICKED, ret[0]);
            lv_obj_add_event_cb(btn, event_cb1, LV_EVENT_PRESSED, user_data1);
            btn1 = btn;
        }

        if (btn2_title)
        {
            btn = ui_add_button(ret[1], btn2_title, 30, 18, UI_FONT_BODY);
            lv_obj_set_style_radius(btn, 4, 0);
            lv_obj_set_style_pad_all(btn, 2, 0);
            lv_group_add_obj(g, btn);

            lv_obj_add_event_cb(btn, [](lv_event_t *e)
                                {
                                lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
                                if (target && lv_obj_is_valid(target))
                                    lv_obj_send_event(target, LV_EVENT_CLICKED, NULL); }, LV_EVENT_CLICKED, ret[0]);
            lv_obj_add_event_cb(btn, event_cb2, LV_EVENT_PRESSED, user_data2);
            btn2 = btn;
        }
        if (btn1_title and btn2_title)
        {
            lv_obj_align(btn1, LV_ALIGN_BOTTOM_MID, -22, -2);
            lv_obj_align(btn2, LV_ALIGN_BOTTOM_MID, 22, -2);
        }
        else
            lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -2);
    }

    return ret[0];
}
