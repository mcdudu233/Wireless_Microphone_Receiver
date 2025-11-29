#include "ui/ui_bt.h"
#include <vector>
// static bool in_select_mode = false;

static void main_widget_cb(lv_event_t *e); // 完成按钮回调 -> 主窗口
static void link_cb(lv_event_t *e);        // 蓝牙连接按钮点击回调函数

void ui_bt_init()
{
    lv_obj_t *btn;
    lv_obj_t *label;
    bt_widget = add_win();

    g1 = lv_group_create(); // 外层group
    g2 = lv_group_create(); // 内层group list的内部按钮选项

    lv_obj_set_style_bg_color(bt_widget, lv_color_hex(0xEEF2F5), 0);

    bt_list = lv_list_create(bt_widget);
    lv_obj_set_style_pad_all(bt_list, 0, 0);
    lv_obj_set_style_radius(bt_list, 8, 0);
    lv_obj_set_style_border_width(bt_list, 0, 0);
    lv_obj_set_size(bt_list, 100, 60);
    lv_obj_align(bt_list, LV_ALIGN_TOP_LEFT, 5, 15);

    // 设置focus样式
    lv_obj_set_style_outline_width(bt_list, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(bt_list, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(bt_list, 1, LV_STATE_FOCUSED);

    lv_group_add_obj(g1, bt_list); // 将list加入外层group
    lv_obj_clear_flag(bt_list, LV_OBJ_FLAG_SCROLLABLE);

    // 示例蓝牙
    label = lv_label_create(bt_widget);
    lv_label_set_text(label, "选择设备");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(label, 0, 0);
    lv_obj_align_to(label, bt_list, LV_ALIGN_OUT_TOP_MID, 0, 0);

    // 连接按钮
    btn = add_button(bt_widget, "连接", 45, 25, &lv_font_harmonyos_14);
    lv_obj_set_style_radius(btn, 5, 0);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align_to(btn, bt_list, LV_ALIGN_OUT_RIGHT_TOP, 5, 0);
    lv_obj_add_event_cb(btn, link_cb, LV_EVENT_CLICKED, bt_list);
    lv_group_add_obj(g1, btn);
    lv_obj_t *last = btn;

    // 完成按钮
    btn = add_button(bt_widget, "完成", 45, 25, &lv_font_harmonyos_14);
    lv_obj_set_style_radius(btn, 5, 0);
    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align_to(btn, last, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_add_event_cb(btn, main_widget_cb, LV_EVENT_CLICKED, bt_widget);
    lv_group_add_obj(g1, btn);

    // 设置list可以被聚焦，以便enter进入其中选择
    lv_obj_add_flag(bt_list, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_group_focus_obj(bt_list);
    lv_obj_add_event_cb(bt_list, bt_list_click_event_cb, LV_EVENT_CLICKED, NULL); // click回调使内层group作用到编码器上
    lv_obj_add_event_cb(bt_list, bt_list_update_event_cb, EVENT_UPDATE, NULL);
    bind_group_to_all_encoders(g1);

    search_bt();
    // 滚动list移到首项
    // lv_obj_scroll_to_view(lv_obj_get_child(bt_list, 0), LV_ANIM_OFF);
}

// 蓝牙连接按钮点击回调函数
static void link_cb(lv_event_t *e)
{
    lv_obj_t *btn;
    lv_obj_t *label;
    lv_obj_t *list = (lv_obj_t *)lv_event_get_user_data(e);

    int32_t cnt = lv_obj_get_child_count_by_type(list, &lv_list_button_class);
    LV_LOG_USER("有%d个", cnt);
    for (int i = 0; i < cnt; i++)
    {
        btn = lv_obj_get_child_by_type(list, i, &lv_list_button_class);
        if (lv_color_eq(lv_obj_get_style_bg_color(btn, LV_PART_MAIN), COLOR_SELECTED)) // 选中
        {
            label = lv_obj_get_child(btn, 0);
            char *bt_name = lv_label_get_text(label); // 待链接蓝牙的名称
            LV_LOG_USER("正在连接蓝牙%s", bt_name);
            bool state = link_bt(bt_name);
            if (state) // 链接成功
            {
                lv_obj_set_style_bg_color(btn, COLOR_LINKED, LV_STATE_CHECKED); // 设置连接状态
                lv_obj_set_user_data(btn, BT_LINKED);
            }
        }
    }
    info_msgbox("awa", "coneettt");
}

// 完成按钮点击回调 进入主窗口
static void main_widget_cb(lv_event_t *e)
{
    ui_main_init();
    lv_obj_delete(bt_widget);
}

// 蓝牙列表点击事件回调
static void list_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_obj_t *list = lv_obj_get_parent(obj);
    if (code == LV_EVENT_CLICKED)
    {
        lv_state_t current_state = lv_obj_get_state(obj);
        LV_LOG_USER("state:%d", current_state);
        const char *bt_name = lv_list_get_button_text(list, obj);
        if (!lv_obj_has_state(obj, LV_STATE_CHECKED)) // 非选中状态说明之前为选中
        {
            // lv_color_t c = lv_obj_get_style_bg_color(obj, 0);
            // LV_LOG_USER("rgb:%d %d %d", c.red, c.green, c.blue);
            if (lv_obj_get_user_data(obj) == BT_LINKED) // 判断是否连接，如果是则断开连接
            {
                bool ret = unlink_bt(bt_name);
                LV_LOG_USER("断开连接");
                if (!ret)
                    lv_obj_add_state(obj, LV_STATE_CHECKED);
                else
                    lv_obj_set_style_bg_color(obj, COLOR_SELECTED, LV_STATE_CHECKED);
                lv_obj_set_user_data(obj, BT_UNLINKED);
                lv_obj_remove_state(obj, LV_STATE_CHECKED);
            }
        }
        lv_obj_remove_state(obj, (lv_state_t)(current_state & ~LV_STATE_CHECKED));
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        bind_group_to_all_encoders(g1); // 外层group
        lv_group_focus_obj(list);       // 重置焦点
        LV_LOG_USER("Clicked: %s", bt_name);
    }
}

static void bt_list_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *list = (lv_obj_t *)lv_event_get_target(e);
    LV_LOG_USER("list点击");
    bind_group_to_all_encoders(g2); // 进入内层
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    // 若列表尚无子项，避免空指针聚焦
    lv_obj_t *first = lv_obj_get_child(bt_list, 0);
    if (first)
        lv_group_focus_obj(first);
    else
        lv_group_focus_obj(list);
}

static void bt_list_update_event_cb(lv_event_t *e)
{
    lv_obj_t *btn;
    lv_obj_t *label;
    std::string bt_name;

    device_data *dev = (device_data *)lv_event_get_param(e);
    int32_t cnt = lv_obj_get_child_count_by_type(bt_list, &lv_list_button_class);
    bool is_exist = false;

    for (int j = 0; j < cnt; j++)
    {
        btn = lv_obj_get_child_by_type(bt_list, j, &lv_list_button_class);
        label = lv_obj_get_child(btn, 0);
        bt_name = lv_label_get_text(label); // 蓝牙mac
        if (bt_name == dev->name)                       // 链接成功
        {
            is_exist = true;
            break;
        }
    }
    if (!is_exist)
    {
        btn = add_list_obj(bt_list, dev->name, list_event_handler, NULL, COLOR_NONE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_group_add_obj(g2, btn);
    }
    if (dev->is_link)
    {
        lv_obj_set_style_bg_color(btn, COLOR_LINKED, LV_STATE_CHECKED); // 设置连接状态
        lv_obj_set_user_data(btn, BT_LINKED);
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        LV_LOG_USER("已连接:%s", dev->name.c_str());
    }else{
        lv_obj_set_style_bg_color(btn, COLOR_SELECTED, LV_STATE_CHECKED);
        lv_obj_set_user_data(btn, BT_UNLINKED);
        lv_obj_remove_state(btn, (lv_state_t)(LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY));
        LV_LOG_USER("已连接:%s", dev->name.c_str());

    }
}

// 添加蓝牙/更新蓝牙状态
void update_bt(device_data *dev)
{
    if(bt_list && lv_obj_is_valid(bt_list))
        lv_obj_send_event(bt_list, EVENT_UPDATE, dev);
}

// 连接蓝牙设备
bool link_bt(const char *device_name)
{
    return true; // 返回连接状态
}
// 断开蓝牙设备
bool unlink_bt(const char *device_name)
{
    return true; // 返回连接状态
}
static void search_bt()
{
    // 模拟搜索
    // 注意: 这些数据会被定时器回调异步访问，必须保证其生命周期
    // 使用 static 保证在进程生命周期内有效，避免悬垂指针导致未定义行为
    static device_data dev1 = {"mac1", false};
    static device_data dev2 = {"mac2", false};
    static device_data dev3 = {"mac3", false};
    lv_timer_create([](lv_timer_t *timer)
                    {
        device_data* dev = (device_data*)lv_timer_get_user_data(timer);
        update_bt(dev);
        lv_timer_delete(timer); }, 2000, &dev1); // 2000ms=2秒，无用户数据
    lv_timer_create([](lv_timer_t *timer)
                    {
        device_data* dev = (device_data*)lv_timer_get_user_data(timer);
        update_bt(dev);
        lv_timer_delete(timer); }, 4000, &dev2); // 2000ms=2秒，无用户数据
    lv_timer_create([](lv_timer_t *timer)
                    {
        device_data* dev = (device_data*)lv_timer_get_user_data(timer);
        update_bt(dev);
        lv_timer_delete(timer); }, 6000, &dev3); // 2000ms=2秒，无用户数据
}
