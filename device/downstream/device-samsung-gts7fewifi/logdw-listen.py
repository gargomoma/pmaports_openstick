#!/usr/bin/env python3
"""Print what Android's liblog sends to /dev/socket/logdw, on a system with no logd: bind the
socket, decode the wire format, and drain it so liblog's writes succeed.

Wire format of one datagram:
    1 byte   log id          (0 main, 2 radio, 3 system, 4 crash, ...)
    2 bytes  tid             little endian
    4 bytes  tv_sec          little endian
    4 bytes  tv_nsec         little endian
    1 byte   priority        (2 verbose ... 7 fatal)
    n bytes  tag, NUL terminated
    n bytes  message, NUL terminated

Usage: logdw-listen.py [seconds]   (default: run until interrupted)
"""

import os
import socket
import struct
import sys
import time

SOCK = "/dev/socket/logdw"
PRIO = {0: "?", 1: "?", 2: "V", 3: "D", 4: "I", 5: "W", 6: "E", 7: "F"}


def main():
    limit = float(sys.argv[1]) if len(sys.argv) > 1 else None
    os.makedirs("/dev/socket", exist_ok=True)
    if os.path.exists(SOCK):
        os.unlink(SOCK)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    s.bind(SOCK)
    os.chmod(SOCK, 0o666)
    # A HAL in a failure loop can emit far more than we can print; a big buffer
    # keeps us from silently dropping the first burst, which is the useful one.
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8 << 20)
    s.settimeout(1.0)

    started = time.time()
    seen = 0
    while limit is None or time.time() - started < limit:
        try:
            data = s.recv(65536)
        except socket.timeout:
            continue
        if len(data) < 12:
            continue
        _logid, tid, sec, nsec, prio = struct.unpack("<BHIIB", data[:12])
        parts = data[12:].split(b"\x00", 2)
        tag = parts[0].decode("utf-8", "replace") if parts else ""
        msg = parts[1].decode("utf-8", "replace") if len(parts) > 1 else ""
        seen += 1
        print(f"{sec % 100000:5d}.{nsec // 1000000:03d} {tid:6d} "
              f"{PRIO.get(prio, '?')} {tag}: {msg}", flush=True)

    print(f"--- {seen} messages ---", flush=True)
    s.close()
    os.unlink(SOCK)


if __name__ == "__main__":
    main()
