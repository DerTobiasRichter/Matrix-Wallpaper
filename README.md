# Matrix Wallpaper

A lightweight, journald-driven **digital rain wallpaper** for **Wayland**, developed and tested on **Hyprland**.

Matrix Wallpaper turns local system journal messages into animated falling text. It uses GTK4, Cairo, and `gtk4-layer-shell` to render behind normal desktop windows, with separate renderers for each monitor.

## Features

- Digital rain generated from local `systemd-journald` messages
- Different visual treatments for informational, warning, and error messages
- CRT-inspired visual effects and fading phosphor trails
- Multi-monitor support, including monitor hotplug and automatic wallpaper recreation
- Event-driven journal processing and an approximately 15 FPS animation cap
- Optional diagnostic and performance logging (`-d` / `--debug`)
- Optional connector selection (`--monitor CONNECTOR`)

## Requirements

- Linux with a Wayland compositor supporting the layer-shell protocol (tested on Hyprland)
- GTK4
- gtk4-layer-shell
- Cairo
- systemd libraries (`libsystemd`)
- C++20 compiler
- CMake **4.4 or newer**, Ninja, and pkgconf (to build)

The application reads the local systemd journal. Which messages are available depends on the permissions of the user running it; elevated privileges are not required for ordinary use.

## Installation on Arch Linux

### Build an Arch package (recommended)

Clone the repository and run `makepkg` from its root directory, where `PKGBUILD`, `CMakeLists.txt`, and the source files reside:

```bash
git clone https://github.com/DerTobiasRichter/Matrix-Wallpaper.git
cd Matrix-Wallpaper
makepkg -si
```

The current `PKGBUILD` packages the **local checkout** and resolves dependencies through pacman. It is intended for local builds, not yet as a standalone AUR source package.

Check the installed files:

```bash
pacman -Ql matrix-wallpaper
```

#### Development Build for Arch packages

For clean builds in your workspace directory.
```bash
git clone https://github.com/DerTobiasRichter/Matrix-Wallpaper.git
cd Matrix-Wallpaper
```
Use the following clean, enforced new build and install option instead.
```bash
makepkg -Cfi
```
This way you can rebuild and install the application without changing the package version number.

### Build and install with CMake

Install the required development dependencies:

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf gtk4 gtk4-layer-shell cairo systemd-libs
```

Then build and install:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --parallel
sudo cmake --install build
```

The executable will be installed to `/usr/local/bin/matrix-wallpaper` with the CMake commands above, or `/usr/bin/matrix-wallpaper` when installed through the Arch package.

## Usage

Run on all available monitors:

```bash
matrix-wallpaper
```

Enable diagnostic output (monitor events, animation, and rendering statistics):

```bash
matrix-wallpaper -d
# or
matrix-wallpaper --debug
```

Run on a specific monitor connector:

```bash
matrix-wallpaper --monitor DP-2
```

Connector names may change after a display or Thunderbolt reconnection. Omit `--monitor` to use all available displays.

## Hyprland
### Hyprland autostart

>Hyprland is now Lua Territory ...

The fastest way to create the autostart in Hyprland is the direct 'on start' execution of the program.

Add the following to `~/.config/hypr/hyprland.lua`:

```lua
hl.on("hyprland.start", function()
    --- some other application starts e.g. 'waybar' ..
    hl.exec_cmd("matrix-wallpaper")
end)
```

The monitor manager listens for display changes, so an external monitor that becomes available after startup can receive its wallpaper automatically.

To inspect diagnostic messages during development, start the program manually with `--debug` rather than running a second instance alongside the autostarted one.

### Monitor Definition

If you have a monitor definition in your `hyprland.lua` then you can define the matrix wallpaper startup on a defined monitor.

```lua
    hl.exec_cmd("matrix-wallpaper --monitor DP-2")
```

### Presentation Mode

### Presentation Mode

Matrix Wallpaper provides an optional **Presentation Mode** (also known as *Paranoia Mode*) that replaces incoming journal text with randomized characters.

The mode can be toggled at runtime by sending `SIGUSR1` to the wallpaper process. No restart is required.

**Existing streams are not cleared.** Messages already queued or rendered may continue displaying their original contents until they naturally disappear.

This feature is intended for presentations and screen sharing, **not as a security or privacy guarantee**.

> **Why doesn't Presentation Mode clear the buffers?**
>
> Because if shoulder surfing is a serious threat, installing a live journald wallpaper probably wasn't your brightest idea in the first place.
>
> Consider this a feature designed to exercise the logical reasoning circuitry in your soft-tissue hardware. 😎

Add the following shortcut to your Hyprland Lua configuration:

```lua
    local mod = "SUPER"
    
    hl.bind(mod .. " + P", hl.dsp.exec_cmd("pkill -USR1 -f '(^|/)matrix-wallpaper($| )'"))
```

Press **SUPER + P** to toggle between live journal messages and randomized characters.

## Architecture

- **`JournalSource`** reads local journald events asynchronously and notifies the GTK main loop.
- **`MatrixRenderer` / `MatrixStream`** turn messages into animated streams.
- **`MonitorManager`** observes GDK monitor additions, removals, and geometry changes.
- **GTK4 + Cairo + gtk4-layer-shell** render background-layer windows on each display.

## Limitations

- Developed and tested primarily on Arch Linux with Hyprland; compatibility with other Wayland compositors depends on layer-shell support.
- The current glyph renderer uses printable ASCII rather than full UTF-8 text rendering.
- The current `PKGBUILD` is designed for local source-tree packaging.

## License

GPL-3.0-only. Copyright © 2026 Tobias Richter.

Project: [DerTobiasRichter on GitHub](https://github.com/DerTobiasRichter)
