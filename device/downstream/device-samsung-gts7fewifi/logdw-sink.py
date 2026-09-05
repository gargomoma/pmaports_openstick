#!/usr/bin/env python3
"""Bind /dev/socket/logdw and discard. With nothing bound, liblog retries the connect for
every message inside the HAL's own threads; draining keeps the receive buffer from filling.
Run logdw-listen.py instead to read the log."""

import os
import socket

PATH = "/dev/socket/logdw"

os.makedirs(os.path.dirname(PATH), exist_ok=True)
try:
    os.unlink(PATH)
except FileNotFoundError:
    pass

sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
sock.bind(PATH)
os.chmod(PATH, 0o666)
# A large receive buffer means fewer wakeups: the kernel coalesces nothing for
# datagrams, but a deep queue lets this loop fall behind briefly without any
# sender ever seeing ENOBUFS.
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 << 20)

while True:
    try:
        sock.recv(65536)
    except OSError:
        pass
