#include "hdhomerun-device-list.h"
#include "hdhomerun.h"

struct _HDHomeRunDeviceList {
  GtkBox parent_instance;

  GtkListBox *list_box;
  AdwStatusPage *status_page;
};

G_DEFINE_TYPE(HDHomeRunDeviceList, hdhomerun_device_list, GTK_TYPE_BOX)

enum {
  DEVICE_SELECTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void on_row_activated(HDHomeRunDeviceList *self, GtkListBoxRow *row, GtkListBox *list_box)
{
  if (!row) return;
  
  uint32_t device_id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "device-id"));
  uint32_t tuner_count = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "tuner-count"));
  if (device_id == 0) return;

  g_signal_emit(self, signals[DEVICE_SELECTED], 0, device_id, tuner_count);
}

static void add_device(HDHomeRunDeviceList *self, uint32_t device_id, const char *ip, uint32_t tuner_count)
{
  g_autofree char *id_str = g_strdup_printf("%08X", device_id);
  GtkWidget *row = adw_action_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), id_str);
  adw_action_row_set_subtitle(ADW_ACTION_ROW(row), ip);
  
  g_object_set_data(G_OBJECT(row), "device-id", GUINT_TO_POINTER(device_id));
  g_object_set_data(G_OBJECT(row), "tuner-count", GUINT_TO_POINTER(tuner_count));
  
  gtk_list_box_append(self->list_box, row);
  gtk_widget_set_visible(GTK_WIDGET(self->status_page), FALSE);
}

static gboolean discover_devices(HDHomeRunDeviceList *self)
{
  struct hdhomerun_discover_t *ds = hdhomerun_discover_create(NULL);
  if (!ds) return G_SOURCE_REMOVE;

  uint32_t device_types[1] = { HDHOMERUN_DEVICE_TYPE_WILDCARD };
  int ret = hdhomerun_discover2_find_devices_broadcast(ds, HDHOMERUN_DISCOVER_FLAGS_IPV4_GENERAL, device_types, 1);
  
  if (ret > 0) {
    // Clear existing (except status page)
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(self->list_box)))) {
        if (child == GTK_WIDGET(self->status_page)) {
             // Keep it but hide it if we have devices
             gtk_widget_set_visible(child, FALSE);
             // Let's just remove everything and re-add status if empty.
        }
        gtk_list_box_remove(self->list_box, child);
    }

    struct hdhomerun_discover2_device_t *device = hdhomerun_discover2_iter_device_first(ds);
    int count = 0;
    while (device) {
      uint32_t device_id = hdhomerun_discover2_device_get_device_id(device);
      uint32_t tuner_count = hdhomerun_discover2_device_get_tuner_count(device);
      struct hdhomerun_discover2_device_if_t *device_if = hdhomerun_discover2_iter_device_if_first(device);
      if (device_id != 0 && device_if) {
        struct sockaddr_storage ip_addr;
        hdhomerun_discover2_device_if_get_ip_addr(device_if, &ip_addr);
        char ip_str[64];
        hdhomerun_sock_sockaddr_to_ip_str(ip_str, (struct sockaddr *)&ip_addr, true);
        add_device(self, device_id, ip_str, tuner_count);
        count++;
      }
      device = hdhomerun_discover2_iter_device_next(device);
    }
    
    if (count == 0) {
        gtk_list_box_append(self->list_box, GTK_WIDGET(self->status_page));
        gtk_widget_set_visible(GTK_WIDGET(self->status_page), TRUE);
    }
  }

  hdhomerun_discover_destroy(ds);
  return G_SOURCE_CONTINUE;
}

static void hdhomerun_device_list_class_init(HDHomeRunDeviceListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  g_type_ensure (ADW_TYPE_STATUS_PAGE);

  gtk_widget_class_set_template_from_resource(widget_class, "/com/silicondust/HDHomeRunConfig/device-list.ui");
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceList, list_box);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceList, status_page);

  signals[DEVICE_SELECTED] = g_signal_new("device-selected",
                                          G_TYPE_FROM_CLASS(klass),
                                          G_SIGNAL_RUN_LAST,
                                          0, NULL, NULL,
                                          NULL,
                                          G_TYPE_NONE,
                                          2, G_TYPE_UINT, G_TYPE_UINT);
}

static void hdhomerun_device_list_init(HDHomeRunDeviceList *self)
{
  gtk_widget_init_template(GTK_WIDGET(self));
  
  g_signal_connect_swapped(self->list_box, "row-activated", G_CALLBACK(on_row_activated), self);

  // Periodic discovery
  g_timeout_add_seconds(5, (GSourceFunc)discover_devices, self);
  discover_devices(self); // Initial discovery
}
