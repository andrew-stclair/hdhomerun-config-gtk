#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define HDHOMERUN_TYPE_APP (hdhomerun_app_get_type())
G_DECLARE_FINAL_TYPE(HDHomeRunApp, hdhomerun_app, HDHOMERUN, APP, AdwApplication)

HDHomeRunApp *hdhomerun_app_new(void);

G_END_DECLS
