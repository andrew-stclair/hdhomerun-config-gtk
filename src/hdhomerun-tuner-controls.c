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
#include <libhdhomerun/hdhomerun.h>
#include <errno.h>
#include <vlc/vlc.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

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
  GtkLabel *device_info_label;
  
  /* State */
  gboolean playing;
  char *device_id;
  guint tuner_index;
  struct hdhomerun_device_t *hd_device;
  
  /* VLC */
  libvlc_instance_t *vlc_instance;
  libvlc_media_player_t *vlc_player;
  libvlc_media_t *vlc_media;
};

G_DEFINE_FINAL_TYPE (HdhomerunTunerControls, hdhomerun_tuner_controls, GTK_TYPE_BOX)

static void
on_play_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  int ret;
  char *stream_url;
  uint32_t device_ip;
  char device_ip_str[16];
  
  (void)button; /* unused */
  
  if (!self->hd_device) {
    g_warning ("Cannot start playback: no device selected");
    return;
  }
  
  g_message ("Starting playback for device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Start streaming from the device */
  ret = hdhomerun_device_stream_start (self->hd_device);
  if (ret < 0) {
    g_warning ("Failed to start streaming from device %s tuner %u", 
               self->device_id ? self->device_id : "unknown", 
               self->tuner_index);
    return;
  }
  
  g_message ("Successfully started streaming from device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Get the device IP address for the stream URL */
  device_ip = hdhomerun_device_get_device_ip (self->hd_device);
  if (device_ip == 0) {
    g_warning ("Failed to get device IP address");
    hdhomerun_device_stream_stop (self->hd_device);
    return;
  }
  
  /* Convert IP address from uint32 to string */
  g_snprintf (device_ip_str, sizeof (device_ip_str), "%u.%u.%u.%u",
              (device_ip >> 24) & 0xFF,
              (device_ip >> 16) & 0xFF,
              (device_ip >> 8) & 0xFF,
              device_ip & 0xFF);
  
  /* Create stream URL: http://<device_ip>:5004/auto/v<tuner> */
  stream_url = g_strdup_printf ("http://%s:5004/auto/v%u", device_ip_str, self->tuner_index);
  g_message ("Stream URL: %s", stream_url);
  
  /* Initialize VLC if not already done */
  if (!self->vlc_instance) {
    const char *vlc_args[] = {
      "--no-xlib",  /* Don't use Xlib for X11 */
    };
    self->vlc_instance = libvlc_new (1, vlc_args);
    if (!self->vlc_instance) {
      g_warning ("Failed to initialize VLC");
      g_free (stream_url);
      hdhomerun_device_stream_stop (self->hd_device);
      return;
    }
    g_message ("VLC instance created");
  }
  
  /* Create VLC media from stream URL */
  self->vlc_media = libvlc_media_new_location (self->vlc_instance, stream_url);
  g_free (stream_url);
  
  if (!self->vlc_media) {
    g_warning ("Failed to create VLC media");
    hdhomerun_device_stream_stop (self->hd_device);
    return;
  }
  
  /* Create VLC media player if not already done */
  if (!self->vlc_player) {
    self->vlc_player = libvlc_media_player_new (self->vlc_instance);
    if (!self->vlc_player) {
      g_warning ("Failed to create VLC media player");
      libvlc_media_release (self->vlc_media);
      self->vlc_media = NULL;
      hdhomerun_device_stream_stop (self->hd_device);
      return;
    }
    g_message ("VLC media player created");
  }
  
  /* Set the media to the player */
  libvlc_media_player_set_media (self->vlc_player, self->vlc_media);
  
  /* Set up video output to the GtkDrawingArea - X11 only for now */
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self->video_preview));
  if (native) {
    GdkSurface *surface = gtk_native_get_surface (native);
    if (surface) {
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_SURFACE (surface)) {
        Window x_window = GDK_SURFACE_XID (surface);
        libvlc_media_player_set_xwindow (self->vlc_player, x_window);
        g_message ("Set VLC X11 window: %lu", (unsigned long)x_window);
      }
#endif
    }
  }
  
  /* Play the media */
  ret = libvlc_media_player_play (self->vlc_player);
  if (ret < 0) {
    g_warning ("Failed to start VLC playback");
    hdhomerun_device_stream_stop (self->hd_device);
    return;
  }
  
  g_message ("VLC playback started");
  
  self->playing = TRUE;
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), TRUE);
}

static void
on_stop_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  (void)button; /* unused */
  
  if (!self->hd_device) {
    g_warning ("Cannot stop playback: no device selected");
    return;
  }
  
  g_message ("Stopping playback for device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Stop VLC playback */
  if (self->vlc_player) {
    libvlc_media_player_stop (self->vlc_player);
    g_message ("VLC playback stopped");
  }
  
  /* Release VLC media */
  if (self->vlc_media) {
    libvlc_media_release (self->vlc_media);
    self->vlc_media = NULL;
  }
  
  /* Stop streaming from the device */
  hdhomerun_device_stream_stop (self->hd_device);
  
  g_message ("Successfully stopped streaming from device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  self->playing = FALSE;
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), FALSE);
}

static void
on_scan_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  int ret;
  
  (void)button; /* unused */
  
  if (!self->hd_device) {
    g_warning ("Cannot start channel scan: no device selected");
    return;
  }
  
  g_message ("Starting channel scan for device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Initialize channel scan with "us-bcast" channelmap (common default for US broadcast TV) */
  ret = hdhomerun_device_channelscan_init (self->hd_device, "us-bcast");
  if (ret < 0) {
    g_warning ("Failed to initialize channel scan for device %s tuner %u", 
               self->device_id ? self->device_id : "unknown", 
               self->tuner_index);
    return;
  }
  
  g_message ("Successfully started channel scan for device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Note: Full channel scan implementation would require advancing through channels
   * and detecting signals in a loop or async operation. This is a basic initialization. */
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
  int ret;
  
  (void)button; /* unused */
  
  if (!self->hd_device) {
    g_warning ("Cannot tune: no device selected");
    return;
  }
  
  frequency = gtk_editable_get_text (GTK_EDITABLE (self->frequency_entry));
  
  /* Validate frequency input to prevent format string issues */
  if (!is_valid_frequency_string (frequency)) {
    g_message ("Invalid or empty frequency entered for device %s tuner %u", 
               self->device_id ? self->device_id : "unknown", 
               self->tuner_index);
    return;
  }
  
  g_message ("Tuning to frequency: %s on device %s tuner %u", 
             frequency, 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Set the channel/frequency on the tuner */
  ret = hdhomerun_device_set_tuner_channel (self->hd_device, frequency);
  if (ret < 0) {
    g_warning ("Failed to tune to frequency %s on device %s tuner %u", 
               frequency, 
               self->device_id ? self->device_id : "unknown", 
               self->tuner_index);
    return;
  }
  
  g_message ("Successfully tuned to frequency %s on device %s tuner %u", 
             frequency, 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
}

static void
hdhomerun_tuner_controls_finalize (GObject *object)
{
  HdhomerunTunerControls *self = HDHOMERUN_TUNER_CONTROLS (object);
  
  g_message ("Cleaning up tuner controls for device %s tuner %u",
             self->device_id ? self->device_id : "unknown",
             self->tuner_index);
  
  /* Stop streaming if active */
  if (self->playing && self->hd_device) {
    hdhomerun_device_stream_stop (self->hd_device);
  }
  
  /* Clean up VLC resources */
  if (self->vlc_player) {
    libvlc_media_player_stop (self->vlc_player);
    libvlc_media_player_release (self->vlc_player);
    self->vlc_player = NULL;
    g_message ("VLC media player released");
  }
  
  if (self->vlc_media) {
    libvlc_media_release (self->vlc_media);
    self->vlc_media = NULL;
  }
  
  if (self->vlc_instance) {
    libvlc_release (self->vlc_instance);
    self->vlc_instance = NULL;
    g_message ("VLC instance released");
  }
  
  /* Destroy the device handle */
  if (self->hd_device) {
    hdhomerun_device_destroy (self->hd_device);
    self->hd_device = NULL;
  }
  
  /* Free device ID string */
  g_clear_pointer (&self->device_id, g_free);
  
  G_OBJECT_CLASS (hdhomerun_tuner_controls_parent_class)->finalize (object);
}

static void
hdhomerun_tuner_controls_class_init (HdhomerunTunerControlsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hdhomerun_tuner_controls_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/com/github/andrewstclair/HDHomeRunConfig/hdhomerun-tuner-controls.ui");
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, video_preview);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, play_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, stop_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, scan_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, channel_dropdown);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, frequency_entry);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, tune_button);
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, device_info_label);
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
  self->device_id = NULL;
  self->tuner_index = 0;
  self->hd_device = NULL;
  self->vlc_instance = NULL;
  self->vlc_player = NULL;
  self->vlc_media = NULL;
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), FALSE);
  
  /* Initially disable controls until a tuner is selected */
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->tune_button), FALSE);
}

void
hdhomerun_tuner_controls_set_tuner (HdhomerunTunerControls *self,
                                     const char *device_id,
                                     guint tuner_index)
{
  uint32_t device_id_int;
  char *label_text;
  char *endptr;
  int ret;
  
  g_return_if_fail (HDHOMERUN_IS_TUNER_CONTROLS (self));
  g_return_if_fail (device_id != NULL);
  
  g_message ("Setting tuner controls to device %s tuner %u", device_id, tuner_index);
  
  /* Clean up existing device if any */
  if (self->hd_device) {
    if (self->playing) {
      /* Stop VLC playback */
      if (self->vlc_player) {
        libvlc_media_player_stop (self->vlc_player);
      }
      if (self->vlc_media) {
        libvlc_media_release (self->vlc_media);
        self->vlc_media = NULL;
      }
      hdhomerun_device_stream_stop (self->hd_device);
      self->playing = FALSE;
      gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), FALSE);
    }
    hdhomerun_device_destroy (self->hd_device);
    self->hd_device = NULL;
  }
  
  /* Store device information */
  g_free (self->device_id);
  self->device_id = g_strdup (device_id);
  self->tuner_index = tuner_index;
  
  /* Update the device info label */
  label_text = g_strdup_printf ("Device: %s | Tuner: %u", device_id, tuner_index);
  gtk_label_set_text (self->device_info_label, label_text);
  g_free (label_text);
  
  /* Convert device ID from hex string to integer with validation */
  errno = 0;
  device_id_int = (uint32_t) strtoul (device_id, &endptr, 16);
  if (errno != 0 || *endptr != '\0' || endptr == device_id) {
    g_warning ("Invalid device ID format: %s", device_id);
    gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->tune_button), FALSE);
    return;
  }
  
  /* Create a new device handle using device ID and tuner index
   * Parameters: device_id, device_ip (0 for auto-detect), tuner, debug handle */
  self->hd_device = hdhomerun_device_create (device_id_int, 0, tuner_index, NULL);
  if (!self->hd_device) {
    g_warning ("Failed to create device handle for %s tuner %u", device_id, tuner_index);
    gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->tune_button), FALSE);
    return;
  }
  
  /* Set the tuner index on the device */
  ret = hdhomerun_device_set_tuner (self->hd_device, tuner_index);
  if (ret < 0) {
    g_warning ("Failed to set tuner %u on device %s", tuner_index, device_id);
    hdhomerun_device_destroy (self->hd_device);
    self->hd_device = NULL;
    gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (self->tune_button), FALSE);
    return;
  }
  
  g_message ("Successfully configured device %s tuner %u", device_id, tuner_index);
  
  /* Enable controls now that we have a device */
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->tune_button), TRUE);
}
