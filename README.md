# HDHomeRun Config GTK

A modern GNOME application for configuring and previewing HDHomeRun network TV tuners.

![CI](https://github.com/andrew-stclair/hdhomerun-config-gtk/workflows/CI/badge.svg)

## Features

- **Automatic Device Discovery**: Automatically finds HDHomeRun devices on your local network
- **Live TV Preview**: Watch live TV from your tuners using integrated video playback
- **Channel Management**: Save favorite channels and easily switch between them
- **Channel Scanning**: Search for available channels automatically
- **Manual Tuning**: Tune to specific frequencies manually
- **Modern UI**: Built with GTK4 and libadwaita for a beautiful, responsive interface
- **Mobile-Friendly**: Adaptive design works great on both desktop and mobile devices

## Screenshots

Coming soon!

## Building

### Dependencies

- GTK 4.10 or later
- libadwaita 1.4 or later
- GLib 2.76 or later
- Meson build system
- libvlc (optional, for video preview)
- libhdhomerun (optional, for device discovery)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/andrew-stclair/hdhomerun-config-gtk.git
cd hdhomerun-config-gtk

# Setup the build directory
meson setup builddir

# Compile the project
meson compile -C builddir

# Install (optional)
meson install -C builddir
```

### Running

After building, you can run the application directly:

```bash
./builddir/src/hdhomerun-config-gtk
```

Or if you installed it:

```bash
hdhomerun-config-gtk
```

## Development

### Project Structure

- `src/` - Source code
  - `main.c` - Application entry point
  - `hdhomerun-application.[ch]` - Main application class
  - `hdhomerun-window.[ch]` - Main window
  - `hdhomerun-device-row.[ch]` - Device list row widget
  - `hdhomerun-tuner-controls.[ch]` - Tuner control panel
- `data/` - Application data files
  - Desktop file
  - AppStream metadata
  - GSettings schema
  - Icons
- `po/` - Translations

### Testing

```bash
meson test -C builddir
```

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the GNU General Public License v2.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Built with [GTK](https://www.gtk.org/) and [libadwaita](https://gnome.pages.gitlab.gnome.org/libadwaita/)
- Designed following [GNOME Human Interface Guidelines](https://developer.gnome.org/hig/)
- Works with [HDHomeRun](https://www.silicondust.com/) network TV tuners