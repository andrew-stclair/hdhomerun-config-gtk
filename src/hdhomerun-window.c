/* hdhomerun-window.c
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
#include "hdhomerun-window.h"
#include "hdhomerun-device-row.h"
#include "hdhomerun-tuner-controls.h"

#include <glib/gi18n.h>

struct _HdhomerunWindow
{
  AdwApplicationWindow parent_instance;

  /* Template widgets */
  AdwHeaderBar *header_bar;
  AdwNavigationSplitView *split_view;
  GtkListBox *device_list;
  AdwStatusPage *placeholder_page;
  HdhomerunTunerControls *tuner_controls;
  GtkButton *add_device_button;
  GtkButton *refresh_button;
  
  /* State */
  GSettings *settings;
  GList *devices;
};

G_DEFINE_FINAL_TYPE (HdhomerunWindow, hdhomerun_window, ADW_TYPE_APPLICATION_WINDOW)

static void
on_add_device_clicked (GtkButton *button,
                       HdhomerunWindow *self)
{
  AdwDialog *dialog;
  
  (void)button; /* unused */
  
  dialog = adw_alert_dialog_new (_("Add Device Manually"), 
                                 _("Enter the IP address of the HDHomeRun device"));
  
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "cancel", _("_Cancel"));
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "add", _("_Add"));
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "add", ADW_RESPONSE_SUGGESTED);
  
  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
on_refresh_clicked (GtkButton *button,
                   HdhomerunWindow *self)
{
  (void)button; /* unused */
  (void)self; /* unused */
  
  /* Placeholder for device discovery */
  g_print ("Refreshing device list...\n");
}

static void
hdhomerun_window_class_init (HdhomerunWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/andrewstclair/HDHomeRunConfig/hdhomerun-window.ui");
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, device_list);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, placeholder_page);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, tuner_controls);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, add_device_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, refresh_button);
  gtk_widget_class_bind_template_callback (widget_class, on_add_device_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_refresh_clicked);
}

static void
hdhomerun_window_init (HdhomerunWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings = g_settings_new ("com.github.andrewstclair.HDHomeRunConfig");
  
  /* Restore window state */
  g_settings_bind (self->settings, "window-width",
                   self, "default-width",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "window-height",
                   self, "default-height",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "window-maximized",
                   self, "maximized",
                   G_SETTINGS_BIND_DEFAULT);
}
