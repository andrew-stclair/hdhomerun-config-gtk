/* hdhomerun-tuner-controls.c
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

#include "hdhomerun-tuner-controls.h"
#include <glib/gi18n.h>

struct _HdhomerunTunerControls
{
  GtkBox parent_instance;

  /* Template widgets */
  GtkDrawingArea *video_preview;
  GtkButton *play_button;
  GtkButton *stop_button;
  GtkButton *scan_button;
  GtkDropDown *channel_dropdown;
  GtkEntry *frequency_entry;
  GtkButton *tune_button;
  
  /* State */
  gboolean playing;
};

G_DEFINE_FINAL_TYPE (HdhomerunTunerControls, hdhomerun_tuner_controls, GTK_TYPE_BOX)

static void
on_play_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  (void)button; /* unused */
  
  self->playing = TRUE;
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), TRUE);
  g_message ("Starting playback");
}

static void
on_stop_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  (void)button; /* unused */
  
  self->playing = FALSE;
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), FALSE);
  g_message ("Stopping playback");
}

static void
on_scan_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  (void)button; /* unused */
  (void)self; /* unused */
  
  g_message ("Starting channel scan");
}

static gboolean
is_valid_frequency_string (const char *str)
{
  if (!str || !*str)
    return FALSE;
  
  /* Check that the string contains only digits, decimal point, and whitespace */
  for (const char *p = str; *p; p++) {
    if (!g_ascii_isdigit (*p) && *p != '.' && *p != ' ')
      return FALSE;
  }
  
  return TRUE;
}

static void
on_tune_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  const char *frequency;
  
  (void)button; /* unused */
  
  frequency = gtk_editable_get_text (GTK_EDITABLE (self->frequency_entry));
  
  /* Validate frequency input to prevent format string issues */
  if (is_valid_frequency_string (frequency)) {
    g_message ("Tuning to frequency: %s", frequency);
  } else {
    g_message ("Invalid or empty frequency entered");
  }
}

static void
hdhomerun_tuner_controls_class_init (HdhomerunTunerControlsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/andrewstclair/HDHomeRunConfig/hdhomerun-tuner-controls.ui");
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, video_preview);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, play_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, stop_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, scan_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, channel_dropdown);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, frequency_entry);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, tune_button);
  gtk_widget_class_bind_template_callback (widget_class, on_play_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_stop_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_scan_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_tune_clicked);
}

static void
hdhomerun_tuner_controls_init (HdhomerunTunerControls *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  
  self->playing = FALSE;
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), FALSE);
}
