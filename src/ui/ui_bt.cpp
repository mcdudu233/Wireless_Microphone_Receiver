#include "ui/ui_bt.h"
#include "module/screen.h"
#include "config.h"
#include <utility>
#include <vector>
// static bool in_select_mode = false;
static lv_obj_t *bt_list;
static lv_obj_t *bt_widget;
static lv_obj_t *bt_title;
static lv_group_t *g1; // list 俩个按钮
static lv_group_t *g2; // list内部设备选项
static std::vector<std::pair<lv_obj_t *, std::string>> bt_rows;

static void link_bt();
static void bt_list_click_event_cb(lv_event_t *e); // bt_list 被点击
static void list_event_handler(lv_event_t *e);     // bt_list 项被点击
static void button_finish_cb(lv_event_t *e);       // 完成按钮回调 -> 主窗口
static void button_setting_cb(lv_event_t *e);      // 设置按钮回调
static uint8_t ui_list_get_select_num();
static uint8_t ui_list_get_link_num();

static std::string get_bt_mac(lv_obj_t *button)
{
    for (const auto &row : bt_rows)
    {
        if (row.first == button)
            return row.second;
    }
    return "";
}

static void delete_bt_groups()
{
    bt_rows.clear();
    if (g2)
    {
        lv_group_delete(g2);
        g2 = nullptr;
    }
    if (g1)
    {
        lv_group_delete(g1);
        g1 = nullptr;
    }
}

void ui_bt_init()
{
    lv_obj_t *btn;
    lv_obj_t *label;
    bt_widget = ui_add_win();
    bt_rows.clear();

    lv_obj_set_user_data(bt_widget, (void *)"bt_list");

    g1 = lv_group_create(); // 外层group
    g2 = lv_group_create(); // 内层group list的内部按钮选项
    lv_group_set_default(g1);
    lv_obj_add_event_cb(bt_widget, [](lv_event_t *e)
                        {
                            lv_group_t *g = (lv_group_t *)lv_event_get_user_data(e);
                            ui_bind_group_to_all_encoders(g);
                            // logger::debugln("从设置回到蓝牙窗口");
                        },
                        (lv_event_code_t)(LV_EVENT_LAST + 1), g1);

    lv_obj_set_style_bg_color(bt_widget, lv_color_hex(0xF4F5F7), 0);

    bt_list = lv_list_create(bt_widget);
    lv_obj_set_style_pad_all(bt_list, 0, 0);
    lv_obj_set_style_border_width(bt_list, 1, 0);
    lv_obj_set_style_border_color(bt_list, lv_color_hex(0xE5E7EB), 0);
    lv_obj_set_style_bg_color(bt_list, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(bt_list, 6, 0);
    lv_obj_set_style_width(bt_list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(bt_list, lv_color_hex(0x2D6BDB), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(bt_list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_scrollbar_mode(bt_list, LV_SCROLLBAR_MODE_OFF);
    // 视口高度为整行数(3行x18px)，滚动吸附行首，避免行上下被截半
    lv_obj_set_scroll_snap_y(bt_list, LV_SCROLL_SNAP_START);
    lv_obj_set_size(bt_list, 100, UI_LIST_ROW_HEIGHT * 3);
    lv_obj_align(bt_list, LV_ALIGN_TOP_LEFT, 5, 21);

    // 设置focus样式（accent-primary，与设置页焦点色一致）
    lv_obj_set_style_outline_width(bt_list, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(bt_list, lv_color_hex(0x2D6BDB), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(bt_list, 1, LV_STATE_FOCUSED);

    lv_group_add_obj(g1, bt_list); // 将list加入外层group
    lv_obj_clear_flag(bt_list, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(bt_widget);
    bt_title = label;
    lv_label_set_text(label, "选择设备");
    lv_obj_set_style_text_font(label, UI_FONT_HEADING, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(label, 0, 0);
    lv_obj_align_to(label, bt_list, LV_ALIGN_OUT_TOP_MID, 0, -3); // 与列表保持3px间距

    // 设置按钮
    btn = ui_add_button(bt_widget, "设置", 45, UI_ACTION_HEIGHT, UI_FONT_HEADING);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D6BDB), 0);
    lv_obj_align_to(btn, bt_list, LV_ALIGN_OUT_RIGHT_TOP, 5, -3);
    lv_obj_add_event_cb(btn, button_setting_cb, LV_EVENT_CLICKED, bt_list);
    lv_group_add_obj(g1, btn);
    lv_obj_t *last = btn;

    // 完成按钮
    btn = ui_add_button(bt_widget, "完成", 45, UI_ACTION_HEIGHT, UI_FONT_HEADING);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x059669), 0);
    lv_obj_align_to(btn, last, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    lv_obj_add_event_cb(btn, button_finish_cb, LV_EVENT_CLICKED, bt_widget);
    lv_group_add_obj(g1, btn);

    // 设置list可以被聚焦，以便enter进入其中选择
    lv_obj_add_flag(bt_list, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_group_focus_obj(bt_list);
    lv_obj_add_event_cb(bt_list, bt_list_click_event_cb, LV_EVENT_CLICKED, NULL); // click回调使内层group作用到编码器上
    ui_bind_group_to_all_encoders(g1);

    // 已连接设备不会重新广播,先注入设备管理器中的已知设备,再开始扫描新设备
    ui_bt_seed_devices();
    ui_bt_search();
}

// 连接蓝牙函数
static void link_bt()
{
    lv_obj_t *btn;
    uint8_t n = ui_list_get_select_num();
    uint8_t link_n = ui_list_get_link_num();
    if (n == 0)
    {
        ui_popwin_msgbox("请先选择要连接的设备", g1, bt_list);
        return;
    }

    else if (link_n >= MAX_DEVICE_COUNT)
    {

        ui_popwin_msgbox("连接数量超过最大", g1, bt_list);
        return;
    }

    ui_popwin_msgbox("正在连接蓝牙...", g1, bt_list);
    int32_t cnt = lv_obj_get_child_count_by_type(bt_list, &lv_list_button_class);
    for (int i = 0; i < cnt; i++)
    {
        btn = lv_obj_get_child_by_type(bt_list, i, &lv_list_button_class);
        if (lv_obj_has_state(btn, LV_STATE_CHECKED) && lv_obj_get_user_data(btn) != BT_LINKED)
        {
            std::string bt_name = get_bt_mac(btn);
            // logger::infoln("正在连接蓝牙%s", bt_name);
            bool state = ui_bt_link(bt_name);
            if (state) // 链接成功
            {
                lv_obj_set_style_bg_color(btn, COLOR_LINKED, LV_STATE_CHECKED); // 设置连接状态
                lv_obj_set_user_data(btn, BT_LINKED);
                ui_popwin_msgbox("连接成功!", g1, bt_list);
            }
        }
    }
}
static void button_setting_cb(lv_event_t *e)
{
    ui_setting_init(bt_widget);
}

// 完成按钮点击回调 进入主窗口
static void button_finish_cb(lv_event_t *e)
{
    uint8_t n = ui_list_get_link_num();
    if (n < 1)
    {
        ui_popwin_msgbox("请先连接设备", g1, bt_list);
        return;
    }
    if (config::config.rf.mode == RF_MODE_WIFI)
    {
        // WiFi模式:先显示连接提示,BLE->WiFi迁移由协议切换任务完成后自动进入主界面
        ui_popwin_msgbox("正在连接...", g1, bt_list);
        ui_bt_pause_search();
    }
    else
    {
        // BLE模式:下发音频启动命令后直接进入主界面
        ui_bt_pause_search();
        ui_bt_finish_to_main();
    }
}

// 完成设备连接并切换到主界面(调用方需持有LVGL锁;供完成按钮与协议切换任务共用)
void ui_bt_finish_to_main()
{
    ui_main_init();
    if (bt_widget != nullptr && lv_obj_is_valid(bt_widget))
        lv_obj_delete(bt_widget);
    // 置空控件引用,避免悬空指针被ui_bt_update访问
    bt_widget = nullptr;
    bt_list = nullptr;
    bt_title = nullptr;
    delete_bt_groups();
}

// 蓝牙列表点击事件回调
static void list_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_state_t current_state = lv_obj_get_state(obj);
        // logger::infoln("state:%d", current_state);
        const std::string bt_name = get_bt_mac(obj);
        ui_bind_group_to_all_encoders(g1);            // 外层group
        lv_group_focus_obj(bt_list);                  // 重置焦点
        if (!lv_obj_has_state(obj, LV_STATE_CHECKED)) // 非选中状态说明之前为选中
        {
            // lv_color_t c = lv_obj_get_style_bg_color(obj, 0);
            // logger::infoln("rgb:%d %d %d", c.red, c.green, c.blue);
            if (lv_obj_get_user_data(obj) == BT_LINKED) // 判断是否连接，如果是则断开连接
            {
                bool ret = ui_bt_unlink(bt_name);
                // logger::infoln("断开连接");
                if (!ret)
                {
                    lv_obj_add_state(obj, LV_STATE_CHECKED);
                    ui_popwin_msgbox("断开失败", g1, bt_list);
                }
                else
                {
                    lv_obj_set_style_bg_color(obj, COLOR_SELECTED, LV_STATE_CHECKED);
                    ui_popwin_msgbox("已断开", g1, bt_list);
                    lv_obj_set_user_data(obj, BT_UNLINKED);
                    lv_obj_remove_state(obj, LV_STATE_CHECKED);
                }
            }
        }
        else // 之前为未选中
        {
            lv_obj_remove_state(obj, (lv_state_t)(current_state & ~LV_STATE_CHECKED));
            ui_bind_group_to_all_encoders(g1); // 外层group
            lv_group_focus_obj(bt_list);       // 重置焦点
            link_bt();
        }
        lv_obj_remove_state(obj, (lv_state_t)(current_state & ~LV_STATE_CHECKED));
        // 保持列表可滚动并让选中行完整可见，避免选中行被视口裁切只显示一半
        lv_obj_scroll_to_view(obj, LV_ANIM_OFF);
        // logger::infoln("Clicked: %s", bt_name);
    }
}

static void bt_list_click_event_cb(lv_event_t *e)
{
    lv_obj_t *list = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *first = lv_obj_get_child(bt_list, 0);
    if (first)
    {
        ui_bind_group_to_all_encoders(g2); // 进入内层
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_state(list, (lv_state_t)(LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY));
        lv_group_focus_obj(first);
    }
    else
    {
        ui_bind_group_to_all_encoders(g1);
        lv_group_focus_obj(list);
    }
}

// 添加蓝牙/更新蓝牙状态
void ui_bt_update(const std::string &mac, bool is_link)
{
    LV_LOCK();
    lv_obj_t *btn;

    // 设备连接页未打开时(主界面运行/重连期间)不更新列表,避免引用已销毁控件
    if (bt_list == nullptr || !lv_obj_is_valid(bt_list) || g2 == nullptr)
    {
        LV_UNLOCK();
        return;
    }

    int32_t cnt = lv_obj_get_child_count_by_type(bt_list, &lv_list_button_class);
    bool is_exist = false;

    for (int j = 0; j < cnt; j++)
    {
        btn = lv_obj_get_child_by_type(bt_list, j, &lv_list_button_class);
        if (get_bt_mac(btn) == mac)
        {
            is_exist = true;
            break;
        }
    }
    if (is_exist && !is_link)
    {
        // 行已存在且仍未连接:不重复刷新,避免扫描回调反复清除用户选中/焦点状态
        LV_UNLOCK();
        return;
    }
    if (!is_exist)
    {
        // 显示完整名称，超长文本由列表标签滚动展示
        btn = ui_add_list_obj(bt_list, mac, list_event_handler, UI_FONT_BODY, COLOR_NONE);
        bt_rows.emplace_back(btn, mac);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_group_add_obj(g2, btn);
    }
    if (is_link)
    {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xECFDF5), LV_STATE_CHECKED); // 浅绿背景
        lv_obj_set_user_data(btn, BT_LINKED);
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        lv_obj_t *link_label = lv_obj_get_child(btn, 0);
        if (link_label)
        {
            lv_obj_set_style_text_color(link_label, lv_color_hex(0x059669), 0);
        }
        // logger::infoln("设置蓝牙:%s 已连接状态", mac.c_str());
    }
    else
    {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFBEB), LV_STATE_CHECKED); // 浅黄背景
        lv_obj_set_user_data(btn, BT_UNLINKED);
        lv_obj_remove_state(btn, (lv_state_t)(LV_STATE_CHECKED | LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY));
        lv_obj_t *unlink_label = lv_obj_get_child(btn, 0);
        if (unlink_label)
        {
            lv_obj_set_style_text_color(unlink_label, lv_color_hex(0x1F2937), 0);
        }
        // logger::infoln("设置蓝牙:%s 未连接状态", mac.c_str());
    }
    const int32_t row_count = lv_obj_get_child_count_by_type(bt_list, &lv_list_button_class);
    lv_obj_set_scrollbar_mode(bt_list, row_count > 3 ? LV_SCROLLBAR_MODE_ON : LV_SCROLLBAR_MODE_OFF);
    lv_label_set_text_fmt(bt_title, "选择设备 %d", row_count);
    LV_UNLOCK();
}

static uint8_t ui_list_get_select_num()
{
    uint8_t n = 0;
    int32_t cnt = lv_obj_get_child_count_by_type(bt_list, &lv_list_button_class);
    for (int i = 0; i < cnt; i++)
    {
        lv_obj_t *btn = lv_obj_get_child_by_type(bt_list, i, &lv_list_button_class);
        if (lv_obj_has_state(btn, LV_STATE_CHECKED) && lv_obj_get_user_data(btn) != BT_LINKED)
            n++;
    }
    return n;
}
static uint8_t ui_list_get_link_num()
{
    uint8_t n = 0;
    int32_t cnt = lv_obj_get_child_count_by_type(bt_list, &lv_list_button_class);
    for (int i = 0; i < cnt; i++)
    {
        lv_obj_t *btn = lv_obj_get_child_by_type(bt_list, i, &lv_list_button_class);
        if (lv_obj_get_user_data(btn) == BT_LINKED) // 选中
            n++;
    }
    return n;
}
