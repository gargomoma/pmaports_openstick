#!/usr/bin/env python3
"""Handle Honor 6X power-key screen toggling and graceful shutdown."""

from __future__ import annotations

import glob
import os
import select
import signal
import struct
import subprocess
import time


BRIGHTNESS = "/sys/class/leds/lcd_backlight0/brightness"
MAX_BRIGHTNESS = "/sys/class/leds/lcd_backlight0/max_brightness"
STATE_FILE = "/run/honor6x-screen-last-brightness"
EVENT = struct.Struct("@llHHi")
EV_KEY = 1
KEY_POWER = 116
KEY_RELEASE = 0
KEY_PRESS = 1
KEY_REPEAT = 2
LONG_PRESS_SECONDS = 3.0
DEBOUNCE_SECONDS = 0.25


def read_text(path: str) -> str:
    with open(path, encoding="ascii") as handle:
        return handle.read().strip()


def read_int(path: str) -> int:
    return int(read_text(path))


def find_power_input() -> str:
    for name_path in sorted(glob.glob("/sys/class/input/event*/device/name")):
        if read_text(name_path) == "hisi_on":
            event = name_path.split("/")[-3]
            device = f"/dev/input/{event}"
            if os.path.exists(device):
                return device
    raise RuntimeError("Honor 6X power-key input was not found")


def verify_hardware() -> None:
    if read_text("/sys/class/graphics/fb0/name") != "hisifb0":
        raise RuntimeError("unexpected framebuffer")
    if not os.access(BRIGHTNESS, os.R_OK | os.W_OK):
        raise RuntimeError(f"backlight is not writable: {BRIGHTNESS}")


def save_last_nonzero(value: int) -> None:
    temporary = STATE_FILE + ".tmp"
    with open(temporary, "w", encoding="ascii") as handle:
        handle.write(f"{value}\n")
    os.replace(temporary, STATE_FILE)


def load_last_nonzero(maximum: int) -> int:
    try:
        value = read_int(STATE_FILE)
    except (FileNotFoundError, OSError, ValueError):
        return maximum
    return value if 0 < value <= maximum else maximum


def write_verified(target: int) -> None:
    with open(BRIGHTNESS, "w", encoding="ascii") as handle:
        handle.write(f"{target}\n")
    time.sleep(0.08)
    actual = read_int(BRIGHTNESS)
    if actual != target:
        raise RuntimeError(
            f"backlight write mismatch target={target} actual={actual}"
        )


def toggle_backlight(maximum: int) -> None:
    current = read_int(BRIGHTNESS)
    if current > 0:
        save_last_nonzero(current)
        target = 0
    else:
        target = load_last_nonzero(maximum)
    write_verified(target)
    print(f"power-key brightness={current}->{target}", flush=True)


def request_poweroff(held_seconds: float) -> None:
    print(f"power-key poweroff held={held_seconds:.2f}s", flush=True)
    subprocess.Popen(
        ["/sbin/poweroff"],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )


def main() -> int:
    verify_hardware()
    input_device = find_power_input()
    maximum = read_int(MAX_BRIGHTNESS)
    if maximum <= 0:
        raise RuntimeError(f"invalid maximum brightness: {maximum}")
    current = read_int(BRIGHTNESS)
    if current > 0:
        save_last_nonzero(current)

    running = True
    pressed_at: float | None = None
    poweroff_requested = False
    last_release = 0.0

    def stop(_signum: int, _frame: object) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    fd = os.open(input_device, os.O_RDONLY | os.O_NONBLOCK)
    print(
        f"ready input={input_device} brightness={current} max={maximum}",
        flush=True,
    )
    try:
        while running:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if ready:
                data = os.read(fd, EVENT.size * 32)
                for offset in range(0, len(data) - EVENT.size + 1, EVENT.size):
                    _, _, event_type, code, value = EVENT.unpack_from(data, offset)
                    if event_type != EV_KEY or code != KEY_POWER:
                        continue
                    now = time.monotonic()
                    if value == KEY_PRESS and pressed_at is None:
                        pressed_at = now
                        poweroff_requested = False
                    elif value == KEY_REPEAT:
                        continue
                    elif value == KEY_RELEASE and pressed_at is not None:
                        held = now - pressed_at
                        pressed_at = None
                        if poweroff_requested or now - last_release < DEBOUNCE_SECONDS:
                            continue
                        last_release = now
                        if held >= LONG_PRESS_SECONDS:
                            request_poweroff(held)
                            poweroff_requested = True
                        else:
                            toggle_backlight(maximum)

            if (
                pressed_at is not None
                and not poweroff_requested
                and time.monotonic() - pressed_at >= LONG_PRESS_SECONDS
            ):
                request_poweroff(time.monotonic() - pressed_at)
                poweroff_requested = True
    finally:
        os.close(fd)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
