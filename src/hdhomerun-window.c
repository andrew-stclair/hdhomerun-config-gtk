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
#include <libhdhomerun/hdhomerun.h>

#define HDHOMERUN_IP_STRING_SIZE 64  /* Size required by libhdhomerun API */

struct _HdhomerunWindow
{
  AdwApplicationWindow parent_instance;

  /* Template widgets */
  AdwHeaderBar *header_bar;
  AdwNavigationSplitView *split_view;
  GtkListBox *device_list;
  GtkStack *content_stack;
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
on_add_device_response (AdwAlertDialog *dialog,
                        char *response,
                        HdhomerunWindow *self)
{
  GtkWidget *entry;
  const char *ip_address;
  
  if (g_strcmp0 (response, "add") != 0)
    return;
  
  entry = g_object_get_data (G_OBJECT (dialog), "ip-entry");
  ip_address = gtk_editable_get_text (GTK_EDITABLE (entry));
  
  if (ip_address && *ip_address)
    {
      /* Validate IP address format */
      if (!g_hostname_is_ip_address (ip_address))
        {
          AdwDialog *error_dialog;
          
          error_dialog = adw_alert_dialog_new (_("Invalid IP Address"),
                                              _("Please enter a valid IPv4 or IPv6 address."));
          adw_alert_dialog_add_response (ADW_ALERT_DIALOG (error_dialog), "ok", _("_OK"));
          adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (error_dialog), "ok");
          adw_dialog_present (error_dialog, GTK_WIDGET (self));
          return;
        }
      
      HdhomerunDeviceRow *row;
      
      row = hdhomerun_device_row_new (ip_address, "Manual Device", ip_address);
      gtk_list_box_append (self->device_list, GTK_WIDGET (row));
      
      g_message ("Adding device at IP: %s", ip_address);
    }
}

static void
on_add_device_clicked (GtkButton *button,
                       HdhomerunWindow *self)
{
  AdwDialog *dialog;
  GtkWidget *entry;
  
  (void)button; /* unused */
  
  dialog = adw_alert_dialog_new (_("Add Device Manually"), 
                                 _("Enter the IP address of the HDHomeRun device"));
  
  entry = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "192.168.1.100");
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), entry);
  
  g_object_set_data (G_OBJECT (dialog), "ip-entry", entry);
  
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "cancel", _("_Cancel"));
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "add", _("_Add"));
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "add", ADW_RESPONSE_SUGGESTED);
  
  g_signal_connect (dialog, "response", G_CALLBACK (on_add_device_response), self);
  
  adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
on_refresh_clicked (GtkButton *button,
                   HdhomerunWindow *self)
{
  GtkWidget *child;
  struct hdhomerun_discover_t *ds;
  uint32_t flags;
  uint32_t device_types[1];
  struct hdhomerun_discover2_device_t *device = NULL;
  
  (void)button; /* unused */
  
  /* Clear existing devices */
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->device_list))) != NULL)
    {
      gtk_list_box_remove (self->device_list, child);
    }
  
  g_message ("Refreshing device list...");
  
  /* Perform device discovery using libhdhomerun */
  ds = hdhomerun_discover_create (NULL);
  if (!ds)
    {
      g_warning ("Failed to initialize device discovery");
      return;
    }
  
  flags = HDHOMERUN_DISCOVER_FLAGS_IPV4_GENERAL;
  device_types[0] = HDHOMERUN_DEVICE_TYPE_TUNER;
  
  if (hdhomerun_discover2_find_devices_broadcast (ds, flags, device_types, 1) >= 0)
    {
      device = hdhomerun_discover2_iter_device_first (ds);
      while (device)
        {
          struct hdhomerun_discover2_device_if_t *device_if;
          struct sockaddr_storage ip_address;
          char ip_address_str[HDHOMERUN_IP_STRING_SIZE];  /* Buffer for IPv4/IPv6 address string */
          uint32_t device_id;
          char device_id_str[9];    /* Buffer for 8-char hex ID plus null */
          HdhomerunDeviceRow *row;
          
          device_id = hdhomerun_discover2_device_get_device_id (device);
          g_snprintf (device_id_str, sizeof (device_id_str), "%08X", device_id);
          
          /* Iterate through all network interfaces for this device */
          device_if = hdhomerun_discover2_iter_device_if_first (device);
          while (device_if)
            {
              hdhomerun_discover2_device_if_get_ip_addr (device_if, &ip_address);
              /* Convert IP address to string, FALSE omits port for display */
              hdhomerun_sock_sockaddr_to_ip_str (ip_address_str, (struct sockaddr *)&ip_address, FALSE);
              
              row = hdhomerun_device_row_new (device_id_str, "HDHomeRun", ip_address_str);
              gtk_list_box_append (self->device_list, GTK_WIDGET (row));
              
              g_message ("Found device: %s at %s", device_id_str, ip_address_str);
              
              device_if = hdhomerun_discover2_iter_device_if_next (device_if);
            }
          
          device = hdhomerun_discover2_iter_device_next (device);
        }
    }
  
  hdhomerun_discover_destroy (ds);
}

static gboolean
refresh_devices_async (gpointer user_data)
{
  HdhomerunWindow *self = HDHOMERUN_WINDOW (user_data);
  
  on_refresh_clicked (NULL, self);
  
  return G_SOURCE_REMOVE;
}

static void
hdhomerun_window_finalize (GObject *object)
{
  HdhomerunWindow *self = (HdhomerunWindow *)object;

  g_clear_object (&self->settings);
  g_list_free (self->devices);

  G_OBJECT_CLASS (hdhomerun_window_parent_class)->finalize (object);
}

static void
hdhomerun_window_class_init (HdhomerunWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hdhomerun_window_finalize;

  /* Ensure HdhomerunTunerControls type is registered before loading the template */
  g_type_ensure (HDHOMERUN_TYPE_TUNER_CONTROLS);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/andrewstclair/HDHomeRunConfig/hdhomerun-window.ui");
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, split_view);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, device_list);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunWindow, content_stack);
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
  
  /* Set initial visible child for the content stack */
  gtk_stack_set_visible_child_name (self->content_stack, "placeholder");
  
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
  
  /* Automatically discover devices on startup (async to avoid blocking UI) */
  g_idle_add (refresh_devices_async, self);
}
