"""Offline lifecycle tests; all writable paths and network/service commands are mocked."""

import os
import pathlib
import signal
import subprocess
import tempfile
import time
import unittest
from unittest import mock


class HotspotControlTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="honor6x-hotspot-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.worker = self.root / "honor6x-hotspot-worker"
        self.pid_file = self.root / "controller.pid"
        self.state_file = self.root / "hotspot.state"
        self.log_file = self.root / "controller.log"
        self.launched = self.root / "launched.pid"
        self.polls = self.root / "polls"
        self.wifi = self.root / "wifi.running"
        self.wifi.touch()
        source = pathlib.Path(__file__).with_name("honor6x-hotspot-control").read_text()
        replacements = {
            "/usr/libexec/honor6x/honor6x-hotspot-worker": str(self.worker),
            "/usr/libexec/honor6x/wpa_supplicant-p2p": str(self.bin / "p2p"),
            "/run/honor6x-hotspot-controller.pid": str(self.pid_file),
            "/run/honor6x-hotspot.state": str(self.state_file),
            "/run/honor6x-hotspot-controller.log": str(self.log_file),
            "/run/honor6x-hotspot/dnsmasq.leases": str(self.root / "leases"),
            "/run/wpa_supplicant": str(self.root / "wpa"),
        }
        for old, new in replacements.items():
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertNotIn("/run/", source)
        self.assertNotIn("/usr/libexec/", source)
        self.controller = self.root / "honor6x-hotspot-control"
        self.controller.write_text(source)
        self.write_script(self.bin / "id", 'printf "0\\n"\n')
        self.write_script(self.bin / "p2p", "exit 99\n")
        for command in ("dnsmasq", "iptables-legacy", "curl"):
            self.write_script(self.bin / command, "exit 99\n")
        self.write_script(self.bin / "ip", 'printf "    inet 192.0.2.2/24 scope global wlan0\\n"\n')
        self.write_script(self.bin / "wpa_cli", 'printf "RSSI=-50\\n"\n')
        self.write_script(self.bin / "sleep", r'''
printf 'poll\n' >>"$TEST_ROOT/polls"
exec /bin/sleep 0.01
''')
        self.write_script(self.bin / "rc-service", r'''
[ "$1" = honor6x-wifi ] || exit 99
printf '%s\n' "$2" >>"$TEST_ROOT/service.calls"
case "$2" in
    status) [ -f "$TEST_ROOT/wifi.running" ] || exit 1; printf 'started\n' ;;
    start) touch "$TEST_ROOT/wifi.running" ;;
    *) exit 99 ;;
esac
''')
        self.write_script(self.worker, r'''
printf '%s\n' "$$" >"$TEST_ROOT/launched.pid"
printf 'launch\n' >>"$TEST_ROOT/launches"
cleanup() { rm -f "$TEST_ROOT/controller.pid" "$TEST_ROOT/hotspot.state"; }
trap cleanup EXIT
trap 'exit 0' TERM INT
case "$TEST_MODE" in
    no_pid) printf 'FAIL: before PID publication\n'; exit 1 ;;
    clean_exit) exit 0 ;;
    state_only)
        printf 'active=1\n' >"$TEST_ROOT/hotspot.state"
        /bin/sleep 0.05
        printf 'FAIL: state without a live PID\n'
        exit 1 ;;
    delayed) /bin/sleep 0.08 ;;
esac
printf '%s\n' "$$" >"$TEST_ROOT/controller.pid"
case "$TEST_MODE" in
    preflight)
        printf 'FAIL: mock kernel lacks iptables-legacyables IPv4 support\n'
        exit 1 ;;
    delayed) /bin/sleep 0.08 ;;
esac
if [ "$TEST_MODE" != never_ready ]; then
    printf 'active=1\nssid=Offline-test\n' >"$TEST_ROOT/hotspot.state"
fi
while :; do /bin/sleep 0.02; done
''')
        self.environment = dict(
            os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
            TEST_ROOT=str(self.root), TEST_MODE="ready",
        )
        self.addCleanup(self.cleanup_worker)

    @staticmethod
    def write_script(path, body):
        path.write_text("#!/bin/sh\nset -eu\n" + body)
        path.chmod(0o755)

    def worker_running(self):
        if not self.launched.exists():
            return False
        pid = int(self.launched.read_text())
        try:
            cmdline = pathlib.Path("/proc", str(pid), "cmdline").read_bytes()
        except (FileNotFoundError, ProcessLookupError):
            return False
        return str(self.worker).encode() in cmdline and b"--run" in cmdline

    def cleanup_worker(self):
        # Never signal a PID unless /proc still identifies this exact test worker.
        if not self.worker_running():
            return
        try:
            os.kill(int(self.launched.read_text()), signal.SIGTERM)
        except ProcessLookupError:
            return
        deadline = time.monotonic() + 2
        while self.worker_running() and time.monotonic() < deadline:
            time.sleep(0.01)
        if self.worker_running():
            try:
                os.kill(int(self.launched.read_text()), signal.SIGKILL)
            except ProcessLookupError:
                return

    def run_control(self, action="start", mode="ready"):
        environment = dict(self.environment, TEST_MODE=mode)
        return subprocess.run(
            ["/bin/sh", str(self.controller), action], env=environment,
            capture_output=True, text=True, timeout=30,
        )

    def poll_count(self):
        return len(self.polls.read_text().splitlines()) if self.polls.exists() else 0

    def assert_fast_failure(self, mode, message=None):
        result = self.run_control(mode=mode)
        self.assertEqual(result.returncode, 1, result)
        self.assertIn("error=hotspot did not start", result.stderr)
        if message:
            self.assertIn(message, result.stderr)
        self.assertNotIn("active=1", result.stdout)
        self.assertLess(self.poll_count(), 100, "controller exhausted the full startup timeout")
        self.assertFalse(self.pid_file.exists())
        self.assertFalse(self.state_file.exists())

    def test_preflight_failure_after_pid_cleanup_returns_early(self):
        self.assert_fast_failure("preflight", "mock kernel lacks iptables-legacyables IPv4 support")

    def test_exit_before_pid_publication_returns_early(self):
        self.assert_fast_failure("no_pid", "before PID publication")

    def test_zero_exit_without_ready_is_not_success(self):
        self.assert_fast_failure("clean_exit")

    def test_state_without_live_worker_pid_is_not_success(self):
        self.assert_fast_failure("state_only", "state without a live PID")

    def test_delayed_pid_and_ready_are_not_false_failures(self):
        result = self.run_control(mode="delayed")
        self.assertEqual(result.returncode, 0, result)
        self.assertIn("active=1", result.stdout)
        self.assertIn("upstream_ip=192.0.2.2/24", result.stdout)
        self.assertGreater(self.poll_count(), 0)
        self.assertTrue(self.worker_running())

    def test_repeated_start_reuses_ready_worker(self):
        self.assertEqual(self.run_control().returncode, 0)
        result = self.run_control()
        self.assertEqual(result.returncode, 0, result)
        self.assertIn("active=1", result.stdout)
        self.assertEqual((self.root / "launches").read_text().splitlines(), ["launch"])

    def test_stop_active_worker_waits_for_cleanup(self):
        self.assertEqual(self.run_control().returncode, 0)
        result = self.run_control("stop")
        self.assertEqual(result.returncode, 0, result)
        self.assertIn("active=0", result.stdout)
        self.assertFalse(self.pid_file.exists())
        self.assertFalse(self.state_file.exists())
        self.assertFalse(self.worker_running())

    def test_stop_stale_pid_does_not_signal_unrelated_process_and_restores_wifi(self):
        self.pid_file.write_text(str(os.getpid()) + "\n")
        self.state_file.write_text("active=1\n")
        self.wifi.unlink()
        result = self.run_control("stop")
        self.assertEqual(result.returncode, 0, result)
        self.assertIn("active=0", result.stdout)
        self.assertEqual((self.root / "service.calls").read_text().splitlines(), ["status", "start"])
        self.assertTrue(self.wifi.exists())
        self.assertFalse(self.pid_file.exists())
        self.assertFalse(self.state_file.exists())

    def test_live_worker_without_ready_keeps_existing_timeout(self):
        result = self.run_control(mode="never_ready")
        self.assertEqual(result.returncode, 1, result)
        self.assertIn("error=hotspot did not start", result.stderr)
        self.assertEqual(self.poll_count(), 100)
        self.assertTrue(self.worker_running())

    def test_disappearing_proc_entry_is_an_exited_fixture_worker(self):
        self.launched.write_text(str(os.getpid()) + "\n")
        with mock.patch.object(pathlib.Path, "read_bytes", side_effect=ProcessLookupError(3, "fixture exited")):
            self.assertFalse(self.worker_running())

    def test_worker_exit_between_identity_check_and_signal_is_safe(self):
        self.launched.write_text(str(os.getpid()) + "\n")
        with mock.patch.object(self, "worker_running", return_value=True), \
             mock.patch("os.kill", side_effect=ProcessLookupError(3, "fixture exited")) as signal_call:
            self.cleanup_worker()
        signal_call.assert_called_once()


if __name__ == "__main__":
    unittest.main()
