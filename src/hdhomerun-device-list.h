#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <stdint.h>

G_BEGIN_DECLS

#define HDHOMERUN_TYPE_DEVICE_LIST (hdhomerun_device_list_get_type())
G_DECLARE_FINAL_TYPE(HDHomeRunDeviceList, hdhomerun_device_list, HDHOMERUN, DEVICE_LIST, GtkBox)

HDHomeRunDeviceList *hdhomerun_device_list_new(void);

G_END_DECLS
