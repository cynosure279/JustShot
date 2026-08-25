# JustShot — Screenshot Tool for Phosh

A screenshot capture tool for Linux mobile devices (Phosh/postmarketOS).

## Features

- **Full screen capture** — via phosh's built-in `org.gnome.Shell.Screenshot` D-Bus interface
- **Area selection** — interactive on-screen region selection via `SelectArea()`
- **Configurable delay** — 0s / 1s / 3s / 10s (default 1s, lets the quick settings panel fold)
- **Desktop notifications** — success/failure feedback
- **Phosh quick setting plugin** — fold-out panel with mode and delay selection
- **Automatic file saving** — `~/Pictures/Screenshots/Screenshot_YYYY-MM-DD_HH-MM-SS.png`

## Requirements

- libjustcapture (included as meson subproject)
- GTK 4, libadwaita 1.6, GLib 2.80+
- phosh ≥ 0.56 (for quick setting plugin)
- Meson ≥ 1.3, Ninja

## Build

```sh
meson setup builddir
ninja -C builddir
sudo ninja -C builddir install
```

## Usage

```sh
# Full screen screenshot
justshot

# Area selection
justshot --area

# With delay
justshot --delay 3
justshot --area --delay 3
```

## Phosh Plugin

The quick setting plugin appears in the phosh quick settings panel.
- **Short press**: fold out settings (mode + delay selection)
- **Footer button**: take screenshot with selected options
- Panel auto-folds on capture

## License

GPL-2.0+
