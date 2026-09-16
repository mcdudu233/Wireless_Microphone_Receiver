#include "ui/ui_bt.h"
#include "module/screen.h"
#include "config.h"
#include <cstdio>
#include <utility>
#include <vector>

// 页面几何:顶部标题 + 圆角全屏卡片(见DESIGN.md 设备连接页)
// 卡片56px(2px边框+54px视口)=3整行×18px,行高与密集列表一致
#define BT_ROW_HEIGHT 18
#define BT_VISIBLE_ROWS 3
#define BT_TITLE_OFFSET_Y 2  // 标题距屏顶
#define BT_CARD_X 3
#define BT_CARD_Y 21
#define BT_CARD_W 154
#define BT_CARD_H 56

static lv_obj_t *bt_list;        // 圆角卡片列表(末行为"完成"模拟按钮)
static lv_obj_t *bt_widget;
static lv_obj_t *bt_empty_hint;  // 无设备时的占位提示(首个设备出现时移除)
static lv_obj_t *bt_finish_row;  // 列表末行"完成"(模拟按钮,点击进入主界面)
static lv_group_t *g1;           // 页面唯一分组:完成行+设备行
static std::vector<std::pair<lv_obj_t *, std::string>> bt_rows; // 设备行(不含完成行)

// 设备信息弹窗当前目标(单例弹窗,按钮回调经此取MAC)
static std::string bt_popup_mac;

static void list_event_handler(lv_event_t *e);     // 设备行被点击 -> 信息弹窗
static void button_finish_cb(lv_event_t *e);       // 完成行回调 -> 主窗口
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
    if (g1)
    {
        lv_group_delete(g1);
        g1 = nullptr;
    }
}

// 连接状态 -> 行user_data(BT_LINKED/BT_LINKING/BT_UNLINKED)
static void *bt_state_to_user_data(BtLinkState state)
{
    switch (state)
    {
    case BT_LINK_STATE_LINKED:
        return BT_LINKED;
    case BT_LINK_STATE_LINKING:
        return BT_LINKING;
    default:
        return BT_UNLINKED;
    }
}

// 连接状态 -> 状态文本(行右侧着色展示)
static const char *bt_state_to_text(BtLinkState state)
{
    switch (state)
    {
    case BT_LINK_STATE_LINKED:
        return "(已连接)";
    case BT_LINK_STATE_LINKING:
        return "(连接中)";
    default:
        return "(未连接)";
    }
}

static lv_color_t bt_state_to_color(BtLinkState state)
{
    switch (state)
    {
    case BT_LINK_STATE_LINKED:
        return lv_color_hex(0x059669); // status-success
    case BT_LINK_STATE_LINKING:
        return lv_color_hex(0x2D6BDB); // accent-primary(进行中)
    default:
        return lv_palette_main(LV_PALETTE_RED); // 语义化错误/离线状态
    }
}

// 创建设备行:左侧"设备N"(持久编号),右侧连接状态文本
static lv_obj_t *create_device_row(const std::string &mac)
{
    const uint8_t number = ui_info_get_number(mac);
    char name[16];
    snprintf(name, sizeof(name), "设备%u", number);

    lv_obj_t *btn = lv_list_add_button(bt_list, NULL, name);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_height(btn, BT_ROW_HEIGHT);
    lv_obj_set_style_pad_hor(btn, 4, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xDBEAFE), LV_STATE_FOCUSED); // accent-surface
    lv_obj_add_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);                        // 焦点行自动滚入视口
    lv_obj_set_style_text_font(btn, UI_FONT_BODY, 0);

    // 名称标签(列表按钮自带的child 0):限宽单行,超长省略
    lv_obj_t *name_label = lv_obj_get_child(btn, 0);
    if (name_label != NULL)
    {
        lv_obj_set_flex_grow(name_label, 0);
        lv_obj_set_width(name_label, 62);
        lv_obj_set_height(name_label, lv_font_get_line_height(UI_FONT_BODY));
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0x1F2937), 0);
    }

    // 状态标签(child 1):弹性占满余宽,右对齐着色
    lv_obj_t *state_label = lv_label_create(btn);
    lv_obj_set_flex_grow(state_label, 1);
    lv_obj_set_height(state_label, lv_font_get_line_height(UI_FONT_BODY));
    lv_label_set_long_mode(state_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(state_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(state_label, bt_state_to_color(BT_LINK_STATE_UNLINKED), 0);
    lv_label_set_text(state_label, "(未连接)");

    lv_obj_add_event_cb(btn, list_event_handler, LV_EVENT_CLICKED, NULL);
    return btn;
}

// 应用连接状态到设备行(user_data/状态文本/行背景)
static void apply_row_state(lv_obj_t *row, BtLinkState state)
{
    lv_obj_set_user_data(row, bt_state_to_user_data(state));
    lv_obj_t *state_label = lv_obj_get_child(row, 1);
    if (state_label != NULL)
    {
        lv_label_set_text(state_label, bt_state_to_text(state));
        lv_obj_set_style_text_color(state_label, bt_state_to_color(state), 0);
    }
    // 已连接行浅绿背景(status-success-bg),其余白底由状态文本表达
    lv_obj_set_style_bg_color(row,
                              state == BT_LINK_STATE_LINKED ? COLOR_LINKED : lv_color_hex(0xFFFFFF),
                              0);
}

// 行数不足视口时给列表顶部留白,保证"完成"行始终贴卡片底部
// (列表内容从顶堆叠,无留白时唯一的完成行会顶到卡片上沿)
static void bt_list_apply_pad()
{
    const int32_t viewport = BT_CARD_H - 2; // 减去上下1px边框
    const int32_t content = (int32_t)(bt_rows.size() + 1) * BT_ROW_HEIGHT; // 设备行+完成行
    lv_obj_set_style_pad_top(bt_list, content < viewport ? viewport - content : 0, 0);
}

/*****************************
      设备信息弹窗
*****************************/
// 复用弹窗"点击关闭"路径:恢复分组/焦点并销毁弹窗
static void bt_popup_close(lv_obj_t *cont)
{
    if (cont != nullptr && lv_obj_is_valid(cont))
        lv_obj_send_event(cont, LV_EVENT_CLICKED, NULL);
}

static void bt_popup_connect_cb(lv_event_t *e)
{
    lv_obj_t *cont = (lv_obj_t *)lv_event_get_user_data(e);
    bt_popup_close(cont); // 先关弹窗再发起,行状态由"(连接中)"跟踪
    if (!ui_bt_link(bt_popup_mac))
        ui_popwin_msgbox("连接失败", g1, bt_list);
}

static void bt_popup_disconnect_cb(lv_event_t *e)
{
    lv_obj_t *cont = (lv_obj_t *)lv_event_get_user_data(e);
    bt_popup_close(cont);
    if (!ui_bt_unlink(bt_popup_mac))
        ui_popwin_msgbox("断开失败", g1, bt_list);
}

static void bt_popup_close_cb(lv_event_t *e)
{
    bt_popup_close((lv_obj_t *)lv_event_get_user_data(e));
}

// 单击设备行弹出:设备称谓/连接状态/MAC地址 + 连接(断开连接)/关闭按钮
static void show_device_info(lv_obj_t *row)
{
    const std::string mac = get_bt_mac(row);
    if (mac.empty())
        return;
    bt_popup_mac = mac;

    // 连接状态取自行user_data(原始状态),不解析标签文本
    const void *row_state = lv_obj_get_user_data(row);
    const bool linked = row_state == BT_LINKED;
    const bool linking = row_state == BT_LINKING;
    const char *state_text = linked ? "(已连接)" : (linking ? "(连接中)" : "(未连接)");
    const lv_color_t state_color = linked   ? lv_color_hex(0x059669)
                                   : linking ? lv_color_hex(0x2D6BDB)
                                             : lv_palette_main(LV_PALETTE_RED);

    lv_obj_t **ret = ui_popwin(true, g1, row);
    lv_obj_t *win = ret[1];
    lv_obj_set_size(win, 144, 75);
    lv_obj_set_style_bg_color(win, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(win, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(win, 8, 0);
    lv_obj_set_style_shadow_width(win, 15, 0);
    lv_obj_set_style_shadow_opa(win, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(win, 4, 0);

    // 标题行:蓝牙图标 + "设备信息"
    lv_obj_t *img = lv_image_create(win);
    lv_image_set_src(img, &ui_img_bt);
    lv_img_set_zoom(img, 64);
    lv_obj_set_size(img, 16, 16);
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 4, 0);
    lv_obj_t *label = lv_label_create(win);
    lv_label_set_text(label, "设备信息");
    lv_obj_set_style_text_font(label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 2, 0);

    // 分隔线
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
    static lv_point_precise_t line_points[] = {{4, 0}, {140, 0}};
    lv_obj_t *line = lv_line_create(win);
    lv_line_set_points(line, line_points, 2);
    lv_obj_add_style(line, &style_line, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, 17);

    // 行1:设备称谓(左) + 连接状态(右,着色)
    label = lv_label_create(win);
    lv_label_set_text_fmt(label, "设备%u", ui_info_get_number(mac));
    lv_obj_set_style_text_font(label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 6, 20);
    label = lv_label_create(win);
    lv_label_set_text(label, state_text);
    lv_obj_set_style_text_font(label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(label, state_color, 0);
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -6, 20);

    // 行2:MAC地址(固定宽度单行,超长省略)
    label = lv_label_create(win);
    lv_obj_set_width(label, 132);
    lv_obj_set_height(label, lv_font_get_line_height(UI_FONT_BODY));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text_fmt(label, "MAC:%s", mac.c_str());
    lv_obj_set_style_text_font(label, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x626367), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 6, 38);

    // 按钮组:进入弹窗即绑定编码器,关闭时由弹窗恢复页面分组
    lv_group_t *btn_group = lv_group_create();
    ui_bind_group_to_all_encoders(btn_group);

    // 主操作:已连接->断开连接;未连接/连接中->连接
    lv_obj_t *btn = ui_add_button(win, linked ? "断开连接" : "连接", linked ? 56 : 34, 18, UI_FONT_BODY);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_all(btn, 2, 0);
    lv_group_add_obj(btn_group, btn);
    lv_group_focus_obj(btn);
    lv_obj_add_event_cb(btn, linked ? bt_popup_disconnect_cb : bt_popup_connect_cb,
                        LV_EVENT_CLICKED, ret[0]);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 6, -2);

    // 关闭按钮:次级样式(白底灰边)
    btn = ui_add_button(win, "关闭", 34, 18, UI_FONT_BODY);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_pad_all(btn, 2, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xF4F5F7), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xD1D5DB), 0);
    lv_obj_t *close_label = lv_obj_get_child(btn, 0);
    if (close_label != NULL)
        lv_obj_set_style_text_color(close_label, lv_color_hex(0x1F2937), 0);
    lv_group_add_obj(btn_group, btn);
    lv_obj_add_event_cb(btn, bt_popup_close_cb, LV_EVENT_CLICKED, ret[0]);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -6, -2);

    // 弹窗销毁时释放按钮组(与ui_popwin_msgbox的button_group清理等价)
    lv_obj_add_event_cb(ret[0], [](lv_event_t *e)
                        {
                            lv_group_t *g = (lv_group_t *)lv_event_get_user_data(e);
                            if (g != nullptr)
                                lv_group_delete(g);
                        },
                        LV_EVENT_DELETE, btn_group);
}

/*****************************
      页面搭建与刷新
*****************************/
void ui_bt_init()
{
    bt_widget = ui_add_win();
    bt_rows.clear();
    bt_empty_hint = nullptr;

    lv_obj_set_user_data(bt_widget, (void *)"bt_list");

    g1 = lv_group_create(); // 页面唯一分组:完成行与设备行同级导航
    lv_group_set_default(g1);
    lv_obj_add_event_cb(bt_widget, [](lv_event_t *e)
                        {
                            lv_group_t *g = (lv_group_t *)lv_event_get_user_data(e);
                            ui_bind_group_to_all_encoders(g);
                        },
                        (lv_event_code_t)(LV_EVENT_LAST + 1), g1);

    // 顶部标题
    lv_obj_t *title = lv_label_create(bt_widget);
    lv_label_set_text(title, "麦克风设备连接");
    lv_obj_set_style_text_font(title, UI_FONT_HEADING, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1F2937), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, BT_TITLE_OFFSET_Y);

    // 圆角卡片列表:底色上的一层近全屏控件,视口3整行×18px,行首对齐滚动吸附
    bt_list = lv_list_create(bt_widget);
    lv_obj_set_style_pad_all(bt_list, 0, 0);
    lv_obj_set_style_border_width(bt_list, 1, 0);
    lv_obj_set_style_border_color(bt_list, lv_color_hex(0xE5E7EB), 0);
    lv_obj_set_style_bg_color(bt_list, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(bt_list, 6, 0);
    lv_obj_set_style_clip_corner(bt_list, true, 0); // 行背景裁剪进圆角
    lv_obj_set_style_width(bt_list, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(bt_list, lv_color_hex(0x2D6BDB), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(bt_list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_scrollbar_mode(bt_list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(bt_list, LV_SCROLL_SNAP_START);
    lv_obj_set_size(bt_list, BT_CARD_W, BT_CARD_H);
    lv_obj_align(bt_list, LV_ALIGN_TOP_LEFT, BT_CARD_X, BT_CARD_Y);

    // 末行"完成":绿色动作行,替代原独立完成按钮(设备行出现时移动到列表末尾)
    bt_finish_row = lv_list_add_button(bt_list, NULL, "完成");
    lv_obj_set_style_pad_all(bt_finish_row, 0, 0);
    lv_obj_set_height(bt_finish_row, BT_ROW_HEIGHT);
    lv_obj_set_style_pad_hor(bt_finish_row, 4, 0);
    lv_obj_set_style_radius(bt_finish_row, 0, 0);
    lv_obj_set_style_border_width(bt_finish_row, 0, 0);
    lv_obj_set_style_bg_color(bt_finish_row, lv_color_hex(0x059669), 0);
    lv_obj_set_style_bg_color(bt_finish_row, lv_color_hex(0x047857), LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(bt_finish_row, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(bt_finish_row, lv_color_hex(0x2D6BDB), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(bt_finish_row, 1, LV_STATE_FOCUSED);
    lv_obj_add_flag(bt_finish_row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_t *finish_label = lv_obj_get_child(bt_finish_row, 0);
    if (finish_label != NULL)
    {
        lv_obj_set_style_text_font(finish_label, UI_FONT_HEADING, 0);
        lv_obj_set_style_text_color(finish_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_align(finish_label, LV_TEXT_ALIGN_CENTER, 0);
    }
    lv_obj_add_event_cb(bt_finish_row, button_finish_cb, LV_EVENT_CLICKED, bt_widget);
    lv_group_add_obj(g1, bt_finish_row);
    lv_group_focus_obj(bt_finish_row);

    // 空列表占位提示(标签不可点击,不影响列表交互;首个设备出现时移除)
    // 完成行经顶部留白贴住卡片底部,提示居中于其上方空白区(0设备时空白区36px)
    bt_list_apply_pad();
    bt_empty_hint = lv_label_create(bt_widget);
    lv_label_set_text(bt_empty_hint, "暂无设备");
    lv_obj_set_style_text_font(bt_empty_hint, UI_FONT_BODY, 0);
    lv_obj_set_style_text_color(bt_empty_hint, lv_color_hex(0x9CA3AF), 0);
    lv_obj_align_to(bt_empty_hint, bt_list, LV_ALIGN_TOP_MID, 0, 12);

    ui_bind_group_to_all_encoders(g1);

    // 已连接设备不会重新广播,先注入设备管理器中的已知设备,再开始扫描新设备;
    // 页面重进时清除"用户手动断开"抑制,恢复发现即自动连接
    ui_bt_reset_autoconnect();
    ui_bt_seed_devices();
    ui_bt_search();
}

// 完成流程:进入主界面(BLE直发;WiFi经协议迁移),供完成行与开机自动进入共用
void ui_bt_try_finish()
{
    uint8_t n = ui_list_get_link_num();
    if (n < 1)
    {
        ui_popwin_msgbox("请先连接设备", g1, bt_finish_row);
        return;
    }
    if (config::config.rf.mode == RF_MODE_WIFI)
    {
        if (ui_bt_wifi_ready())
        {
            // 从主界面重新进入本页且设备仍经WiFi连接:无需BLE->WiFi迁移,直接回主界面
            // (若仍走协议切换,wifi_open会先关闭重建AP,已连接设备被迫断开重连)
            ui_bt_finish_to_main();
            return;
        }
        // WiFi模式:先显示连接提示,BLE->WiFi迁移由协议切换任务完成后自动进入主界面
        ui_popwin_msgbox("正在连接...", g1, bt_finish_row);
        ui_bt_pause_search();
    }
    else
    {
        // BLE模式:下发音频启动命令后直接进入主界面
        ui_bt_pause_search();
        ui_bt_finish_to_main();
    }
}

// 完成按钮点击回调 进入主窗口
static void button_finish_cb(lv_event_t *e)
{
    ui_bt_try_finish();
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
    bt_finish_row = nullptr;
    bt_empty_hint = nullptr;
    bt_popup_mac.clear();
    delete_bt_groups();
}

// 设备行点击回调 -> 设备信息弹窗(单击即弹窗,连接/断开由弹窗按钮或自动连接完成)
static void list_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (code != LV_EVENT_CLICKED || obj == bt_finish_row)
        return;
    show_device_info(obj);
}

// 添加蓝牙/更新蓝牙状态
void ui_bt_update(const std::string &mac, BtLinkState state)
{
    LV_LOCK();

    // 设备连接页未打开时(主界面运行/重连期间)不更新列表,避免引用已销毁控件
    if (bt_list == nullptr || !lv_obj_is_valid(bt_list) || g1 == nullptr)
    {
        LV_UNLOCK();
        return;
    }

    // 查找既有行
    lv_obj_t *row = nullptr;
    for (const auto &bt_row : bt_rows)
    {
        if (bt_row.second == mac)
        {
            row = bt_row.first;
            break;
        }
    }

    // 行已存在且状态未变:不重复刷新,避免扫描回调反复重绘
    if (row != nullptr && lv_obj_get_user_data(row) == bt_state_to_user_data(state))
    {
        LV_UNLOCK();
        return;
    }

    if (row == nullptr)
    {
        row = create_device_row(mac);
        bt_rows.emplace_back(row, mac);
        lv_group_add_obj(g1, row);
        // 设备行按发现顺序填充上方,"完成"始终保持在列表末尾
        lv_obj_move_to_index(bt_finish_row, (int32_t)lv_obj_get_child_count(bt_list) - 1);
        if (bt_empty_hint != nullptr && lv_obj_is_valid(bt_empty_hint))
        {
            lv_obj_delete(bt_empty_hint);
            bt_empty_hint = nullptr;
        }
    }
    apply_row_state(row, state);
    bt_list_apply_pad(); // 行数变化后重算顶部留白,保持"完成"贴底
    lv_obj_set_scrollbar_mode(bt_list,
                              bt_rows.size() > BT_VISIBLE_ROWS ? LV_SCROLLBAR_MODE_ON : LV_SCROLLBAR_MODE_OFF);
    LV_UNLOCK();
}

static uint8_t ui_list_get_link_num()
{
    uint8_t n = 0;
    for (const auto &bt_row : bt_rows)
    {
        if (lv_obj_get_user_data(bt_row.first) == BT_LINKED)
            n++;
    }
    return n;
}
