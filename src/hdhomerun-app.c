#include "hdhomerun-app.h"
#include "hdhomerun-window.h"

struct _HDHomeRunApp {
  AdwApplication parent_instance;
};

G_DEFINE_TYPE(HDHomeRunApp, hdhomerun_app, ADW_TYPE_APPLICATION)

HDHomeRunApp *hdhomerun_app_new(void)
{
  return g_object_new(HDHOMERUN_TYPE_APP,
                      "application-id", "com.silicondust.HDHomeRunConfig",
                      "flags", G_APPLICATION_DEFAULT_FLAGS,
                      NULL);
}

static void hdhomerun_app_activate(GApplication *app)
{
  GtkWindow *window;

  window = gtk_application_get_active_window(GTK_APPLICATION(app));
  if (window == NULL) {
    window = GTK_WINDOW(hdhomerun_window_new(HDHOMERUN_APP(app)));
  }

  gtk_window_present(window);
}

static void hdhomerun_app_class_init(HDHomeRunAppClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS(klass);
  app_class->activate = hdhomerun_app_activate;
}

static void hdhomerun_app_init(HDHomeRunApp *self)
{
}
