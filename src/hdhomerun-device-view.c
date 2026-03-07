#include "hdhomerun-device-view.h"
#include "hdhomerun.h"
#include <stdio.h>

struct _HDHomeRunDeviceView {
  AdwBin parent_instance;

  GtkStack *stack;
  AdwActionRow *id_row;
  AdwActionRow *model_row;
  AdwActionRow *firmware_row;
  GtkListBox *tuner_list;

  AdwPreferencesGroup *scan_group;
  GtkProgressBar *scan_progress;
  GtkLabel *scan_label;
  AdwComboRow *region_combo;

  uint32_t current_device_id;
  uint32_t current_tuner_count;
  struct hdhomerun_device_t *hd;
  guint poll_id;
};

G_DEFINE_TYPE(HDHomeRunDeviceView, hdhomerun_device_view, ADW_TYPE_BIN)

typedef struct {
  HDHomeRunDeviceView *view;
  uint32_t tuner_index;
} ScanTaskData;

static void on_scan_clicked(GtkButton *button, gpointer user_data);

static void on_freq_applied(AdwEntryRow *row, gpointer user_data)
{
  HDHomeRunDeviceView *self = user_data;
  uint32_t tuner_index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "tuner-index"));
  const char *text = gtk_editable_get_text(GTK_EDITABLE(row));

  g_message("UI: Setting tuner %u to %s", tuner_index, text);
  
  if (self->hd) {
    hdhomerun_device_set_tuner(self->hd, tuner_index);
    if (hdhomerun_device_set_tuner_channel(self->hd, text) <= 0) {
      g_warning("UI: Failed to set channel/frequency to %s", text);
    }
  }
}

static gboolean poll_tuner_status(HDHomeRunDeviceView *self)
{
  if (!self->hd) return G_SOURCE_REMOVE;

  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(self->tuner_list));
  uint32_t i = 0;
  while (child) {
    if (ADW_IS_EXPANDER_ROW(child)) {
      hdhomerun_device_set_tuner(self->hd, i);
      char *status_str;
      struct hdhomerun_tuner_status_t status;
      
      if (hdhomerun_device_get_tuner_status(self->hd, &status_str, &status) > 0) {
        adw_expander_row_set_subtitle(ADW_EXPANDER_ROW(child), status_str);
        
        AdwActionRow *ss_row = g_object_get_data(G_OBJECT(child), "ss-row");
        AdwActionRow *snq_row = g_object_get_data(G_OBJECT(child), "snq-row");
        AdwActionRow *seq_row = g_object_get_data(G_OBJECT(child), "seq-row");
        
        if (ss_row) {
          g_autofree char *val = g_strdup_printf("%u%%", status.signal_strength);
          adw_action_row_set_subtitle(ss_row, val);
        }
        if (snq_row) {
          g_autofree char *val = g_strdup_printf("%u%%", status.signal_to_noise_quality);
          adw_action_row_set_subtitle(snq_row, val);
        }
        if (seq_row) {
          g_autofree char *val = g_strdup_printf("%u%%", status.symbol_error_quality);
          adw_action_row_set_subtitle(seq_row, val);
        }
      }
    }
    child = gtk_widget_get_next_sibling(child);
    i++;
  }

  return G_SOURCE_CONTINUE;
}

static void refresh_tuners(HDHomeRunDeviceView *self, uint32_t tuner_count)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child(GTK_WIDGET(self->tuner_list)))) {
    gtk_list_box_remove(self->tuner_list, child);
  }

  for (uint32_t i = 0; i < tuner_count; i++) {
    GtkWidget *expander = adw_expander_row_new();
    g_autofree char *title = g_strdup_printf("Tuner %u", i);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(expander), title);
    
    GtkWidget *scan_button = gtk_button_new_with_label("Scan");
    gtk_widget_set_valign(scan_button, GTK_ALIGN_CENTER);
    adw_expander_row_add_suffix(ADW_EXPANDER_ROW(expander), scan_button);
    
    g_object_set_data(G_OBJECT(scan_button), "tuner-index", GUINT_TO_POINTER(i));
    g_signal_connect(scan_button, "clicked", G_CALLBACK(on_scan_clicked), self);

    // Entry for frequency
    AdwEntryRow *freq_row = ADW_ENTRY_ROW(adw_entry_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(freq_row), "Set Channel / Frequency");
    // AdwEntryRow doesn't have a placeholder property, but we can set a tooltip
    gtk_widget_set_tooltip_text(GTK_WIDGET(freq_row), "e.g. 816500000 or au-bcast:69");
    adw_entry_row_set_show_apply_button(freq_row, TRUE);
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), GTK_WIDGET(freq_row));
    g_object_set_data(G_OBJECT(freq_row), "tuner-index", GUINT_TO_POINTER(i));
    g_signal_connect(freq_row, "apply", G_CALLBACK(on_freq_applied), self);

    // Details
    AdwActionRow *ss_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(ss_row), "Signal Strength");
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), GTK_WIDGET(ss_row));
    g_object_set_data(G_OBJECT(expander), "ss-row", ss_row);

    AdwActionRow *snq_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(snq_row), "Signal Quality (SNQ)");
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), GTK_WIDGET(snq_row));
    g_object_set_data(G_OBJECT(expander), "snq-row", snq_row);

    AdwActionRow *seq_row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(seq_row), "Symbol Quality (SEQ)");
    adw_expander_row_add_row(ADW_EXPANDER_ROW(expander), GTK_WIDGET(seq_row));
    g_object_set_data(G_OBJECT(expander), "seq-row", seq_row);

    gtk_list_box_append(self->tuner_list, expander);
  }
}

typedef struct {
    HDHomeRunDeviceView *view;
    uint8_t progress;
    char *channel;
} ScanUIData;

static gboolean update_scan_ui_idle(gpointer d)
{
    ScanUIData *ud = d;
    if (G_IS_OBJECT(ud->view)) {
        gtk_progress_bar_set_fraction(ud->view->scan_progress, (double)ud->progress / 100.0);
        g_autofree char *txt = g_strdup_printf("Scanning: %s (%u%%)", ud->channel, ud->progress);
        gtk_label_set_text(ud->view->scan_label, txt);
    }
    g_free(ud->channel);
    g_free(ud);
    return G_SOURCE_REMOVE;
}

static void scan_task_func(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
  ScanTaskData *data = task_data;
  HDHomeRunDeviceView *self = data->view;
  uint32_t tuner = data->tuner_index;

  g_message("Background: Starting scan on device %08X tuner %u", self->current_device_id, tuner);

  struct hdhomerun_device_t *hd = hdhomerun_device_create_from_str(g_strdup_printf("%08X", self->current_device_id), NULL);
  if (!hd) {
    g_task_return_boolean(task, FALSE);
    return;
  }

  hdhomerun_device_set_tuner(hd, tuner);
  
  const char *scan_group = NULL;
  uint32_t selected_index = adw_combo_row_get_selected(self->region_combo);
  
  if (selected_index > 0) {
    GtkStringList *list = GTK_STRING_LIST(adw_combo_row_get_model(self->region_combo));
    scan_group = gtk_string_list_get_string(list, selected_index);
    g_message("Scan: Using user-selected region: %s", scan_group);
  } else {
    char *channelmap;
    if (hdhomerun_device_get_tuner_channelmap(hd, &channelmap) <= 0) {
      g_warning("Scan: Failed to get channelmap");
      hdhomerun_device_destroy(hd);
      g_task_return_boolean(task, FALSE);
      return;
    }
    scan_group = hdhomerun_channelmap_get_channelmap_scan_group(channelmap);
    g_message("Scan: Using device default region: %s", scan_group);
  }

  if (hdhomerun_device_channelscan_init(hd, scan_group) <= 0) {
    g_warning("Scan: Failed to init scan");
    hdhomerun_device_destroy(hd);
    g_task_return_boolean(task, FALSE);
    return;
  }

  int ret;
  struct hdhomerun_channelscan_result_t result;
  while ((ret = hdhomerun_device_channelscan_advance(hd, &result)) > 0) {
    if (g_cancellable_is_cancelled(cancellable)) break;

    uint8_t progress = hdhomerun_device_channelscan_get_progress(hd);
    g_message("Scan Progress: %u%% - %u (%s)", progress, (unsigned int)result.frequency, result.channel_str);

    // Update UI via idle
    ScanUIData *ui_data = g_malloc(sizeof(ScanUIData));
    ui_data->view = self;
    ui_data->progress = progress;
    ui_data->channel = g_strdup(result.channel_str);
    
    g_idle_add(update_scan_ui_idle, ui_data);

    hdhomerun_device_channelscan_detect(hd, &result);
  }

  hdhomerun_device_destroy(hd);
  g_task_return_boolean(task, TRUE);
}

static void on_scan_clicked(GtkButton *button, gpointer user_data)
{
  HDHomeRunDeviceView *self = user_data;
  uint32_t tuner_index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "tuner-index"));

  g_message("UI: Scan requested for tuner %u", tuner_index);
  gtk_widget_set_visible(GTK_WIDGET(self->scan_group), TRUE);
  gtk_progress_bar_set_fraction(self->scan_progress, 0.0);
  gtk_label_set_text(self->scan_label, "Initializing scan...");

  GTask *task = g_task_new(self, NULL, NULL, NULL);
  ScanTaskData *task_data = g_new0(ScanTaskData, 1);
  task_data->view = self;
  task_data->tuner_index = tuner_index;
  g_task_set_task_data(task, task_data, (GDestroyNotify)g_free);
  
  g_task_run_in_thread(task, scan_task_func);
  g_object_unref(task);
}

void hdhomerun_device_view_set_device(HDHomeRunDeviceView *self, uint32_t device_id, uint32_t tuner_count)
{
  if (self->current_device_id == device_id) return;

  g_message("UI: Switching to device %08X", device_id);

  if (self->hd) {
    hdhomerun_device_destroy(self->hd);
    self->hd = NULL;
  }

  self->current_device_id = device_id;
  self->current_tuner_count = tuner_count;
  self->hd = hdhomerun_device_create_from_str(g_strdup_printf("%08X", device_id), NULL);

  if (self->hd) {
    const char *model = hdhomerun_device_get_model_str(self->hd);
    char *version;
    hdhomerun_device_get_version(self->hd, &version, NULL);

    g_autofree char *id_str = g_strdup_printf("%08X", device_id);
    adw_action_row_set_subtitle(self->id_row, id_str);
    adw_action_row_set_subtitle(self->model_row, model ? model : "Unknown");
    adw_action_row_set_subtitle(self->firmware_row, version ? version : "Unknown");

    refresh_tuners(self, tuner_count);

    gtk_stack_set_visible_child_name(self->stack, "details");
    gtk_widget_set_visible(GTK_WIDGET(self->scan_group), FALSE);
  } else {
    gtk_stack_set_visible_child_name(self->stack, "empty");
  }
}

static void hdhomerun_device_view_dispose(GObject *object)
{
  HDHomeRunDeviceView *self = HDHOMERUN_DEVICE_VIEW(object);

  if (self->poll_id) {
    g_source_remove(self->poll_id);
    self->poll_id = 0;
  }

  if (self->hd) {
    hdhomerun_device_destroy(self->hd);
    self->hd = NULL;
  }

  G_OBJECT_CLASS(hdhomerun_device_view_parent_class)->dispose(object);
}

static void hdhomerun_device_view_class_init(HDHomeRunDeviceViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  g_type_ensure (ADW_TYPE_STATUS_PAGE);
  g_type_ensure (ADW_TYPE_CLAMP);
  g_type_ensure (ADW_TYPE_PREFERENCES_GROUP);
  g_type_ensure (ADW_TYPE_ACTION_ROW);
  g_type_ensure (ADW_TYPE_EXPANDER_ROW);
  g_type_ensure (ADW_TYPE_ENTRY_ROW);
  g_type_ensure (ADW_TYPE_COMBO_ROW);

  object_class->dispose = hdhomerun_device_view_dispose;

  gtk_widget_class_set_template_from_resource(widget_class, "/com/silicondust/HDHomeRunConfig/device-view.ui");
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, stack);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, id_row);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, model_row);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, firmware_row);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, tuner_list);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, scan_group);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, scan_progress);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, scan_label);
  gtk_widget_class_bind_template_child(widget_class, HDHomeRunDeviceView, region_combo);
}

static void hdhomerun_device_view_init(HDHomeRunDeviceView *self)
{
  gtk_widget_init_template(GTK_WIDGET(self));
  self->poll_id = g_timeout_add_seconds(1, (GSourceFunc)poll_tuner_status, self);
}
