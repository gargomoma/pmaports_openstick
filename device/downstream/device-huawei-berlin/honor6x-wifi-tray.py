#!/usr/bin/python3
import os
import json
import re
import fcntl
import time
import signal
import socket
import subprocess
import sys
import threading

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, GLib, Gtk, Pango


CONTROL = "/usr/libexec/honor6x/honor6x-wifi-control"
HOTSPOT_CONTROL = "/usr/libexec/honor6x/honor6x-hotspot-control"
HOTSPOT_AVAILABLE = os.access(HOTSPOT_CONTROL, os.X_OK)
SOCKET_PATH = os.path.join(
    os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}"),
    "honor6x-wifi.sock",
)


def control(command, *args, input_text=None, timeout=90):
    result = subprocess.run(
        ["pkexec", CONTROL, command, *args],
        input=input_text,
        text=True,
        capture_output=True,
        timeout=timeout,
        check=False,
    )
    if result.returncode:
        message = result.stderr.strip() or result.stdout.strip() or "operation failed"
        raise RuntimeError(message.replace("error=", "", 1))
    return result.stdout


def hotspot_control(command, timeout=120):
    if not HOTSPOT_AVAILABLE:
        raise RuntimeError("hotspot backend is not installed")
    result = subprocess.run(
        ["pkexec", HOTSPOT_CONTROL, command],
        text=True,
        capture_output=True,
        timeout=timeout,
        check=False,
    )
    if result.returncode:
        message = result.stderr.strip() or result.stdout.strip() or "operation failed"
        raise RuntimeError(message.replace("error=", "", 1))
    return result.stdout


def parse_status(text):
    values = {}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def decode_ssid(value):
    data = re.sub(rb'\\x([0-9a-fA-F]{2})|\\([\\"])',
                  lambda m: bytes([int(m[1], 16)]) if m[1] else m[2],
                  value.encode('utf-8'))
    if not data or any(byte < 32 or byte == 127 for byte in data):
        return None
    try:
        return data.decode('utf-8')
    except UnicodeDecodeError:
        return None


def signal_presentation(raw):
    # UI policy, not a speed estimate or a universal RSSI standard.
    try:
        rssi = int(raw)
    except (TypeError, ValueError):
        return 'dialog-question', '未知'
    if not -127 <= rssi < 0:
        return 'dialog-question', '未知'
    for threshold, level, label in (
        (-55, 'excellent', '很强'), (-65, 'good', '良好'),
        (-75, 'ok', '一般'), (-85, 'weak', '较弱'),
        (-127, 'none', '很弱'),
    ):
        if rssi >= threshold:
            return f'network-wireless-signal-{level}-symbolic', f'{label}（{rssi} dBm）'


class WifiUi:
    def __init__(self, show_window=False):
        self.status_busy = False
        self.action_busy = False
        self.status_generation = 0
        self.link_key = None
        self.internet_generation = 0
        self.internet_busy = False
        self.internet_state = 'not_connected'
        self.internet_last_check = 0
        self.base_tooltip = 'Wi-Fi 未连接'
        self.saved_networks = set()
        self.scan_security = {}
        self.current_ssid = ''
        self.hotspot_active = False
        self.hotspot_syncing = False
        self.icon = Gtk.StatusIcon.new_from_icon_name("network-wireless-offline-symbolic")
        self.icon.set_visible(True)
        self.icon.set_tooltip_text("Honor 6X Wi-Fi")
        self.icon.connect("activate", lambda *_: self.show())
        self.icon.connect("popup-menu", self.popup_menu)

        self.window = Gtk.Window(title="Honor 6X Wi-Fi")
        self.window.set_skip_taskbar_hint(True)
        self.window.set_skip_pager_hint(True)
        self.window.set_icon_name("network-wireless")
        self.window.set_border_width(8)
        self.window.connect("delete-event", self.hide_window)

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scroll.set_min_content_width(0)
        scroll.set_min_content_height(0)
        self.window.add(scroll)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_border_width(8)
        scroll.add(box)

        self.status_label = Gtk.Label(label="正在读取状态...")
        self.status_label.set_xalign(0)
        self.status_label.set_line_wrap(True)
        self.status_label.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR)
        self.status_label.set_max_width_chars(30)
        box.pack_start(self.status_label, False, False, 0)
        self.internet_label = Gtk.Label(label='互联网：等待 Wi-Fi 连接')
        self.internet_label.set_xalign(0)
        self.internet_label.set_line_wrap(True)
        self.internet_label.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR)
        self.internet_label.set_max_width_chars(30)
        box.pack_start(self.internet_label, False, False, 0)

        self.hotspot_switch = None
        self.hotspot_label = None
        if HOTSPOT_AVAILABLE:
            hotspot_row = Gtk.Box(spacing=10)
            hotspot_title = Gtk.Label(label="无线中继")
            hotspot_title.set_xalign(0)
            hotspot_row.pack_start(hotspot_title, True, True, 0)
            self.hotspot_switch = Gtk.Switch()
            self.hotspot_switch.set_tooltip_text("通过当前 Wi-Fi 向其他设备提供网络")
            self.hotspot_switch.connect("notify::active", self.hotspot_toggled)
            hotspot_row.pack_end(self.hotspot_switch, False, False, 0)
            box.pack_start(hotspot_row, False, False, 0)
            self.hotspot_label = Gtk.Label(label="无线中继：关闭")
            self.hotspot_label.set_xalign(0)
            self.hotspot_label.set_line_wrap(True)
            self.hotspot_label.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR)
            self.hotspot_label.set_max_width_chars(30)
            box.pack_start(self.hotspot_label, False, False, 0)
        self.controls_layout = Gtk.Box(spacing=12)
        box.pack_start(self.controls_layout, False, False, 0)
        form_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        action_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.controls_layout.pack_start(form_box, True, True, 0)
        self.controls_layout.pack_start(action_box, True, True, 0)

        self.networks = Gtk.ComboBoxText.new_with_entry()
        self.networks.get_child().set_placeholder_text("选择或输入网络名称")
        self.networks.get_child().set_width_chars(12)
        self.networks.get_child().set_max_width_chars(24)
        self.networks.set_hexpand(True)
        form_box.pack_start(self.networks, False, False, 0)

        self.password = Gtk.Entry()
        self.password.set_placeholder_text("新网络密码（8-63 位）")
        self.password.set_visibility(False)
        self.password.set_width_chars(12)
        self.password.set_input_purpose(Gtk.InputPurpose.PASSWORD)
        form_box.pack_start(self.password, False, False, 0)
        self.show_password = Gtk.CheckButton(label="显示密码")
        self.show_password.connect("toggled", lambda button: self.password.set_visibility(button.get_active()))
        form_box.pack_start(self.show_password, False, False, 0)
        self.security = Gtk.ComboBoxText()
        self.security.append('wpa', 'WPA / WPA2 个人网络')
        self.security.append('open', '开放网络（无密码）')
        self.security.set_active_id('wpa')
        form_box.pack_start(self.security, False, False, 0)

        row = Gtk.Box(spacing=10)
        action_box.pack_start(row, False, False, 0)
        self.scan_button = Gtk.Button(label="扫描网络")
        self.scan_button.connect("clicked", lambda *_: self.scan())
        row.pack_start(self.scan_button, True, True, 0)
        self.connect_button = Gtk.Button(label="保存并连接")
        self.connect_button.connect("clicked", lambda *_: self.connect_selected())
        row.pack_start(self.connect_button, True, True, 0)

        row2 = Gtk.Box(spacing=10)
        action_box.pack_start(row2, False, False, 0)
        saved = Gtk.Button(label="重连上次网络")
        saved.connect("clicked", lambda *_: self.run_action("reconnect"))
        row2.pack_start(saved, True, True, 0)
        disconnect = Gtk.Button(label="断开连接")
        disconnect.connect("clicked", lambda *_: self.run_action("disconnect"))
        row2.pack_start(disconnect, True, True, 0)

        forget = Gtk.Button(label="忘记所选网络")
        forget.connect("clicked", lambda *_: self.forget_selected())
        row3 = Gtk.Box(spacing=10)
        action_box.pack_start(row3, False, False, 0)
        row3.pack_start(forget, True, True, 0)
        self.internet_button = Gtk.Button(label='检测互联网')
        self.internet_button.connect('clicked', lambda *_: self.refresh_internet(force=True))
        self.internet_button.set_size_request(-1, 44)
        row3.pack_start(self.internet_button, True, True, 0)
        self.action_controls = [self.scan_button, self.connect_button, saved,
                                disconnect, forget, self.networks, self.password,
                                self.security]
        self.networks.connect('changed', self.selection_changed)
        self.button_rows = [row, row2, row3]
        self.last_area = None
        for widget in self.action_controls:
            widget.set_size_request(-1, 44)
        self.fit_window()
        GLib.timeout_add_seconds(2, self.fit_window)

        self.window.show_all()
        if not show_window:
            self.window.hide()
        self.refresh_status()
        GLib.timeout_add_seconds(6, self.refresh_status)
        if show_window:
            self.scan()

    def fit_window(self):
        display = Gdk.Display.get_default()
        native = self.window.get_window()
        monitor = display.get_monitor_at_window(native) if native else display.get_primary_monitor()
        if monitor is None:
            monitor = display.get_monitor(0)
        area = monitor.get_workarea()
        key = (area.x, area.y, area.width, area.height)
        if key != self.last_area:
            self.last_area = key
            width = max(240, min(640, area.width - 24))
            height = max(180, min(720, area.height - 64))
            self.controls_layout.set_orientation(Gtk.Orientation.HORIZONTAL if width >= 520 else Gtk.Orientation.VERTICAL)
            for row in self.button_rows:
                row.set_orientation(Gtk.Orientation.VERTICAL if width < 520 else Gtk.Orientation.HORIZONTAL)
            self.window.resize(width, height)
            self.window.move(area.x + max(0, (area.width - width) // 2), area.y + 24)
        return True

    def hide_window(self, *_):
        self.window.hide()
        return True

    def show(self):
        self.window.show_all()
        self.window.present()
        if not self.action_busy:
            self.scan()

    def popup_menu(self, icon, button, event_time):
        menu = Gtk.Menu()
        status = Gtk.MenuItem(label="打开 Wi-Fi 管理")
        status.connect("activate", lambda *_: self.show())
        menu.append(status)
        reconnect = Gtk.MenuItem(label="重连上次网络")
        reconnect.connect("activate", lambda *_: self.run_action("reconnect"))
        menu.append(reconnect)
        disconnect = Gtk.MenuItem(label="断开连接")
        disconnect.connect("activate", lambda *_: self.run_action("disconnect"))
        menu.append(disconnect)
        menu.show_all()
        menu.popup(None, None, Gtk.StatusIcon.position_menu, icon, button, event_time)

    def set_busy(self, message):
        self.action_busy = True
        self.status_generation += 1
        self.internet_generation += 1
        self.internet_state = 'pending'
        self.internet_last_check = 0
        self.update_internet_label()
        self.status_label.set_text(message)
        for widget in self.action_controls:
            widget.set_sensitive(False)
        if self.hotspot_switch:
            self.hotspot_switch.set_sensitive(False)

    def finish_busy(self):
        self.action_busy = False
        for widget in self.action_controls:
            widget.set_sensitive(not self.hotspot_active)
        if self.hotspot_switch:
            self.hotspot_switch.set_sensitive(True)
        self.refresh_status()

    def error(self, message):
        dialog = Gtk.MessageDialog(
            transient_for=self.window,
            flags=0,
            message_type=Gtk.MessageType.ERROR,
            buttons=Gtk.ButtonsType.CLOSE,
            text="Wi-Fi 操作失败",
        )
        dialog.format_secondary_text(str(message))
        dialog.run()
        dialog.destroy()

    def threaded(self, work, success=None):
        def runner():
            try:
                value = work()
            except Exception as exc:
                GLib.idle_add(self.error, str(exc))
            else:
                if success:
                    GLib.idle_add(success, value)
            finally:
                GLib.idle_add(self.finish_busy)

        threading.Thread(target=runner, daemon=True).start()

    def refresh_status(self):
        if self.status_busy or self.action_busy:
            return True
        self.status_busy = True
        generation = self.status_generation

        def runner():
            try:
                if HOTSPOT_AVAILABLE:
                    hotspot = parse_status(hotspot_control("status", timeout=8))
                else:
                    hotspot = {"active": "0"}
                if hotspot.get("active") == "1":
                    values = hotspot
                    values["state"] = "HOTSPOT"
                else:
                    values = parse_status(control("status", timeout=8))
                    values["active"] = "0"
            except Exception as exc:
                values = {"state": "ERROR", "error": str(exc)}
            GLib.idle_add(self.apply_status, values, generation)

        threading.Thread(target=runner, daemon=True).start()
        return True

    def apply_status(self, values, generation=None):
        self.status_busy = False
        if generation is not None and generation != self.status_generation:
            if not self.action_busy:
                self.refresh_status()
            return False
        if self.action_busy:
            return False
        state = values.get("state", "DISCONNECTED")
        self.hotspot_active = values.get("active") == "1"
        if self.hotspot_switch:
            self.hotspot_syncing = True
            self.hotspot_switch.set_active(self.hotspot_active)
            self.hotspot_syncing = False
        for widget in self.action_controls:
            widget.set_sensitive(not self.hotspot_active)
        key = (("hotspot", values.get("upstream_ssid"), values.get("upstream_ip"),
                values.get("group")) if self.hotspot_active else
               ((values.get('ssid'), values.get('bssid'), values.get('ip'),
                 values.get('gateway')) if state == 'COMPLETED' and values.get('ip') else None))
        if key != self.link_key:
            self.link_key = key
            self.internet_generation += 1
            self.internet_last_check = 0
            self.internet_state = 'pending' if key else 'not_connected'
        if key is None:
            self.internet_state = 'not_connected'
        if state == "HOTSPOT":
            upstream = values.get("upstream_ssid", "")
            address = values.get("upstream_ip", "")
            icon, signal_text = signal_presentation(values.get("rssi_dbm"))
            clients = values.get("clients", "0")
            hotspot_ssid = values.get("ssid", "")
            passphrase = values.get("passphrase", "")
            self.current_ssid = upstream
            self.status_label.set_text(
                f"上游：{upstream}\n信号：{signal_text}\nIPv4：{address or '等待地址'}"
            )
            if self.hotspot_label:
                self.hotspot_label.set_text(
                    f"热点：{hotspot_ssid}\n密码：{passphrase}\n已连接设备：{clients}"
                )
            self.icon.set_from_icon_name(icon)
            self.base_tooltip = f"无线中继：{hotspot_ssid}\n上游：{upstream}\n信号：{signal_text}"
        elif state == "COMPLETED":
            ssid = values.get("ssid", "")
            self.current_ssid = ssid
            address = values.get("ip", "")
            icon, signal_text = signal_presentation(values.get('rssi_dbm'))
            self.status_label.set_text(f"热点：{ssid}\n信号：{signal_text}\nIPv4：{address or '等待地址'}")
            self.icon.set_from_icon_name(icon)
            self.base_tooltip = f"Wi-Fi：{ssid}\n信号：{signal_text}"
        elif state == "ERROR":
            self.current_ssid = ''
            self.status_label.set_text(f"服务异常：{values.get('error', '')}")
            self.icon.set_from_icon_name("dialog-error")
            self.base_tooltip = 'Wi-Fi 状态读取失败，信号未知'
        elif state in ('SCANNING', 'AUTHENTICATING', 'ASSOCIATING', 'ASSOCIATED',
                        '4WAY_HANDSHAKE', 'GROUP_HANDSHAKE'):
            self.current_ssid = ''
            self.status_label.set_text('正在扫描或连接 Wi-Fi...')
            self.icon.set_from_icon_name('network-wireless-acquiring-symbolic')
            self.base_tooltip = 'Wi-Fi 正在扫描或连接，尚未完成认证'
        else:
            self.current_ssid = ''
            saved = values.get("saved_ssid", "")
            suffix = f"\n已保存：{saved}" if saved else ""
            self.status_label.set_text(f"Wi-Fi 未连接{suffix}")
            self.icon.set_from_icon_name("network-wireless-offline-symbolic")
            self.base_tooltip = 'Wi-Fi 未连接'
        if self.hotspot_label and not self.hotspot_active:
            self.hotspot_label.set_text("无线中继：关闭")
        self.refresh_internet()
        self.update_internet_label()
        return False

    def update_internet_label(self):
        labels = {
            'online': '互联网：可达（HTTPS 检测通过）',
            'check_failed': '互联网：未确认（检测失败）',
            'pending': '互联网：等待检测',
            'checking': '互联网：检测中...',
            'not_connected': '互联网：等待 Wi-Fi 连接或地址',
            'link_changed': '互联网：网络已变化，等待重测',
        }
        text = labels.get(self.internet_state, '互联网：状态未知')
        self.internet_label.set_text(text)
        self.internet_label.set_tooltip_text('通过当前 Wi-Fi 检查 postmarketOS HTTPS 端点；失败也可能是检测站点暂时不可达。')
        self.icon.set_tooltip_text(self.base_tooltip + '\n' + text)
        self.internet_button.set_sensitive(bool(self.link_key) and not self.internet_busy and not self.action_busy)

    def refresh_internet(self, force=False):
        if self.action_busy or self.internet_busy or not self.link_key:
            return
        if not force and time.monotonic() - self.internet_last_check < 30:
            return
        self.internet_busy = True
        self.internet_state = 'checking'
        self.internet_last_check = time.monotonic()
        generation = self.internet_generation
        key = self.link_key
        self.update_internet_label()

        def runner():
            try:
                probe = hotspot_control if self.hotspot_active else control
                result = parse_status(probe('internet', timeout=15))
            except Exception:
                result = {'internet': 'check_failed'}
            GLib.idle_add(self.apply_internet, result, generation, key)

        threading.Thread(target=runner, daemon=True).start()

    def apply_internet(self, values, generation, key):
        self.internet_busy = False
        if generation == self.internet_generation and key == self.link_key and not self.action_busy:
            self.internet_state = values.get('internet', 'check_failed')
            if self.internet_state == 'link_changed':
                self.internet_last_check = 0
        self.update_internet_label()
        return False

    def scan(self):
        if self.action_busy or self.hotspot_active:
            return
        self.set_busy("正在扫描 Wi-Fi...")

        def apply_scan(result):
            text, saved = result
            self.saved_networks = set(json.loads(saved))
            ssids = set(self.saved_networks)
            selected = self.networks.get_active_text()
            self.scan_security = {}
            for line in text.splitlines():
                fields = line.split("\t", 4)
                if len(fields) >= 5 and fields[4]:
                    ssid = decode_ssid(fields[4])
                    if ssid:
                        ssids.add(ssid)
                        self.scan_security[ssid] = fields[3]
            self.networks.remove_all()
            for ssid in sorted(ssids, key=str.casefold):
                self.networks.append_text(ssid)
            if ssids:
                self.networks.set_active(0)
                preferred = selected or self.current_ssid or next(iter(sorted(self.saved_networks)), '')
                if preferred:
                    self.networks.get_child().set_text(preferred)
                self.status_label.set_text(f"发现 {len(ssids)} 个网络")
            else:
                self.status_label.set_text("没有发现可见网络")
            return False

        self.threaded(lambda: (control("scan"), control("saved")), apply_scan)

    def selection_changed(self, *_):
        ssid = self.networks.get_active_text()
        flags = self.scan_security.get(ssid, '')
        if flags:
            self.security.set_active_id('wpa' if any(x in flags for x in ('WPA', 'WEP', 'RSN')) else 'open')
        self.password.set_text('')
        self.password.set_placeholder_text('已保存，留空使用原密码' if ssid in self.saved_networks else '网络密码')

    def forget_selected(self):
        if self.action_busy:
            return
        ssid = self.networks.get_active_text()
        if ssid not in self.saved_networks:
            self.error('请选择已保存网络')
            return
        dialog = Gtk.MessageDialog(transient_for=self.window, modal=True,
                                   buttons=Gtk.ButtonsType.OK_CANCEL,
                                   text=f'忘记网络 {ssid}？')
        response = dialog.run()
        dialog.destroy()
        if response != Gtk.ResponseType.OK:
            return
        self.set_busy('正在忘记网络...')
        def finished(_):
            self.saved_networks.discard(ssid)
            self.selection_changed()
        self.threaded(lambda: control('forget', ssid), finished)

    def connect_selected(self):
        if self.action_busy:
            return
        ssid = self.networks.get_active_text()
        password = self.password.get_text()
        if not ssid:
            self.error("请先扫描并选择网络")
            return
        if ssid in self.saved_networks and not password:
            self.set_busy(f'正在连接 {ssid}...')
            self.threaded(lambda: control('select', ssid))
            return
        flags = self.scan_security.get(ssid, '')
        if 'EAP' in flags or 'WEP' in flags or ('SAE' in flags and 'PSK' not in flags):
            self.error('当前驱动路径暂不支持此网络的认证方式')
            return
        security = self.security.get_active_id()
        if security == 'wpa' and not 8 <= len(password.encode('utf-8')) <= 63:
            self.error("WPA 密码长度必须为 8 到 63 位")
            return
        self.set_busy(f"正在连接 {ssid}...")
        self.password.set_text("")
        self.threaded(
            lambda: control("connect", ssid, security, input_text=password + "\n"),
            lambda _: self.saved_networks.add(ssid),
        )

    def run_action(self, command):
        if self.action_busy:
            return
        self.set_busy("正在连接..." if command == "reconnect" else "正在断开...")
        self.threaded(lambda: control(command))

    def hotspot_toggled(self, switch, _):
        if not HOTSPOT_AVAILABLE or self.hotspot_syncing or self.action_busy:
            return
        enable = switch.get_active()
        self.set_busy("正在开启无线中继..." if enable else "正在关闭无线中继...")
        self.threaded(lambda: hotspot_control("start" if enable else "stop"))


def notify_existing():
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.settimeout(1)
            client.connect(SOCKET_PATH)
            client.sendall(b"show\n")
        return True
    except OSError:
        return False


def command_server(ui):
    try:
        os.unlink(SOCKET_PATH)
    except FileNotFoundError:
        pass
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    os.chmod(SOCKET_PATH, 0o600)
    server.listen(2)
    while True:
        client, _ = server.accept()
        with client:
            client.settimeout(1)
            try:
                data = client.recv(32)
            except OSError:
                continue
        if data.strip() == b"show":
            GLib.idle_add(ui.show)


def cleanup(*_):
    try:
        os.unlink(SOCKET_PATH)
    except FileNotFoundError:
        pass
    Gtk.main_quit()


def main():
    show_window = "--show" in sys.argv
    # Hold a kernel lock for the process lifetime, including GTK startup.
    instance_lock = open(SOCKET_PATH + '.lock', 'a')
    os.chmod(SOCKET_PATH + '.lock', 0o600)
    try:
        fcntl.flock(instance_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        if show_window:
            for _ in range(10):
                if notify_existing():
                    return 0
                time.sleep(.5)
        return 0
    ui = WifiUi(show_window=show_window)
    threading.Thread(target=command_server, args=(ui,), daemon=True).start()
    signal.signal(signal.SIGTERM, cleanup)
    signal.signal(signal.SIGINT, cleanup)
    Gtk.main()
    cleanup()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
