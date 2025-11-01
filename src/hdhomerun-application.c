/* hdhomerun-application.c
 *
 * Copyright 2025 Andrew St. Clair
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hdhomerun-config-gtk-config.h"
#include "hdhomerun-application.h"
#include "hdhomerun-window.h"

#include <glib/gi18n.h>

struct _HdhomerunApplication
{
  AdwApplication parent_instance;
};

G_DEFINE_FINAL_TYPE (HdhomerunApplication, hdhomerun_application, ADW_TYPE_APPLICATION)

HdhomerunApplication *
hdhomerun_application_new (const char        *application_id,
                            GApplicationFlags  flags)
{
  g_return_val_if_fail (application_id != NULL, NULL);

  return g_object_new (HDHOMERUN_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

static void
hdhomerun_application_activate (GApplication *app)
{
  GtkWindow *window;

  g_assert (HDHOMERUN_IS_APPLICATION (app));

  window = gtk_application_get_active_window (GTK_APPLICATION (app));
  
  if (window == NULL)
    window = g_object_new (HDHOMERUN_TYPE_WINDOW,
                           "application", app,
                           NULL);

  gtk_window_present (window);
}

static void
hdhomerun_application_class_init (HdhomerunApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  app_class->activate = hdhomerun_application_activate;
}

static void
hdhomerun_application_about_action (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  static const char *developers[] = {"Andrew St. Clair", NULL};
  HdhomerunApplication *self = user_data;
  GtkWindow *window = NULL;

  g_assert (HDHOMERUN_IS_APPLICATION (self));

  window = gtk_application_get_active_window (GTK_APPLICATION (self));

  adw_show_about_dialog (window,
                         "application-name", "HDHomeRun Config",
                         "application-icon", "com.github.andrewstclair.HDHomeRunConfig",
                         "developer-name", "Andrew St. Clair",
                         "version", PACKAGE_VERSION,
                         "developers", developers,
                         "copyright", "© 2025 Andrew St. Clair",
                         "license-type", GTK_LICENSE_GPL_2_0,
                         NULL);
}

static void
hdhomerun_application_quit_action (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  HdhomerunApplication *self = user_data;

  g_assert (HDHOMERUN_IS_APPLICATION (self));

  g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
  { "quit", hdhomerun_application_quit_action },
  { "about", hdhomerun_application_about_action },
};

static void
hdhomerun_application_init (HdhomerunApplication *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   app_actions,
                                   G_N_ELEMENTS (app_actions),
                                   self);
  
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.quit",
                                         (const char *[]) { "<primary>q", NULL });
}
