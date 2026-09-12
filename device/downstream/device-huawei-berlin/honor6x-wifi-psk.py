#!/usr/bin/python3
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile

CONFIG = Path('/etc/wpa_supplicant/honor6x.conf')
PROFILES = Path('/etc/wpa_supplicant/honor6x-profiles.json')
HEADER = 'ctrl_interface=/run/wpa_supplicant\nupdate_config=0\np2p_disabled=1\n'


def atomic_write(path, text):
    fd, name = tempfile.mkstemp(dir=path.parent, prefix=path.name + '.')
    try:
        with os.fdopen(fd, 'w') as stream:
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def load_profiles():
    if PROFILES.exists():
        return json.loads(PROFILES.read_text())
    profiles = {}
    # Import our previous single-profile format without exposing its PSK.
    if CONFIG.exists():
        block = CONFIG.read_text().partition('network={')[2]
        ssid = next((line.strip()[5:] for line in block.splitlines()
                     if line.strip().startswith('ssid=')), None)
        if ssid:
            profiles[json.loads(ssid)] = 'network={' + block
    return profiles


def profile_command(command, ssid=None):
    profiles = load_profiles()
    if command == 'list':
        print(json.dumps(sorted(profiles), ensure_ascii=False))
        return
    if command == 'save':
        profiles[ssid] = make_network(ssid, sys.stdin.readline().rstrip('\n'),
                                      sys.argv[3] if len(sys.argv) > 3 else 'wpa')
        atomic_write(PROFILES, json.dumps(profiles, ensure_ascii=False))
    elif command == 'forget':
        if ssid not in profiles:
            raise ValueError('network is not saved')
        old = profiles.pop(ssid)
        atomic_write(PROFILES, json.dumps(profiles, ensure_ascii=False))
        if CONFIG.exists() and old.strip() in CONFIG.read_text():
            atomic_write(CONFIG, HEADER)
        return
    elif command != 'select':
        raise ValueError('unknown profile operation')
    if ssid not in profiles:
        raise ValueError('network is not saved')
    atomic_write(CONFIG, HEADER + profiles[ssid])


def make_network(ssid, passphrase, security='wpa'):
    ssid_bytes = ssid.encode('utf-8')
    password = passphrase.encode('utf-8')
    if not 1 <= len(ssid_bytes) <= 32 or any(c in ssid for c in '\n\r\t\x00'):
        raise ValueError('SSID must contain 1 to 32 bytes without control characters')
    if security not in ('wpa', 'open'):
        raise ValueError('unsupported security mode')
    escaped = ssid.replace('\\', '\\\\').replace('"', '\\"')
    lines = ['network={', f'\tssid="{escaped}"', '\tscan_ssid=1']
    if security == 'open':
        if passphrase:
            raise ValueError('open network must not have a password')
        lines.append('\tkey_mgmt=NONE')
    else:
        if not 8 <= len(password) <= 63:
            raise ValueError('WPA passphrase must contain 8 to 63 bytes')
        psk = hashlib.pbkdf2_hmac('sha1', password, ssid_bytes, 4096, dklen=32).hex()
        lines.extend(['\tkey_mgmt=WPA-PSK', f'\tpsk={psk}'])
    return '\n'.join(lines + ['}', ''])


def main():
    if len(sys.argv) > 1:
        try:
            profile_command(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
        except (ValueError, OSError) as exc:
            raise SystemExit(str(exc))
        return
    ssid = sys.stdin.readline().rstrip("\n")
    passphrase = sys.stdin.readline().rstrip("\n")
    ssid_bytes = ssid.encode("utf-8")
    passphrase_bytes = passphrase.encode("utf-8")

    if not 1 <= len(ssid_bytes) <= 32:
        raise SystemExit("SSID must contain 1 to 32 bytes")
    if not 8 <= len(passphrase_bytes) <= 63:
        raise SystemExit("WPA passphrase must contain 8 to 63 bytes")

    escaped_ssid = ssid.replace("\\", "\\\\").replace('"', '\\"')
    psk = hashlib.pbkdf2_hmac(
        "sha1", passphrase_bytes, ssid_bytes, 4096, dklen=32
    ).hex()
    print("network={")
    print(f'\tssid="{escaped_ssid}"')
    print(f"\tpsk={psk}")
    print("}")


if __name__ == "__main__":
    main()
