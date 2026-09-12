#!/usr/bin/env python3
"""Device package regressions using source reads and disposable command mocks only."""

import configparser
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET


SOURCE = Path(os.environ.get("H6X_DEVICE_SOURCE", Path(__file__).resolve().parent))


def source_text(name):
    return (SOURCE / name).read_text(encoding="utf-8")


def active_lines(text):
    return "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))


class DevicePolicyTests(unittest.TestCase):
    def test_b532_identity_and_split_boot_header_contract(self):
        values = dict(re.findall(r'^([a-z0-9_]+)="([^"\n]*)"$', source_text("deviceinfo"), re.M))
        expected = {
            "deviceinfo_arch": "aarch64", "deviceinfo_codename": "huawei-berlin",
            "deviceinfo_generate_bootimg": "true", "deviceinfo_header_version": "0",
            "deviceinfo_flash_pagesize": "2048", "deviceinfo_flash_offset_base": "0x00478000",
            "deviceinfo_flash_offset_kernel": "0x00008000",
            "deviceinfo_flash_offset_ramdisk": "0x07b88000",
            "deviceinfo_flash_offset_second": "0x00e88000",
            "deviceinfo_flash_offset_tags": "0x07988000",
        }
        for name, value in expected.items():
            with self.subTest(name=name):
                self.assertEqual(values.get(name), value)
        self.assertEqual(values.get("deviceinfo_create_initfs_extra"), "false")
        self.assertNotIn("deviceinfo_flash_fastboot_partition_kernel", values)

    def test_explicit_rndis_selection(self):
        self.assertIn('deviceinfo_usb_network_function="rndis.usb0"', source_text("deviceinfo"))

    def test_cmdline_has_no_debug_or_duplicate_splash(self):
        tokens = active_lines(source_text("kernel-cmdline.conf")).split()
        self.assertTrue({"buildvariant=user", "androidboot.selinux=enforcing"} <= set(tokens))
        self.assertLessEqual(tokens.count("splash"), 1)
        self.assertFalse(any(token.startswith(("pmos.debug", "rd.break"))
                             for token in tokens))

    def install_calls(self, name):
        with tempfile.TemporaryDirectory(prefix="honor6x-device-hook-") as directory:
            root = Path(directory)
            fake = root / "rc-update"
            fake.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >>"$TEST_CALLS"\n')
            fake.chmod(0o755)
            environment = dict(os.environ, PATH=str(root), TEST_CALLS=str(root / "calls"))
            result = subprocess.run(["/bin/sh", str(SOURCE / name)], env=environment,
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result)
            return (root / "calls").read_text().splitlines()

    def test_core_install_enables_recovery_and_not_hotspot(self):
        calls = self.install_calls("device-huawei-berlin-openrc.post-install")
        for service in ("honor6x-screen-toggle", "honor6x-health-led", "honor6x-zram-swap",
                        "honor6x-wifi", "kill-pbsplash"):
            self.assertIn(f"add {service} default", calls)
        self.assertIn("add networking boot", calls)
        self.assertFalse(any(call.startswith("add honor6x-hotspot ") for call in calls))

    def test_xfce_hook_disables_conflicting_services(self):
        calls = self.install_calls("device-huawei-berlin-xfce-openrc.post-install")
        self.assertIn("add honor6x-display-manager default", calls)
        for service in ("bluetooth", "kill-plymouth", "networkmanager", "nftables",
                        "postmarketos-zram-swap", "swapfile", "wpa_supplicant", "zram-init"):
            self.assertIn(f"del {service} default", calls)

    def test_greeter_sizing_and_no_autologin(self):
        config = configparser.ConfigParser(interpolation=None)
        config.read_string(source_text("90_honor6x-slick-greeter.gschema.override"))
        values = config["x.dm.slick-greeter"]
        for key, value in {"background": "'/usr/share/backgrounds/honor6x/honor6x-pmos-v26-portrait.png'",
                           "background-color": "'#203040'", "font-name": "'Sans 11'",
                           "icon-theme-name": "'Adwaita'", "xft-dpi": "96.0",
                           "cursor-theme-size": "24", "enable-hidpi": "'on'"}.items():
            self.assertEqual(values.get(key), value)
        lightdm = active_lines(source_text("50-honor6x-lightdm.conf"))
        self.assertRegex(lightdm, r"(?m)^user-session=honor6x-xfce$")
        self.assertRegex(lightdm, r"(?m)^allow-guest=false$")
        self.assertNotRegex(lightdm, r"(?m)^autologin-user\s*=\s*\S+")

    def test_xfce_scaling_and_manual_lock_remain_separate(self):
        settings = ET.fromstring(source_text("v26-honor6x-xsettings.xml"))
        for parent, child, expected in (("Xft", "DPI", "96"), ("Gtk", "FontName", "Sans 11"),
                                        ("Gdk", "WindowScalingFactor", "2")):
            item = settings.find(f"./property[@name='{parent}']/property[@name='{child}']")
            self.assertIsNotNone(item)
            self.assertEqual(item.get("value"), expected)
        saver = ET.fromstring(source_text("v26-honor6x-screensaver.xml"))
        for path, expected in (("./property[@name='saver']/property[@name='idle-activation']/property[@name='enabled']", "false"),
                               ("./property[@name='lock']/property[@name='enabled']", "true")):
            item = saver.find(path)
            self.assertIsNotNone(item)
            self.assertEqual(item.get("value"), expected)

    def test_required_firmware_hotspot_and_optional_package_boundary(self):
        recipe = source_text("APKBUILD")
        dependencies = set(" ".join(re.findall(r'(?m)^\s*depends="([^"]*)"', recipe)).split())
        self.assertTrue({"firmware-huawei-berlin", "honor6x-hotspot", "linux-huawei-berlin"} <= dependencies)
        blocked = ("docker", "containerd", "honor6x-extra-vnc", "tigervnc", "novnc",
                   "websockify", "mihomo", "sing-box", "openvpn", "wireguard", "chromium", "firefox")
        self.assertFalse(any(name == item or name.startswith(item + "-")
                             for name in dependencies for item in blocked))

    def test_package_runs_its_checks(self):
        recipe = source_text("APKBUILD")
        options = re.search(r'(?m)^options="([^"]*)"', recipe).group(1).split()
        self.assertNotIn("!check", options)
        self.assertRegex(recipe, r'(?m)^checkdepends="python3"$')
        self.assertIn("check() {", recipe)
        self.assertIn('python3 -B "$srcdir"/test_device_integration.py', recipe)


class SplashTests(unittest.TestCase):
    def exercise(self, result=0, missing=False):
        with tempfile.TemporaryDirectory(prefix="honor6x-splash-test-") as directory:
            root = Path(directory)
            service = root / "service"
            service.write_text(source_text("kill-pbsplash.initd").replace(
                "/usr/libexec/honor6x-p31-splash-stop", '"$TEST_HELPER"'))
            helper = root / "helper"
            if not missing:
                helper.write_text('#!/bin/sh\nprintf "stop\\n" >>"$TEST_LOG"\nexit "$TEST_RESULT"\n')
                helper.chmod(0o755)
            runner = root / "runner"
            runner.write_text('ebegin() { :; }\neend() { return "$1"; }\n. "$TEST_SERVICE"\nstart\n')
            environment = dict(os.environ, PATH=str(root), TEST_LOG=str(root / "calls"),
                               TEST_HELPER=str(helper), TEST_RESULT=str(result), TEST_SERVICE=str(service))
            run = subprocess.run(["/bin/sh", str(runner)], env=environment, capture_output=True,
                                 text=True, timeout=10)
            calls = (root / "calls").read_text().splitlines() if (root / "calls").exists() else []
            return run.returncode, calls

    def test_original_renderer_helper_success(self):
        self.assertEqual(self.exercise(), (0, ["stop"]))

    def test_cleanup_failure_propagates(self):
        self.assertEqual(self.exercise(7), (7, ["stop"]))

    def test_missing_helper_is_not_success(self):
        status, calls = self.exercise(missing=True)
        self.assertNotEqual(status, 0)
        self.assertEqual(calls, [])

    def test_lightdm_has_a_hard_dependency_not_only_ordering(self):
        self.assertIn('rc_need="kill-pbsplash honor6x-display-manager"',
                      active_lines(source_text("honor6x-lightdm.confd")))
        manager = source_text("honor6x-display-manager.initd")
        self.assertIn("need localmount kill-pbsplash", manager)
        self.assertIn("/usr/libexec/honor6x-p31-splash-stop || return 1", manager)

    def test_helper_is_bounded_and_does_not_start_another_renderer(self):
        helper = active_lines(source_text("honor6x-p31-splash-stop"))
        self.assertIn('[ "$count" -ge 500 ]', helper)
        self.assertIn('[ "$count" -ge 100 ]', helper)
        self.assertIn("pgrep -x plymouthd", helper)
        self.assertNotIn("plymouth quit", helper)

    def test_package_installs_both_helper_and_lightdm_dependency(self):
        recipe = source_text("APKBUILD")
        self.assertIn('"$pkgdir"/usr/libexec/honor6x-p31-splash-stop', recipe)
        self.assertIn('"$subpkgdir"/etc/conf.d/lightdm', recipe)

    def test_explicit_order_before_actual_device_display_service(self):
        source = source_text("kill-pbsplash.initd")
        self.assertIn("before honor6x-display-manager display-manager lightdm", source)
        self.assertIn("need localmount", source)
        self.assertIn("after udev", source)
        self.assertNotIn("killall -9", source)


if __name__ == "__main__":
    unittest.main()
