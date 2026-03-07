#include <adwaita.h>
#include "hdhomerun-app.h"

int main(int argc, char *argv[])
{
  adw_init();
  g_autoptr(HDHomeRunApp) app = hdhomerun_app_new();
  return g_application_run(G_APPLICATION(app), argc, argv);
}
