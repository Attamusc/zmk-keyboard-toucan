#!/usr/bin/env python3
"""Structural checks for Toucan's compile-time display style selection."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DISPLAY = ROOT / "boards/shields/nice_view_gem"
WIDGETS = DISPLAY / "widgets"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def parse_style_sources(cmake: str) -> dict[int, list[str]]:
    branches: dict[int, list[str]] = {}
    branch_pattern = re.compile(
        r"(?:if|elseif)\(CONFIG_TOUCAN_STATUS_SCREEN EQUAL ([0-3])\)\s*"
        r"set\(TOUCAN_STATUS_SCREEN_WIDGET_SOURCES\s*(.*?)\s*\)",
        re.DOTALL,
    )
    for style, body in branch_pattern.findall(cmake):
        branches[int(style)] = re.findall(r"widgets/[a-z0-9_]+\.c", body)
    return branches


def main() -> None:
    kconfig = (DISPLAY / "Kconfig.defconfig").read_text()
    require('int "Status screen style (0 to 3)"' in kconfig, "Kconfig help must advertise styles 0..3")
    require(re.search(r"^\s*default 3\s*$", kconfig, re.MULTILINE) is not None, "Kconfig default must be style 3")
    require(re.search(r"^\s*range 0 3\s*$", kconfig, re.MULTILINE) is not None, "Kconfig range must accept style 3")
    for description in ("0 for text", "1 for the Toucan logo", "2 for arc/WPM", "3 for the Pixel Operator UI"):
        require(description in kconfig, f"Kconfig help must describe {description}")

    left_conf = (ROOT / "boards/shields/toucan/toucan_left.conf").read_text()
    require("CONFIG_TOUCAN_STATUS_SCREEN=3" in left_conf, "Toucan left config must select style 3")

    font_names = (
        "quinquefive_8",
        "quinquefive_12",
        "quinquefive_18",
        "quinquefive_24",
        "pixel_operator_mono_16",
        "pixel_operator_mono_32",
    )
    declarations = (DISPLAY / "assets/custom_fonts.h").read_text()
    screen = (DISPLAY / "custom_status_screen.c").read_text()
    for font in font_names:
        require((DISPLAY / f"assets/{font}.c").is_file(), f"missing generated font asset: {font}")
        require(f"LV_FONT_DECLARE({font});" in declarations, f"missing font declaration: {font}")
        require(f'assets/{font}.c' in screen, f"custom status screen does not include {font}")
    require("CONFIG_TOUCAN_STATUS_SCREEN == 3" in screen, "font inclusion must have an explicit style-3 seam")

    state = (WIDGETS / "util.h").read_text()
    require(re.search(r"\buint8_t\s+wpm\s*;", state) is not None, "screen state must retain WPM")
    require(re.search(r"\bbool\s+peripheral_connected\s*;", state) is not None, "screen state must retain peripheral connection")

    cmake = (DISPLAY / "CMakeLists.txt").read_text()
    sources = parse_style_sources(cmake)
    expected = {
        0: ["battery.c", "battery_peripheral.c", "layer.c", "output.c", "profile.c", "sleep.c"],
        1: ["battery.c", "battery_peripheral.c", "layer_logo.c", "output.c", "profile.c", "sleep.c"],
        2: ["battery_arc.c", "battery_arc_peripheral.c", "chart.c", "layer_arc.c", "output_arc.c", "profile_arc.c", "sleep.c"],
        3: ["battery_pixel.c", "battery_peripheral_pixel.c", "layer_pixel.c", "output_pixel.c", "profile_pixel.c", "sleep_pixel.c"],
    }
    require(set(sources) == set(expected), f"CMake must define explicit source paths for styles 0..3; found {sorted(sources)}")

    required_symbols = {
        "draw_battery_status",
        "draw_battery_peripheral_status",
        "draw_layer_status",
        "draw_output_status",
        "draw_profile_status",
        "draw_sleep_screen",
    }
    definition = re.compile(r"\bvoid\s+(draw_[a-z_]+)\s*\(")
    for style, expected_names in expected.items():
        selected = sources[style]
        require(len(selected) == len(set(selected)), f"style {style} selects a source more than once: {selected}")
        require(sorted(Path(path).name for path in selected) == sorted(expected_names), f"style {style} source mismatch: {selected}")

        counts = {symbol: 0 for symbol in required_symbols}
        for relative_path in selected:
            path = DISPLAY / relative_path
            require(path.is_file(), f"style {style} selects missing source: {relative_path}")
            for symbol in definition.findall(path.read_text()):
                if symbol in counts:
                    counts[symbol] += 1
        require(all(count == 1 for count in counts.values()), f"style {style} draw symbol counts must all be one: {counts}")

    dispatcher = (WIDGETS / "screen.c").read_text()
    require("widget_peripheral_connection_status_init();" in dispatcher, "peripheral connection listener must be initialized")
    require("widget_chart_status_init();" in dispatcher, "WPM listener must be initialized for style 2")

    print("display style structural checks passed")


if __name__ == "__main__":
    main()
