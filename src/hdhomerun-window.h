#pragma once

#include <adwaita.h>
#include "hdhomerun-app.h"

G_BEGIN_DECLS

#define HDHOMERUN_TYPE_WINDOW (hdhomerun_window_get_type())
G_DECLARE_FINAL_TYPE(HDHomeRunWindow, hdhomerun_window, HDHOMERUN, WINDOW, AdwApplicationWindow)

HDHomeRunWindow *hdhomerun_window_new(HDHomeRunApp *app);

G_END_DECLS
