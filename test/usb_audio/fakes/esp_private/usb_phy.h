#pragma once
enum { USB_PHY_CTRL_SERIAL_JTAG, USB_PHY_CTRL_OTG, USB_PHY_TARGET_INT, USB_OTG_MODE_DEVICE, USB_PHY_SPEED_FULL };
struct usb_phy_config_t { int controller, target, otg_mode, otg_speed; };
using usb_phy_handle_t = int *;
#define ESP_OK 0
int usb_new_phy(const usb_phy_config_t *, usb_phy_handle_t *);
int usb_del_phy(usb_phy_handle_t);
