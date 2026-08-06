# nice_view_gem

`nice_view_gem` is the Toucan2 nice!view status-screen shield. It requires an `&nice_view_spi` labeled SPI bus with MOSI, SCK, and CS pins. Toucan2 provides that bus in `boards/shields/toucan/toucan_left.overlay`.

## Selecting a display style

Set `CONFIG_TOUCAN_STATUS_SCREEN` in `boards/shields/toucan/toucan_left.conf` before building the left-half firmware:

| Value | Layout |
| --- | --- |
| `0` | Text status |
| `1` | Toucan logo |
| `2` | Arc gauges with WPM chart |
| `3` | Pixel Operator UI |

Style `3` is the default. Style selection happens at compile time, so changing the value requires rebuilding and reflashing the left half.

The text, logo, and arc/WPM layouts use the QuinqueFive assets. The Pixel layout uses Pixel Operator Mono. Font attribution and license files are linked from the repository README.
