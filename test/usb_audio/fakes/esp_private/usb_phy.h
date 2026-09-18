#pragma once
enum { USB_PHY_CTRL_OTG, USB_PHY_CTRL_SERIAL_JTAG };
enum { USB_PHY_TARGET_INT };
enum { USB_PHY_MODE_DEFAULT, USB_OTG_MODE_HOST, USB_OTG_MODE_DEVICE };
enum { USB_PHY_SPEED_UNDEFINED, USB_PHY_SPEED_LOW, USB_PHY_SPEED_FULL };
struct usb_phy_config_t { int controller, target, otg_mode, otg_speed; };
using usb_phy_handle_t = int *;
#define ESP_OK 0
int usb_new_phy(const usb_phy_config_t *, usb_phy_handle_t *);
int usb_del_phy(usb_phy_handle_t);
