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
#include <string.h>
#include <vlc/vlc.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

/* Stream buffer size - 2MB should hold ~1 second of HD video */
#define STREAM_BUFFER_SIZE (2 * 1024 * 1024)

/* Structure to hold stream buffer data */
typedef struct {
  guint8 *data;
  gsize size;
  gsize write_pos;
  gsize read_pos;
  GMutex mutex;
} StreamBuffer;

/* Structure to hold scanned channel information */
typedef struct {
  char *channel_str;      /* Channel identifier (e.g., "5", "13.1") */
  guint32 frequency;      /* Frequency in Hz */
  guint16 program_count;  /* Number of programs found */
  char *name;             /* Channel name if available */
} ScannedChannel;

/* Structure to hold saved channel information from device lineup */
typedef struct {
  char *channel_str;      /* Channel identifier (e.g., "5.1", "13") */
  char *name;             /* Channel name (e.g., "NBC", "ABC") */
  guint32 frequency;      /* Frequency in Hz */
} SavedChannel;

/* Channel scan state */
typedef struct {
  gboolean scanning;
  guint scan_timeout_id;
  GList *found_channels;  /* List of ScannedChannel* */
  guint32 current_frequency;
  guint channels_scanned;
  guint channels_total;
  AdwDialog *scan_dialog;
  GtkLabel *scan_status_label;
  GtkProgressBar *scan_progress_bar;
  GtkButton *scan_cancel_button;
} ScanState;

struct _HdhomerunTunerControls
{
  GtkBox parent_instance;

  /* Template widgets */
  GtkDrawingArea *video_preview;
  GtkButton *play_button;
  GtkButton *stop_button;
  GtkButton *scan_button;
  GtkDropDown *channel_dropdown;
  GtkDropDown *channelmap_dropdown;
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
  
  /* UDP Streaming */
  StreamBuffer *stream_buffer;
  guint stream_timeout_id;
  
  /* Channel Scanning */
  ScanState *scan_state;
  
  /* Saved Channels */
  GtkStringList *channel_list;
  GList *saved_channels;  /* List of SavedChannel* */
};

G_DEFINE_FINAL_TYPE (HdhomerunTunerControls, hdhomerun_tuner_controls, GTK_TYPE_BOX)

/* Stream buffer functions */
static StreamBuffer *
stream_buffer_new (void)
{
  StreamBuffer *buffer = g_new0 (StreamBuffer, 1);
  buffer->data = g_malloc (STREAM_BUFFER_SIZE);
  buffer->size = STREAM_BUFFER_SIZE;
  buffer->write_pos = 0;
  buffer->read_pos = 0;
  g_mutex_init (&buffer->mutex);
  return buffer;
}

static void
stream_buffer_free (StreamBuffer *buffer)
{
  if (!buffer)
    return;
  g_mutex_clear (&buffer->mutex);
  g_free (buffer->data);
  g_free (buffer);
}

static gsize
stream_buffer_write (StreamBuffer *buffer, const guint8 *data, gsize len)
{
  gsize written = 0;
  g_mutex_lock (&buffer->mutex);
  
  while (len > 0) {
    gsize space_to_end = buffer->size - buffer->write_pos;
    gsize to_write = MIN (len, space_to_end);
    
    /* Check if buffer is full (write would overtake read) */
    gsize next_write = (buffer->write_pos + to_write) % buffer->size;
    if (buffer->write_pos < buffer->read_pos && next_write >= buffer->read_pos) {
      break; /* Buffer full */
    }
    if (buffer->write_pos >= buffer->read_pos && next_write >= buffer->read_pos && next_write < buffer->write_pos) {
      break; /* Buffer full (wrapped) */
    }
    
    memcpy (buffer->data + buffer->write_pos, data, to_write);
    buffer->write_pos = (buffer->write_pos + to_write) % buffer->size;
    data += to_write;
    len -= to_write;
    written += to_write;
  }
  
  g_mutex_unlock (&buffer->mutex);
  return written;
}

static gsize
stream_buffer_read (StreamBuffer *buffer, guint8 *data, gsize len)
{
  gsize read = 0;
  g_mutex_lock (&buffer->mutex);
  
  while (len > 0 && buffer->read_pos != buffer->write_pos) {
    gsize available_to_end = (buffer->write_pos > buffer->read_pos) ?
                             (buffer->write_pos - buffer->read_pos) :
                             (buffer->size - buffer->read_pos);
    gsize to_read = MIN (len, available_to_end);
    
    memcpy (data, buffer->data + buffer->read_pos, to_read);
    buffer->read_pos = (buffer->read_pos + to_read) % buffer->size;
    data += to_read;
    len -= to_read;
    read += to_read;
  }
  
  g_mutex_unlock (&buffer->mutex);
  return read;
}

/* Forward declarations */
static void populate_saved_channels (HdhomerunTunerControls *self);

/* Scanned channel functions */
static ScannedChannel *
scanned_channel_new (const char *channel_str, guint32 frequency, guint16 program_count, const char *name)
{
  ScannedChannel *channel = g_new0 (ScannedChannel, 1);
  channel->channel_str = g_strdup (channel_str);
  channel->frequency = frequency;
  channel->program_count = program_count;
  channel->name = name ? g_strdup (name) : NULL;
  return channel;
}

static void
scanned_channel_free (ScannedChannel *channel)
{
  if (!channel)
    return;
  g_free (channel->channel_str);
  g_free (channel->name);
  g_free (channel);
}

/* Saved channel functions */
static SavedChannel *
saved_channel_new (const char *channel_str, const char *name, guint32 frequency)
{
  SavedChannel *channel = g_new0 (SavedChannel, 1);
  channel->channel_str = g_strdup (channel_str);
  channel->name = name ? g_strdup (name) : NULL;
  channel->frequency = frequency;
  return channel;
}

static void
saved_channel_free (SavedChannel *channel)
{
  if (!channel)
    return;
  g_free (channel->channel_str);
  g_free (channel->name);
  g_free (channel);
}

static ScanState *
scan_state_new (void)
{
  ScanState *state = g_new0 (ScanState, 1);
  state->scanning = FALSE;
  state->scan_timeout_id = 0;
  state->found_channels = NULL;
  state->current_frequency = 0;
  state->channels_scanned = 0;
  state->channels_total = 0;
  state->scan_dialog = NULL;
  state->scan_status_label = NULL;
  state->scan_progress_bar = NULL;
  state->scan_cancel_button = NULL;
  return state;
}

static void
scan_state_free (ScanState *state)
{
  if (!state)
    return;
  
  if (state->scan_timeout_id > 0) {
    g_source_remove (state->scan_timeout_id);
    state->scan_timeout_id = 0;
  }
  
  g_list_free_full (state->found_channels, (GDestroyNotify)scanned_channel_free);
  
  /* Dialog widgets are owned by the dialog, don't free them */
  if (state->scan_dialog) {
    adw_dialog_close (state->scan_dialog);
  }
  
  g_free (state);
}

/* VLC imem callbacks */
static int
vlc_imem_open (void *opaque, void **datap, uint64_t *sizep)
{
  (void)opaque;
  (void)datap;
  (void)sizep;
  g_message ("VLC imem: open callback called");
  return 0;
}

static ssize_t
vlc_imem_read (void *opaque, unsigned char *buf, size_t len)
{
  HdhomerunTunerControls *self = (HdhomerunTunerControls *)opaque;
  ssize_t bytes_read;
  
  if (!self->stream_buffer || !self->playing) {
    g_debug ("VLC imem: read callback - not playing or no buffer");
    return 0;
  }
  
  bytes_read = stream_buffer_read (self->stream_buffer, buf, len);
  if (bytes_read > 0) {
    g_debug ("VLC imem: read %zd bytes (requested %zu)", bytes_read, len);
  }
  
  return bytes_read;
}

static int
vlc_imem_seek (void *opaque, uint64_t offset)
{
  (void)opaque;
  (void)offset;
  /* Seeking not supported for live streams */
  g_debug ("VLC imem: seek callback called (not supported for live streams)");
  return -1;
}

static void
vlc_imem_close (void *opaque)
{
  (void)opaque;
  g_message ("VLC imem: close callback called");
}

/* Timeout callback to receive stream data */
static gboolean
stream_recv_timeout (gpointer user_data)
{
  HdhomerunTunerControls *self = HDHOMERUN_TUNER_CONTROLS (user_data);
  size_t actual_size;
  uint8_t *data;
  
  if (!self->playing || !self->hd_device || !self->stream_buffer) {
    g_message ("Stream receive timeout: stopping (playing=%d, hd_device=%p, stream_buffer=%p)",
               self->playing, (void*)self->hd_device, (void*)self->stream_buffer);
    return G_SOURCE_REMOVE;
  }
  
  /* Receive data from HDHomeRun device */
  data = hdhomerun_device_stream_recv (self->hd_device, 
                                        VIDEO_DATA_BUFFER_SIZE_1S / 20, /* ~50ms worth */
                                        &actual_size);
  
  if (data && actual_size > 0) {
    gsize written = stream_buffer_write (self->stream_buffer, data, actual_size);
    g_debug ("Stream receive: got %zu bytes, wrote %zu bytes to buffer", actual_size, written);
    if (written < actual_size) {
      g_warning ("Stream buffer full, dropped %zu bytes", actual_size - written);
    }
  } else {
    g_debug ("Stream receive: no data received (data=%p, size=%zu)", (void*)data, actual_size);
  }
  
  return G_SOURCE_CONTINUE;
}

/* Channel scanning functions */
static void
stop_channel_scan (HdhomerunTunerControls *self)
{
  if (!self->scan_state)
    return;
  
  g_message ("Stopping channel scan for device %s tuner %u",
             self->device_id ? self->device_id : "unknown",
             self->tuner_index);
  
  self->scan_state->scanning = FALSE;
  
  if (self->scan_state->scan_timeout_id > 0) {
    g_message ("Removing scan timeout (ID: %u)", self->scan_state->scan_timeout_id);
    g_source_remove (self->scan_state->scan_timeout_id);
    self->scan_state->scan_timeout_id = 0;
  }
  
  if (self->scan_state->scan_dialog) {
    g_message ("Closing scan dialog");
    adw_dialog_close (self->scan_state->scan_dialog);
    self->scan_state->scan_dialog = NULL;
  }
  
  /* Re-enable scan button */
  gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), TRUE);
  g_message ("Re-enabled scan button");
}

static void
on_scan_cancel_clicked (GtkButton *button, HdhomerunTunerControls *self)
{
  (void)button;
  g_message ("Scan cancel button clicked");
  stop_channel_scan (self);
}

static gboolean
scan_advance_timeout (gpointer user_data)
{
  HdhomerunTunerControls *self = (HdhomerunTunerControls *)user_data;
  struct hdhomerun_channelscan_result_t result;
  int ret;
  
  if (!self->scan_state || !self->scan_state->scanning) {
    g_message ("Scan advance: stopping (scan_state=%p, scanning=%d)",
               (void*)self->scan_state,
               self->scan_state ? self->scan_state->scanning : 0);
    return G_SOURCE_REMOVE;
  }
  
  g_debug ("Scan advance: calling hdhomerun_device_channelscan_advance");
  
  /* Advance to next channel */
  ret = hdhomerun_device_channelscan_advance (self->hd_device, &result);
  
  if (ret <= 0) {
    /* Scan complete or error */
    g_message ("Channel scan complete: found %u channels (ret=%d)", 
               g_list_length (self->scan_state->found_channels), ret);
    
    if (self->scan_state->scan_status_label) {
      char *status_text = g_strdup_printf ("Scan complete! Found %u channel(s)", 
                                            g_list_length (self->scan_state->found_channels));
      gtk_label_set_label (self->scan_state->scan_status_label, status_text);
      g_message ("Updated scan status: %s", status_text);
      g_free (status_text);
    }
    
    if (self->scan_state->scan_progress_bar) {
      gtk_progress_bar_set_fraction (self->scan_state->scan_progress_bar, 1.0);
      g_message ("Set progress bar to 100%%");
    }
    
    if (self->scan_state->scan_cancel_button) {
      gtk_button_set_label (self->scan_state->scan_cancel_button, "Close");
      g_message ("Changed cancel button to Close");
    }
    
    self->scan_state->scanning = FALSE;
    self->scan_state->scan_timeout_id = 0;
    
    /* Re-enable scan button */
    gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), TRUE);
    g_message ("Re-enabled scan button");
    
    /* Populate saved channels dropdown from scan results */
    g_message ("Populating saved channels from scan results");
    populate_saved_channels (self);
    
    return G_SOURCE_REMOVE;
  }
  
  /* Update progress */
  self->scan_state->current_frequency = result.frequency;
  self->scan_state->channels_scanned++;
  
  g_debug ("Scan advance: frequency=%u, scanned=%u/%u",
           result.frequency,
           self->scan_state->channels_scanned,
           self->scan_state->channels_total);
  
  if (self->scan_state->scan_status_label) {
    char *status_text = g_strdup_printf ("Scanning frequency %u MHz...", 
                                          result.frequency / 1000000);
    gtk_label_set_label (self->scan_state->scan_status_label, status_text);
    g_free (status_text);
  }
  
  if (self->scan_state->scan_progress_bar && self->scan_state->channels_total > 0) {
    double fraction = (double)self->scan_state->channels_scanned / self->scan_state->channels_total;
    gtk_progress_bar_set_fraction (self->scan_state->scan_progress_bar, fraction);
  }
  
  /* Detect programs on this channel */
  g_debug ("Scan advance: calling hdhomerun_device_channelscan_detect");
  ret = hdhomerun_device_channelscan_detect (self->hd_device, &result);
  g_debug ("Scan detect: returned %d, program_count=%u", ret, result.program_count);
  
  if (ret > 0 && result.program_count > 0) {
    g_message ("Found %u program(s) on frequency %u (channel %s)", 
               result.program_count, result.frequency, result.channel_str);
    
    /* Iterate through all programs and save each virtual channel */
    for (guint i = 0; i < result.program_count; i++) {
      struct hdhomerun_channelscan_program_t *program = &result.programs[i];
      const char *vchannel = program->program_str;
      
      /* Skip if no program string */
      if (!vchannel || vchannel[0] == '\0') {
        g_debug ("Skipping program %u with no program_str", i);
        continue;
      }
      
      g_message ("Found virtual channel '%s' (program %u/%u)", 
                 vchannel, i + 1, result.program_count);
      
      /* Store each virtual channel separately */
      ScannedChannel *channel = scanned_channel_new (vchannel, 
                                                       result.frequency,
                                                       1,  /* Each entry is for 1 program */
                                                       program->name[0] ? program->name : NULL);
      self->scan_state->found_channels = g_list_append (self->scan_state->found_channels, channel);
      g_message ("Added virtual channel %s to found_channels list", vchannel);
    }
    
    /* Update status to show found channels */
    if (self->scan_state->scan_status_label && result.program_count > 0) {
      struct hdhomerun_channelscan_program_t *first_program = &result.programs[0];
      char *status_text;
      if (result.program_count == 1) {
        status_text = g_strdup_printf ("Found channel %s", first_program->program_str);
      } else {
        status_text = g_strdup_printf ("Found %u channels (%s, ...)", 
                                        result.program_count,
                                        first_program->program_str);
      }
      gtk_label_set_label (self->scan_state->scan_status_label, status_text);
      g_free (status_text);
    }
  }
  
  /* Continue scanning - call again immediately */
  g_debug ("Scan advance: scheduling next iteration");
  g_idle_add (scan_advance_timeout, self);
  return G_SOURCE_REMOVE;
}

static void
on_play_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  int ret;
  
  (void)button; /* unused */
  
  if (!self->hd_device) {
    g_warning ("Cannot start playback: no device selected");
    return;
  }
  
  if (self->playing) {
    g_warning ("Already playing");
    return;
  }
  
  g_message ("Starting UDP streaming playback for device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Create stream buffer */
  if (!self->stream_buffer) {
    self->stream_buffer = stream_buffer_new ();
    g_message ("Created stream buffer (%zu bytes)", (gsize)STREAM_BUFFER_SIZE);
  }
  
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
  
  /* Initialize VLC if not already done */
  if (!self->vlc_instance) {
    g_message ("Creating VLC instance");
    self->vlc_instance = libvlc_new (0, NULL);
    if (!self->vlc_instance) {
      const char *vlc_error = libvlc_errmsg ();
      g_warning ("Failed to initialize VLC: %s", vlc_error ? vlc_error : "unknown error");
      hdhomerun_device_stream_stop (self->hd_device);
      return;
    }
    g_message ("VLC instance created successfully");
  }
  
  /* Create VLC media using imem (memory input) with TS demux */
  const char *media_options[] = {
    ":demux=ts",
    ":no-audio"
  };
  
  g_message ("Creating VLC media with imem callbacks");
  self->vlc_media = libvlc_media_new_callbacks (self->vlc_instance,
                                                 vlc_imem_open,
                                                 vlc_imem_read,
                                                 vlc_imem_seek,
                                                 vlc_imem_close,
                                                 self);
  
  if (!self->vlc_media) {
    const char *vlc_error = libvlc_errmsg ();
    g_warning ("Failed to create VLC media with imem: %s", vlc_error ? vlc_error : "unknown error");
    hdhomerun_device_stream_stop (self->hd_device);
    return;
  }
  g_message ("VLC media created successfully");
  
  /* Add media options for TS demux */
  g_message ("Adding media options: demux=ts, no-audio");
  for (size_t i = 0; i < sizeof(media_options) / sizeof(media_options[0]); i++) {
    libvlc_media_add_option (self->vlc_media, media_options[i]);
    g_message ("Added media option: %s", media_options[i]);
  }
  
  g_message ("VLC media configured with imem callbacks and TS demux options");
  
  /* Create VLC media player if not already done */
  if (!self->vlc_player) {
    g_message ("Creating VLC media player");
    self->vlc_player = libvlc_media_player_new (self->vlc_instance);
    if (!self->vlc_player) {
      const char *vlc_error = libvlc_errmsg ();
      g_warning ("Failed to create VLC media player: %s", vlc_error ? vlc_error : "unknown error");
      libvlc_media_release (self->vlc_media);
      self->vlc_media = NULL;
      hdhomerun_device_stream_stop (self->hd_device);
      return;
    }
    g_message ("VLC media player created successfully");
  }
  
  /* Set the media to the player */
  g_message ("Setting media to player");
  libvlc_media_player_set_media (self->vlc_player, self->vlc_media);
  
  /* Set up video output to the GtkDrawingArea - X11 only for now */
  g_message ("Setting up video output");
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self->video_preview));
  if (native) {
    GdkSurface *surface = gtk_native_get_surface (native);
    if (surface) {
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_SURFACE (surface)) {
        Window x_window = GDK_SURFACE_XID (surface);
        libvlc_media_player_set_xwindow (self->vlc_player, x_window);
        g_message ("Set VLC X11 window: %lu", (unsigned long)x_window);
      } else {
        g_warning ("Surface is not X11, VLC output may not work");
      }
#else
      g_warning ("X11 support not compiled, VLC output may not work");
#endif
    } else {
      g_warning ("Failed to get GdkSurface from native");
    }
  } else {
    g_warning ("Failed to get GtkNative from video preview widget");
  }
  
  /* Mark as playing before starting playback */
  g_message ("Marking as playing and starting stream receive timeout");
  self->playing = TRUE;
  
  /* Start the timeout to receive stream data (every 50ms) */
  self->stream_timeout_id = g_timeout_add (50, stream_recv_timeout, self);
  g_message ("Started stream receive timeout (ID: %u)", self->stream_timeout_id);
  
  /* Play the media */
  g_message ("Calling libvlc_media_player_play");
  ret = libvlc_media_player_play (self->vlc_player);
  if (ret < 0) {
    const char *vlc_error = libvlc_errmsg ();
    g_warning ("Failed to start VLC playback (ret=%d): %s", ret, vlc_error ? vlc_error : "unknown error");
    self->playing = FALSE;
    if (self->stream_timeout_id > 0) {
      g_source_remove (self->stream_timeout_id);
      self->stream_timeout_id = 0;
    }
    hdhomerun_device_stream_stop (self->hd_device);
    return;
  }
  
  g_message ("VLC playback started successfully with UDP streaming");
  
  g_message ("Updating UI: disabling play button, enabling stop button");
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
  
  /* Mark as not playing */
  self->playing = FALSE;
  g_message ("Marked as not playing");
  
  /* Remove the timeout */
  if (self->stream_timeout_id > 0) {
    g_message ("Removing stream receive timeout (ID: %u)", self->stream_timeout_id);
    g_source_remove (self->stream_timeout_id);
    self->stream_timeout_id = 0;
    g_message ("Removed stream receive timeout");
  }
  
  /* Stop VLC playback */
  if (self->vlc_player) {
    g_message ("Stopping VLC playback");
    libvlc_media_player_stop (self->vlc_player);
    g_message ("VLC playback stopped");
  }
  
  /* Release VLC media */
  if (self->vlc_media) {
    g_message ("Releasing VLC media");
    libvlc_media_release (self->vlc_media);
    self->vlc_media = NULL;
    g_message ("VLC media released");
  }
  
  /* Stop streaming from the device and flush buffer */
  g_message ("Flushing and stopping HDHomeRun device stream");
  hdhomerun_device_stream_flush (self->hd_device);
  hdhomerun_device_stream_stop (self->hd_device);
  
  g_message ("Successfully stopped streaming from device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  g_message ("Updating UI: enabling play button, disabling stop button");
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), FALSE);
}

static void
on_scan_clicked (GtkButton *button,
                 HdhomerunTunerControls *self)
{
  int ret;
  GtkWidget *content_box;
  GtkWidget *status_label;
  GtkWidget *progress_bar;
  GtkWidget *cancel_button;
  
  (void)button; /* unused */
  
  if (!self->hd_device) {
    g_warning ("Cannot start channel scan: no device selected");
    return;
  }
  
  /* Check if already scanning */
  if (self->scan_state && self->scan_state->scanning) {
    g_warning ("Channel scan already in progress");
    return;
  }
  
  g_message ("Starting channel scan for device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Initialize scan state if needed */
  if (!self->scan_state) {
    g_message ("Creating new scan state");
    self->scan_state = scan_state_new ();
  }
  
  /* Clear any previous scan results */
  g_message ("Clearing previous scan results");
  g_list_free_full (self->scan_state->found_channels, (GDestroyNotify)scanned_channel_free);
  self->scan_state->found_channels = NULL;
  self->scan_state->channels_scanned = 0;
  
  /* Get selected channelmap from dropdown */
  guint selected = gtk_drop_down_get_selected (self->channelmap_dropdown);
  const char *channelmap;
  guint estimated_channels;
  
  switch (selected) {
    case 0: /* United States - Broadcast */
      channelmap = "us-bcast";
      estimated_channels = 69;
      break;
    case 1: /* United States - Cable */
      channelmap = "us-cable";
      estimated_channels = 135;
      break;
    case 2: /* European Union - Broadcast */
      channelmap = "eu-bcast";
      estimated_channels = 69;
      break;
    case 3: /* European Union - Cable */
      channelmap = "eu-cable";
      estimated_channels = 135;
      break;
    case 4: /* Australia - Broadcast */
      channelmap = "au-bcast";
      estimated_channels = 69;
      break;
    case 5: /* Australia - Cable */
      channelmap = "au-cable";
      estimated_channels = 135;
      break;
    default:
      channelmap = "us-bcast";
      estimated_channels = 69;
      break;
  }
  
  g_message ("Initializing channel scan with channelmap '%s'", channelmap);
  ret = hdhomerun_device_channelscan_init (self->hd_device, channelmap);
  if (ret < 0) {
    g_warning ("Failed to initialize channel scan for device %s tuner %u (ret=%d)", 
               self->device_id ? self->device_id : "unknown", 
               self->tuner_index, ret);
    return;
  }
  
  /* Set estimated total channels based on channelmap */
  self->scan_state->channels_total = estimated_channels;
  g_message ("Estimated %u total channels to scan", self->scan_state->channels_total);
  
  /* Create scan progress dialog */
  g_message ("Creating scan progress dialog");
  AdwDialog *dialog = adw_dialog_new ();
  adw_dialog_set_title (dialog, "Channel Scan");
  
  /* Create content */
  content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_start (content_box, 24);
  gtk_widget_set_margin_end (content_box, 24);
  gtk_widget_set_margin_top (content_box, 24);
  gtk_widget_set_margin_bottom (content_box, 24);
  
  /* Status label */
  status_label = gtk_label_new ("Initializing scan...");
  gtk_widget_add_css_class (status_label, "title-4");
  gtk_box_append (GTK_BOX (content_box), status_label);
  
  /* Progress bar */
  progress_bar = gtk_progress_bar_new ();
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (progress_bar), TRUE);
  gtk_widget_set_size_request (progress_bar, 300, -1);
  gtk_box_append (GTK_BOX (content_box), progress_bar);
  
  /* Cancel button */
  cancel_button = gtk_button_new_with_label ("Cancel");
  gtk_widget_set_halign (cancel_button, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (cancel_button, "pill");
  g_signal_connect (cancel_button, "clicked", G_CALLBACK (on_scan_cancel_clicked), self);
  gtk_box_append (GTK_BOX (content_box), cancel_button);
  
  adw_dialog_set_child (dialog, content_box);
  g_message ("Scan dialog UI created");
  
  /* Store dialog and widgets in scan state */
  self->scan_state->scan_dialog = dialog;
  self->scan_state->scan_status_label = GTK_LABEL (status_label);
  self->scan_state->scan_progress_bar = GTK_PROGRESS_BAR (progress_bar);
  self->scan_state->scan_cancel_button = GTK_BUTTON (cancel_button);
  self->scan_state->scanning = TRUE;
  g_message ("Stored dialog references in scan state");
  
  /* Present the dialog */
  g_message ("Presenting scan dialog");
  GtkWidget *root = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_APPLICATION_WINDOW);
  if (root) {
    adw_dialog_present (dialog, GTK_WIDGET (root));
    g_message ("Dialog presented");
  } else {
    g_warning ("Failed to find root window for dialog");
  }
  
  g_message ("Successfully started channel scan for device %s tuner %u", 
             self->device_id ? self->device_id : "unknown", 
             self->tuner_index);
  
  /* Disable scan button while scanning */
  gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), FALSE);
  g_message ("Disabled scan button while scanning");
  
  /* Start the scan loop - call once to begin, it will reschedule itself */
  g_message ("Starting scan loop with g_idle_add");
  self->scan_state->scan_timeout_id = g_idle_add (scan_advance_timeout, self);
  g_message ("Scan loop started with ID: %u", self->scan_state->scan_timeout_id);
}

static gboolean
is_valid_frequency_string (const char *str)
{
  if (!str || !*str)
    return FALSE;
  
  /* Check that the string contains valid channel/frequency characters
   * Allow digits, decimal point, whitespace, and hyphens for channel specs like "2.1" or "2-1" */
  for (const char *p = str; *p; p++) {
    if (!g_ascii_isdigit (*p) && *p != '.' && *p != ' ' && *p != '-')
      return FALSE;
  }
  
  return TRUE;
}

/* Populate saved channels dropdown from scan results */
static void
populate_saved_channels (HdhomerunTunerControls *self)
{
  GList *iter;
  
  g_return_if_fail (HDHOMERUN_IS_TUNER_CONTROLS (self));
  
  g_message ("Populating saved channels dropdown");
  
  /* Clear existing saved channels */
  g_message ("Clearing existing saved channels list");
  g_list_free_full (self->saved_channels, (GDestroyNotify)saved_channel_free);
  self->saved_channels = NULL;
  
  /* Clear dropdown */
  if (self->channel_list) {
    g_message ("Clearing existing channel dropdown list");
    g_object_unref (self->channel_list);
  }
  self->channel_list = gtk_string_list_new (NULL);
  
  /* If we have scan results, populate from them */
  if (self->scan_state && self->scan_state->found_channels) {
    guint channel_count = g_list_length (self->scan_state->found_channels);
    g_message ("Populating from %u scanned channels", channel_count);
    
    for (iter = self->scan_state->found_channels; iter != NULL; iter = iter->next) {
      ScannedChannel *scanned = (ScannedChannel *)iter->data;
      SavedChannel *saved;
      char *display_name;
      
      g_debug ("Adding channel: %s (freq: %u, programs: %u)", 
               scanned->channel_str, scanned->frequency, scanned->program_count);
      
      /* Create saved channel from scanned channel */
      saved = saved_channel_new (scanned->channel_str, scanned->name, scanned->frequency);
      self->saved_channels = g_list_append (self->saved_channels, saved);
      
      /* Add to dropdown */
      if (scanned->name && scanned->name[0] != '\0') {
        display_name = g_strdup_printf ("%s - %s", scanned->channel_str, scanned->name);
      } else {
        display_name = g_strdup_printf ("Channel %s", scanned->channel_str);
      }
      gtk_string_list_append (self->channel_list, display_name);
      g_debug ("Added to dropdown: %s", display_name);
      g_free (display_name);
    }
    
    g_message ("Populated %u saved channels for device %s tuner %u",
               g_list_length (self->saved_channels),
               self->device_id ? self->device_id : "unknown",
               self->tuner_index);
  } else {
    g_message ("No scan results available to populate");
  }
  
  /* Set the model on the dropdown */
  g_message ("Setting model on dropdown");
  gtk_drop_down_set_model (self->channel_dropdown, G_LIST_MODEL (self->channel_list));
  
  /* Enable dropdown if we have channels */
  gboolean has_channels = g_list_length (self->saved_channels) > 0;
  gtk_widget_set_sensitive (GTK_WIDGET (self->channel_dropdown), has_channels);
  g_message ("Dropdown %s (has %u channels)",
             has_channels ? "enabled" : "disabled",
             g_list_length (self->saved_channels));
}

/* Handle channel selection from dropdown */
static void
on_channel_selected (GtkDropDown *dropdown,
                     GParamSpec *pspec,
                     HdhomerunTunerControls *self)
{
  guint selected;
  SavedChannel *channel;
  int ret;
  
  (void)pspec; /* unused */
  
  if (!self->hd_device) {
    g_warning ("Cannot tune: no device selected");
    return;
  }
  
  selected = gtk_drop_down_get_selected (dropdown);
  if (selected == GTK_INVALID_LIST_POSITION) {
    g_debug ("Channel selection: no channel selected");
    return;
  }
  
  g_message ("Channel dropdown selection changed to index %u", selected);
  
  /* Get the corresponding saved channel */
  channel = (SavedChannel *)g_list_nth_data (self->saved_channels, selected);
  if (!channel) {
    g_warning ("Failed to get saved channel at index %u", selected);
    return;
  }
  
  g_message ("Tuning to saved channel %s (freq: %u) on device %s tuner %u",
             channel->channel_str, channel->frequency,
             self->device_id ? self->device_id : "unknown",
             self->tuner_index);
  
  /* Set the channel/frequency on the tuner */
  ret = hdhomerun_device_set_tuner_channel (self->hd_device, channel->channel_str);
  if (ret < 0) {
    g_warning ("Failed to tune to channel %s on device %s tuner %u (ret=%d)",
               channel->channel_str,
               self->device_id ? self->device_id : "unknown",
               self->tuner_index);
    return;
  }
  
  g_message ("Successfully tuned to channel %s on device %s tuner %u",
             channel->channel_str,
             self->device_id ? self->device_id : "unknown",
             self->tuner_index);
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
  
  /* Mark as not playing and remove timeout */
  self->playing = FALSE;
  if (self->stream_timeout_id > 0) {
    g_source_remove (self->stream_timeout_id);
    self->stream_timeout_id = 0;
  }
  
  /* Stop streaming if active */
  if (self->hd_device) {
    hdhomerun_device_stream_flush (self->hd_device);
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
  
  /* Clean up stream buffer */
  if (self->stream_buffer) {
    stream_buffer_free (self->stream_buffer);
    self->stream_buffer = NULL;
  }
  
  /* Clean up scan state */
  if (self->scan_state) {
    scan_state_free (self->scan_state);
    self->scan_state = NULL;
  }
  
  /* Clean up saved channels */
  g_list_free_full (self->saved_channels, (GDestroyNotify)saved_channel_free);
  self->saved_channels = NULL;
  
  if (self->channel_list) {
    g_object_unref (self->channel_list);
    self->channel_list = NULL;
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
  gtk_widget_class_bind_template_child (widget_class, HdhomerunTunerControls, channelmap_dropdown);
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
  self->stream_buffer = NULL;
  self->stream_timeout_id = 0;
  self->scan_state = NULL;
  self->channel_list = NULL;
  self->saved_channels = NULL;
  gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button), FALSE);
  
  /* Initially disable controls until a tuner is selected */
  gtk_widget_set_sensitive (GTK_WIDGET (self->play_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->scan_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->tune_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->channel_dropdown), FALSE);
  
  /* Set up channel map dropdown with user-friendly names */
  const char *channelmap_names[] = {
    "United States - Broadcast",
    "United States - Cable",
    "European Union - Broadcast",
    "European Union - Cable",
    "Australia - Broadcast",
    "Australia - Cable",
    NULL
  };
  
  GtkStringList *channelmap_list = gtk_string_list_new (channelmap_names);
  gtk_drop_down_set_model (self->channelmap_dropdown, G_LIST_MODEL (channelmap_list));
  gtk_drop_down_set_selected (self->channelmap_dropdown, 0); /* Default to US Broadcast */
  g_object_unref (channelmap_list);
  
  /* Connect dropdown selection signal */
  g_signal_connect (self->channel_dropdown, "notify::selected",
                    G_CALLBACK (on_channel_selected), self);
}

void
hdhomerun_tuner_controls_set_tuner (HdhomerunTunerControls *self,
                                     const char *device_id,
                                     guint tuner_index)
{
  uint32_t device_id_int;
  char *label_text;
  char *endptr;
  
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
   * Parameters: device_id, device_ip (0 for auto-detect), tuner, debug handle
   * We pass the tuner index here so we don't need a separate set_tuner call */
  self->hd_device = hdhomerun_device_create (device_id_int, 0, tuner_index, NULL);
  if (!self->hd_device) {
    g_warning ("Failed to create device handle for %s tuner %u", device_id, tuner_index);
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
