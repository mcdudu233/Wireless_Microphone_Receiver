#include "ui/ui.h"
// void link_mbox_cb(lv_event_t* e)
// {
//     intptr_t choice = (intptr_t)lv_event_get_user_data(e);
//     switch (choice)
//     {
//     case 0:
//         /* code */

//         break;
//     case 1:
//         break;
//     default:
//         break;
//     }
// }
// 蓝牙连接按钮点击回调函数
void link_cb(lv_event_t* e)
{
    lv_obj_t * btn;
    lv_obj_t * label;
    lv_obj_t * list = (lv_obj_t*)lv_event_get_user_data(e);

    int32_t cnt = lv_obj_get_child_count_by_type(list,&lv_list_button_class);
    LV_LOG_USER("有%d个",cnt);
    for(int i=0; i<cnt; i++)
    {
        btn = lv_obj_get_child_by_type(list,i,&lv_list_button_class);
        if(lv_color_eq(lv_obj_get_style_bg_color(btn,0),lv_palette_main(selected)))
        {
            label = lv_obj_get_child(btn,0);
            char * bt_name = lv_label_get_text(label); // 待链接蓝牙的名称
            //TODO:链接操作
            LV_LOG_USER("已选择蓝牙%s",bt_name);
            lv_obj_set_style_bg_color(btn,lv_palette_main(linked),0); //设置连接状态
        }
        // lv_obj_t* msgbox = lv_msgbox_create(NULL);
        // lv_msgbox_add_title(msgbox, "Hello");

        // lv_msgbox_add_text(msgbox, "This is a message box with two buttons.");
        // lv_msgbox_add_close_button(msgbox);

        // btn = lv_msgbox_add_footer_button(msgbox, "Apply");
        // lv_obj_add_event_cb(btn, link_mbox_cb, LV_EVENT_CLICKED, (void*)1);
        // btn = lv_msgbox_add_footer_button(msgbox, "Cancel");
        // lv_obj_add_event_cb(btn, link_mbox_cb, LV_EVENT_CLICKED, (void*)0);
    }
}
// 完成按钮点击回调 进入主窗口
void main_widget_cb(lv_event_t * e)
{
    ui_main_init(e);
}
// 蓝牙列表点击事件回调
static void list_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target_obj(e);
    lv_obj_t * list = lv_obj_get_parent(obj);
    if(code == LV_EVENT_CLICKED) {
        LV_UNUSED(obj);
        lv_obj_t * label = lv_obj_get_child(obj,0);
        if(lv_color_eq(lv_obj_get_style_bg_color(obj,0),lv_palette_main(none))){
            lv_obj_set_style_bg_color(obj,lv_palette_main(selected),0);
        }
        else{
            lv_obj_set_style_bg_color(obj,lv_palette_main(none),0);
        }
        //TODO:功能待验证
        lv_group_focus_obj(list); // 重置焦点

        LV_LOG_USER("Clicked: %s", lv_list_get_button_text(list, obj));

    }
}
void ui_bt_init(lv_event_t * e)
{
    // lv_obj_t* label = lv_label_create(lv_screen_active());
    // lv_label_set_text(label,"finish widget");
    // lv_obj_center(label);
    /*Create a list*/
    lv_obj_t * btn;
    lv_obj_t * label;
    lv_obj_t* bt_widget = add_win();
    lv_obj_t* bt_list = lv_list_create(bt_widget);
    lv_obj_set_style_pad_all(bt_list, 0, 0);
    obj_set_size(bt_list, 160*0.6,70);

    obj_set_pos(bt_list,0,0);
    lv_obj_align_to(bt_list,bt_widget,LV_ALIGN_TOP_LEFT,5,5);

    // 示例蓝牙
    label = lv_list_add_text(bt_list,"选择设备");
    //lv_obj_set_style_text_font(label,&lv_font_harmonyos_12,0);
    lv_obj_align(label,LV_ALIGN_CENTER,0,0);

    add_list_obj(bt_list,"device1",list_event_handler,NULL,none);
    add_list_obj(bt_list,"device2",list_event_handler,NULL,none);
    add_list_obj(bt_list,"device3",list_event_handler,NULL,none);
    add_list_obj(bt_list,"device4",list_event_handler,NULL,none);

    btn = add_button(bt_widget,"连接",160*0.3,20,NULL);
    lv_obj_align_to(btn,bt_list,LV_ALIGN_OUT_RIGHT_MID,5,-15);
    lv_obj_add_event_cb(btn,link_cb,LV_EVENT_CLICKED,bt_list);
    lv_obj_t * last = btn;

    btn = add_button(bt_widget,"完成",160*0.3,20,NULL);
    lv_obj_align_to(btn,bt_list,LV_ALIGN_OUT_RIGHT_MID,5,15);
    lv_obj_add_event_cb(btn,main_widget_cb,LV_EVENT_CLICKED,bt_widget);
}
