#!/usr/bin/python3
import subprocess
import sys

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk


BRIGHTNESS = "/sys/class/leds/lcd_backlight0/brightness"
MAXIMUM = "/sys/class/leds/lcd_backlight0/max_brightness"
HELPER = "/usr/libexec/honor6x/honor6x-brightness-control"


def read_int(path):
    with open(path, encoding="ascii") as handle:
        return int(handle.read().strip())


class BrightnessWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="Honor 6X Brightness")
        self.set_border_width(24)
        self.set_default_size(458, 260)
        self.set_position(Gtk.WindowPosition.CENTER)

        current = read_int(BRIGHTNESS)
        maximum = read_int(MAXIMUM)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=18)
        self.add(box)

        label = Gtk.Label(label="Screen brightness / 屏幕亮度")
        label.set_xalign(0)
        box.pack_start(label, False, False, 0)

        self.scale = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, 1, maximum, max(1, maximum // 100)
        )
        self.scale.set_value(max(1, current))
        self.scale.set_hexpand(True)
        self.scale.set_draw_value(True)
        box.pack_start(self.scale, True, True, 0)

        self.status = Gtk.Label()
        self.status.set_xalign(0)
        box.pack_start(self.status, False, False, 0)

        button = Gtk.Button(label="Apply / 应用")
        button.connect("clicked", self.on_apply)
        box.pack_end(button, False, False, 0)

    def on_apply(self, _button):
        value = int(self.scale.get_value())
        result = subprocess.run(
            ["pkexec", HELPER, "set", str(value)],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode == 0:
            self.status.set_text(f"Applied / 已应用: {value}")
        else:
            message = result.stderr.strip() or "permission or hardware error"
            self.status.set_text(f"Failed / 失败: {message}")


def main():
    try:
        window = BrightnessWindow()
    except (OSError, ValueError) as error:
        print(f"brightness device unavailable: {error}", file=sys.stderr)
        return 1
    window.connect("destroy", Gtk.main_quit)
    window.show_all()
    Gtk.main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
