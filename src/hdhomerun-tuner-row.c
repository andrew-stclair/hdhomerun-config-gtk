/* hdhomerun-tuner-row.c
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

#include "hdhomerun-tuner-row.h"

struct _HdhomerunTunerRow
{
  AdwActionRow parent_instance;

  char *device_id;
  unsigned int tuner_index;
};

G_DEFINE_FINAL_TYPE (HdhomerunTunerRow, hdhomerun_tuner_row, ADW_TYPE_ACTION_ROW)

enum {
  PROP_0,
  PROP_DEVICE_ID,
  PROP_TUNER_INDEX,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

HdhomerunTunerRow *
hdhomerun_tuner_row_new (const char *device_id,
                         unsigned int tuner_index)
{
  return g_object_new (HDHOMERUN_TYPE_TUNER_ROW,
                       "device-id", device_id,
                       "tuner-index", tuner_index,
                       NULL);
}

static void
hdhomerun_tuner_row_finalize (GObject *object)
{
  HdhomerunTunerRow *self = (HdhomerunTunerRow *)object;

  g_clear_pointer (&self->device_id, g_free);

  G_OBJECT_CLASS (hdhomerun_tuner_row_parent_class)->finalize (object);
}

static void
hdhomerun_tuner_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  HdhomerunTunerRow *self = HDHOMERUN_TUNER_ROW (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_string (value, self->device_id);
      break;
    case PROP_TUNER_INDEX:
      g_value_set_uint (value, self->tuner_index);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hdhomerun_tuner_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HdhomerunTunerRow *self = HDHOMERUN_TUNER_ROW (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_free (self->device_id);
      self->device_id = g_value_dup_string (value);
      /* Update title when device_id changes */
      if (self->device_id)
        {
          char *title = g_strdup_printf ("%s - Tuner %u", self->device_id, self->tuner_index);
          adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);
          g_free (title);
        }
      break;
    case PROP_TUNER_INDEX:
      self->tuner_index = g_value_get_uint (value);
      /* Update title when tuner_index changes */
      if (self->device_id)
        {
          char *title = g_strdup_printf ("%s - Tuner %u", self->device_id, self->tuner_index);
          adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title);
          g_free (title);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hdhomerun_tuner_row_class_init (HdhomerunTunerRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hdhomerun_tuner_row_finalize;
  object_class->get_property = hdhomerun_tuner_row_get_property;
  object_class->set_property = hdhomerun_tuner_row_set_property;

  properties [PROP_DEVICE_ID] =
    g_param_spec_string ("device-id",
                         "Device ID",
                         "The device ID",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_TUNER_INDEX] =
    g_param_spec_uint ("tuner-index",
                       "Tuner Index",
                       "The tuner index",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
hdhomerun_tuner_row_init (HdhomerunTunerRow *self)
{
  GtkWidget *icon;
  
  /* Add a chevron icon to make the row visually and functionally activatable */
  icon = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (self), icon);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (self), icon);
}
