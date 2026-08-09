# Toucan2 ZMK firmware

This repository builds ZMK firmware for the [Beekeeb Toucan2](https://beekeeb.com/introducing-toucan2/), a wireless split 42-key column-staggered keyboard built around a Seeed Studio XIAO BLE controller on each half. The left half uses a nice!view display; the right half uses an I2C Azoteq TPS43 trackpad. Both firmware builds include the RGB LED adapter shield.

## Firmware builds

[`build.yaml`](build.yaml) defines three GitHub Actions outputs:

| Output | Board and shields | Purpose |
| --- | --- | --- |
| Left half | `seeeduino_xiao_ble` + `toucan_left rgbled_adapter nice_view_gem` | Keyboard central, nice!view status display, and ZMK Studio over USB |
| Right half | `seeeduino_xiao_ble` + `toucan_right rgbled_adapter` | Keyboard peripheral and TPS43 trackpad |
| Settings reset | `seeeduino_xiao_ble` + `settings_reset` | Clears stored ZMK settings and Bluetooth bonds |

The [Build ZMK firmware workflow](.github/workflows/build.yml) runs for pushes and pull requests and can also be started manually from the Actions tab. Download its firmware artifact and use the UF2 matching the half you are flashing.

## Configuration

### Keymap and per-half configuration

- Edit [`config/toucan.keymap`](config/toucan.keymap) for the active keymap.
- Edit [`boards/shields/toucan/toucan_left.conf`](boards/shields/toucan/toucan_left.conf) for left-half and display Kconfig settings.
- Edit [`boards/shields/toucan/toucan_right.conf`](boards/shields/toucan/toucan_right.conf) for right-half TPS43 and power settings.

The files [`config/toucan_left.conf`](config/toucan_left.conf) and [`config/toucan_right.conf`](config/toucan_right.conf) are links to those canonical per-half files so ZMK's user-config build finds them. Do not add duplicate root-level configuration links.

### Trackpad and gestures

Toucan2 uses the Azoteq TPS43 configuration in the `tps43_trackpad` node of [`boards/shields/toucan/toucan_right.overlay`](boards/shields/toucan/toucan_right.overlay). That node contains the hardware-facing tuning surfaces:

- LP2 report rate and power management
- pointer and scroll sensitivity
- scroll angle, axis orientation, and scroll inversion
- filtering and hold timing
- tap, press-and-hold, zoom, scroll, and three-finger swipe features

Input processing is defined in [`boards/shields/toucan/toucan.dtsi`](boards/shields/toucan/toucan.dtsi). Adjust that file for pointer/scroll scaling, zoom key bindings, mouse-layer activation, or swipe shortcuts.

The current build uses the default macOS gesture branch: pinch zoom emits Command-minus/Command-equal, and directional three-finger swipes emit Control-Command plus the corresponding arrow key. `TOUCAN_WIN_MODE` selects the alternate bindings in `toucan.dtsi`; it is not enabled by default.

### Display styles

`CONFIG_TOUCAN_STATUS_SCREEN` selects one of four compile-time nice!view layouts:

| Value | Layout |
| --- | --- |
| `0` | Text status |
| `1` | Toucan logo |
| `2` | Arc gauges with WPM chart |
| `3` | Pixel Operator UI |

Style `3` is the default. Change `CONFIG_TOUCAN_STATUS_SCREEN=3` in [`boards/shields/toucan/toucan_left.conf`](boards/shields/toucan/toucan_left.conf), then rebuild the left-half firmware. See the [`nice_view_gem` shield documentation](boards/shields/nice_view_gem/README.md) for the display integration details.

## Flashing and reset

1. Build with GitHub Actions and download the firmware artifact.
2. Connect one half over USB and double-press its reset button to mount the XIAO BLE bootloader drive.
3. Copy the matching left- or right-half UF2 to the drive. The controller reboots when the copy completes.
4. Repeat for the other half, then power-cycle both halves if they do not reconnect immediately.

To clear Bluetooth bonds or recover from stale split settings, flash the settings-reset UF2 to each affected half, wait for it to run, and then flash that half's normal firmware again. Remove the old Toucan pairing from macOS before pairing the reset keyboard.

## License and font attribution

Repository code is licensed under the [MIT License](LICENSE). The included `nice_view_gem` shield is modified from [M165437/nice-view-gem](https://github.com/M165437/nice-view-gem), also under the MIT License. ZMK-derived snippets are used under ZMK's MIT License.

The embedded fonts retain their own licenses:

- **QuinqueFive**, designed by GGBotNet, is licensed under the SIL Open Font License 1.1. See [`QuinqueFive_License.txt`](QuinqueFive_License.txt).
- **Pixel Operator Mono**, created by Jayvee Enaguas (HarvettFox96), is released under CC0 1.0 Universal. The [author's Pixel Operator page](https://www.dafont.com/pixel-operator.font) states the CC0 1.0 license. See [`PixelOperatorMono_CC0.txt`](PixelOperatorMono_CC0.txt).
