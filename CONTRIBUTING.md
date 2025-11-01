# Contributing to HDHomeRun Config GTK

Thank you for your interest in contributing to HDHomeRun Config GTK!

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/YOUR-USERNAME/hdhomerun-config-gtk.git
   cd hdhomerun-config-gtk
   ```

3. Install the build dependencies:
   ```bash
   # On Ubuntu/Debian
   sudo apt-get install meson ninja-build libgtk-4-dev libadwaita-1-dev \
     libglib2.0-dev gettext appstream desktop-file-utils pkg-config
   ```

4. Build the project:
   ```bash
   meson setup builddir
   meson compile -C builddir
   ```

5. Run the application:
   ```bash
   ./builddir/src/hdhomerun-config-gtk
   ```

## Code Style

- Follow the existing code style in the project
- Use 2 spaces for indentation
- Keep lines under 100 characters when possible
- Add comments for complex logic

## Submitting Changes

1. Create a new branch for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. Make your changes and commit them:
   ```bash
   git add .
   git commit -m "Brief description of your changes"
   ```

3. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```

4. Open a Pull Request on GitHub

## Testing

Before submitting a PR, make sure:

- The code compiles without errors:
  ```bash
  meson compile -C builddir
  ```

- All tests pass:
  ```bash
  meson test -C builddir
  ```

- The application runs without crashes

## Reporting Issues

When reporting issues, please include:

- Your operating system and version
- GTK4 and libadwaita versions
- Steps to reproduce the issue
- Expected vs actual behavior
- Any error messages or logs

## Questions?

Feel free to open an issue on GitHub if you have questions!
