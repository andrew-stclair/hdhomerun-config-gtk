/* hdhomerun-device-row.c
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

#include "hdhomerun-device-row.h"

struct _HdhomerunDeviceRow
{
  AdwExpanderRow parent_instance;

  char *device_id;
  char *model_name;
  char *ip_address;
};

G_DEFINE_FINAL_TYPE (HdhomerunDeviceRow, hdhomerun_device_row, ADW_TYPE_EXPANDER_ROW)

enum {
  PROP_0,
  PROP_DEVICE_ID,
  PROP_MODEL_NAME,
  PROP_IP_ADDRESS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

HdhomerunDeviceRow *
hdhomerun_device_row_new (const char *device_id,
                          const char *model_name,
                          const char *ip_address)
{
  return g_object_new (HDHOMERUN_TYPE_DEVICE_ROW,
                       "device-id", device_id,
                       "model-name", model_name,
                       "ip-address", ip_address,
                       NULL);
}

static void
hdhomerun_device_row_finalize (GObject *object)
{
  HdhomerunDeviceRow *self = (HdhomerunDeviceRow *)object;

  g_clear_pointer (&self->device_id, g_free);
  g_clear_pointer (&self->model_name, g_free);
  g_clear_pointer (&self->ip_address, g_free);

  G_OBJECT_CLASS (hdhomerun_device_row_parent_class)->finalize (object);
}

static void
hdhomerun_device_row_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  HdhomerunDeviceRow *self = HDHOMERUN_DEVICE_ROW (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_string (value, self->device_id);
      break;
    case PROP_MODEL_NAME:
      g_value_set_string (value, self->model_name);
      break;
    case PROP_IP_ADDRESS:
      g_value_set_string (value, self->ip_address);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hdhomerun_device_row_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HdhomerunDeviceRow *self = HDHOMERUN_DEVICE_ROW (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_free (self->device_id);
      self->device_id = g_value_dup_string (value);
      break;
    case PROP_MODEL_NAME:
      g_free (self->model_name);
      self->model_name = g_value_dup_string (value);
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), self->model_name);
      break;
    case PROP_IP_ADDRESS:
      g_free (self->ip_address);
      self->ip_address = g_value_dup_string (value);
      adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (self), self->ip_address);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hdhomerun_device_row_class_init (HdhomerunDeviceRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hdhomerun_device_row_finalize;
  object_class->get_property = hdhomerun_device_row_get_property;
  object_class->set_property = hdhomerun_device_row_set_property;

  properties [PROP_DEVICE_ID] =
    g_param_spec_string ("device-id",
                         "Device ID",
                         "The device ID",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_MODEL_NAME] =
    g_param_spec_string ("model-name",
                         "Model Name",
                         "The model name",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_IP_ADDRESS] =
    g_param_spec_string ("ip-address",
                         "IP Address",
                         "The IP address",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
hdhomerun_device_row_init (HdhomerunDeviceRow *self)
{
  adw_expander_row_set_show_enable_switch (ADW_EXPANDER_ROW (self), FALSE);
}
