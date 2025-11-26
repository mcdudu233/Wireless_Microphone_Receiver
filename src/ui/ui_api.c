// 界面会调用这些函数用来获取数据
#include "ui/ui.h"
#include <string.h>
// 获取蓝牙设备名称列表
char **get_bt_list()
{
    int count = 4;
    char *names[] = {"设备1", "设备2", "设备3", "设备4"};
    char **bt_list = lv_malloc(sizeof(char *) * (count + 1));
    LV_ASSERT_MALLOC(bt_list);
    for (int i = 0; i < count; i++)
    {
        bt_list[i] = lv_malloc(strlen(names[i]) + 1);
        LV_ASSERT_MALLOC(bt_list[i]);
        strcpy(bt_list[i], names[i]);
    }
    bt_list[count] = NULL; // 用NULL标记结尾
    return bt_list;
    // for (int i = 0; bt_list[i] != NULL; i++) {   数据获取时遍历方式
    //     printf("第 %d 个元素：%s\n", i+1, bt_list[i]);
    // }
}
// 获取已连接蓝牙设备的名称列表
char **get_linked_bt_list()
{
    int count = 3;
    char *names[] = {"设备1","设备3","设备4"};
    char **bt_list = lv_malloc(sizeof(char *) * (count + 1));
    LV_ASSERT_MALLOC(bt_list);
    for (int i = 0; i < count; i++)
    {
        bt_list[i] = lv_malloc(strlen(names[i]) + 1);
        LV_ASSERT_MALLOC(bt_list[i]);
        strcpy(bt_list[i], names[i]);
    }
    bt_list[count] = NULL;
    return bt_list;
    // for (int i = 0; bt_list[i] != NULL; i++) {   数据获取时遍历方式
    //     printf("第 %d 个元素：%s\n", i+1, bt_list[i]);
    // }
}
// 获取设备左声道音量大小百分比
int32_t get_left_voice_per(char *device_name)
{
    int32_t vp;
    vp = 100;
    return vp;
}
// 获取设备右声道音量大小百分比
int32_t get_right_voice_per(char *device_name)
{
    int32_t vp;
    vp = 100;
    return vp;
}
// 获取设备信号强度百分比
int32_t get_signal_per(char *device_name)
{
    int32_t sp;
    sp = 80;
    return sp;
}
// 获取设备电量百分比
int32_t get_power_per(char *device_name)
{
    int32_t p;
    p = 70;
    return p;
}
// 获取设备上传速度
char *get_upload_speed()
{
    return "1.1b/s"; // 因为没办法直接确定单位，所以设置返回值为字符串 比如 1.1 b/s 100kb/s 直接作为数据显示输出
}

// 获取设备下载速度
char *get_download_speed()
{
    return "1.1b/s"; // 因为没办法直接确定单位，所以设置返回值为字符串 比如 1.1 b/s 100kb/s 直接作为数据显示输出
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
