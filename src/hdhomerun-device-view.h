#pragma once

#include <adwaita.h>
#include <stdint.h>

G_BEGIN_DECLS

#define HDHOMERUN_TYPE_DEVICE_VIEW (hdhomerun_device_view_get_type())
G_DECLARE_FINAL_TYPE(HDHomeRunDeviceView, hdhomerun_device_view, HDHOMERUN, DEVICE_VIEW, AdwBin)

void hdhomerun_device_view_set_device(HDHomeRunDeviceView *self, uint32_t device_id, uint32_t tuner_count);

G_END_DECLS
