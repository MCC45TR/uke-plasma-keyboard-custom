# Uke Plasma Keyboard Custom

A tablet virtual keyboard and laptop control surface for Fedora KDE Wayland.
The source baseline is original KDE Plasma Keyboard 6.8.3, with an ordered
[Uke patch series](docs/upstream-maintenance.md). The existing interface and
optional tools from [mops1k/plasma-keyboard-custom](https://github.com/mops1k/plasma-keyboard-custom)
are retained with attribution and licenses. The Uke RPM stays separate from
Fedora's original Plasma Keyboard, KWin, Plasma and Qt packages.

The source-baseline transition is candidate45; its native/package/device
qualification is pending.44 passed the new functional input scenarios but
failed fixture shutdown. The last delivered release is39, described below.

The first target is the POCO Pad X1 (Uke), aarch64. The source discovers display,
touch and language capabilities rather than hardcoding a connector name.
Fedora 44, 45 and Rawhide builds are qualified independently.

## Modes

- Normal: a bottom-docked keyboard, with optional floating mode and original
  language input engines, themes, gamepad navigation and local voice input.
- Static laptop: an automatically opened, persistent full-panel keyboard and
  touchpad. Requires an already enabled internal touchscreen and an extended
  external display. KWin integration keeps pointers and application windows
  on external displays. It never enables a display or changes screen layout,
  scale, orientation or interface language.
  Automatic entry can be disabled in keyboard settings. Choosing Normal keyboard
  keeps it closed until the next eligibility cycle or an explicit manual entry.
- Dynamic laptop: a movable keyboard/touchpad panel, opened manually from the
  tray by default, with optional text-focus activation. It leaves desktop
  pointer, window and refresh policy intact. Initial implementation; hardware
  qualification is pending.

Key sizing uses physical millimetres with a calibration ruler. Function,
modifier, arrow, navigation and system-key regions are independently selectable.
The touchpad supports tap, drag, scroll, pinch and three/four-finger gestures.
Its width, buttons and short typing guard are configurable.
Static mode is blocked while a physical USB typing keyboard or USB receiver
is connected, and exits on attachment. Pogo, Bluetooth and virtual keyboards
do not trigger this USB condition. Normal and dynamic modes remain available.

Space dragging moves the text cursor; dragging left from Backspace deletes
words. Suggestions use optional original Fedora Hunspell dictionaries. Personal
learning is disabled by default. Password/unknown-content contexts do not expose
clipboard history, suggestions or emoji. Voice models are not downloaded or
loaded until requested.

![Laptop surface prototype](docs/screenshots/laptop-prototype.png)

The prototype above is rendered from the installed QML with synthetic input.
It shows the toolbar, PC keyboard and centered touchpad; physical touch and
display qualification are still pending.

## Installation and recovery

Install the signed `uke-plasma-keyboard-custom` RPM through its COPR repository,
then select **Uke Plasma Keyboard Custom** in System Settings → Virtual Keyboard.
Installation does not replace the selected input method. The original
`plasma-keyboard` package remains required and available for rollback.

On Fedora 44 or 45 aarch64:

```sh
sudo dnf copr enable mcc45tr/uke-plasma-keyboard-custom
sudo dnf install uke-plasma-keyboard-custom
```

On Rawhide aarch64, explicitly select the Rawhide chroot. Its repository name
does not follow Rawhide's numeric `$releasever`:

```sh
sudo dnf copr enable mcc45tr/uke-plasma-keyboard-custom fedora-rawhide-aarch64
sudo dnf install uke-plasma-keyboard-custom
```

When upgrading from 0.1.0-27 or earlier, the old watcher can miss an executable
replacement with unchanged size and timestamp. If the running version stays
old, reload the selected Uke provider once after DNF finishes:

```sh
uke-plasma-keyboard-custom --restart-input-method
```

This helper preserves a different provider selection made during the reload.
It does nothing when an unrelated keyboard is selected.

The KWin plugin is part of this RPM and must be rebuilt for each KWin ABI.
An incompatible plugin disables static mode with an explanation; it does not
force a KWin downgrade. Native code and native tests are built only in COPR.
There is no project Python runtime or build generator.

COPR [11317837](https://copr.fedorainfracloud.org/coprs/mcc45tr/uke-plasma-keyboard-custom/build/11317837/)
builds version 0.1.0-39 on all three targets, each with 40 passing CTests.
The signed DNF35→39 upgrade automatically renews the selected provider in the
same compositor and boot, preserving tracked preferences and display fields.
The actual fullscreen surface is on the tablet. Touch updates are independent
of rendered frames; optional physical/time-based acceleration preserves slow
precision and makes fast swipes travel farther. Static content can shift by
one physical pixel on each axis between interactions, about once per minute.
The opaque background stays fixed; exit/disable stops and resets the shift.
Both features can be disabled in keyboard settings. The native small-motion,
acceleration and pixel policies pass; runtime shifting is observed. User feedback
on39's feel and new appearance, advanced physical tests and adaptive-refresh
qualification remain open; see the
[first tablet trial](docs/device-trial.md) before using laptop mode.

See [architecture and source provenance](docs/architecture.md),
[acceptance status](docs/acceptance.md) and the retained
[original KDE README](README.upstream.md) and
[custom-tool documentation](README.custom-tools.md).
