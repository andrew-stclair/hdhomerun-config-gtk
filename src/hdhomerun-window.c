#include "hdhomerun-window.h"
#include "hdhomerun-device-list.h"
#include "hdhomerun-device-view.h"

struct _HDHomeRunWindow {
  AdwApplicationWindow parent_instance;

  AdwNavigationSplitView *split_view;
  HDHomeRunDeviceList *device_list;
  HDHomeRunDeviceView *device_view;
  AdwNavigationPage *content_page;
};

G_DEFINE_TYPE(HDHomeRunWindow, hdhomerun_window, ADW_TYPE_APPLICATION_WINDOW)

static void on_device_selected(HDHomeRunWindow *self, uint32_t device_id, uint32_t tuner_count, HDHomeRunDeviceList *list)
{
  hdhomerun_device_view_set_device(self->device_view, device_id, tuner_count);
  adw_navigation_split_view_set_show_content(self->split_view, TRUE);
}

HDHomeRunWindow *hdhomerun_window_new(HDHomeRunApp *app)
{
  return g_object_new(HDHOMERUN_TYPE_WINDOW, "application", app, NULL);
}

static void hdhomerun_window_class_init(HDHomeRunWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  g_type_ensure (HDHOMERUN_TYPE_DEVICE_LIST);
  g_type_ensure (HDHOMERUN_TYPE_DEVICE_VIEW);

  gtk_widget_class_set_template_from_resource(widget_class, "/com/silicondust/HDHomeRunConfig/window.ui");
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunWindow, split_view);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunWindow, device_list);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunWindow, device_view);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunWindow, content_page);
}

static void hdhomerun_window_init(HDHomeRunWindow *self)
{
  gtk_widget_init_template(GTK_WIDGET(self));

  g_signal_connect_swapped(self->device_list, "device-selected", G_CALLBACK(on_device_selected), self);
}
