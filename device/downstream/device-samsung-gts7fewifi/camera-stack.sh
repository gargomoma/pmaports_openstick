#!/bin/sh
# Bring up the Android userspace the camera HAL needs, in order: binderfs, properties,
# hwservicemanager, gralloc allocator, under the two -nostdlib preload libraries.

set -u

PRE=/system/lib64/libselinux_shim.so:/system/lib64/libpmos_props.so
LINKER=/system/bin/linker64
LIBDIR=/usr/lib/pmos-camera

# CAMERA_LOG_SINK=yes swaps the discarding log socket for the decoding one.
: "${CAMERA_LOG_SINK:=no}"

say() { printf 'camera-stack: %s\n' "$*"; }

if [ "$(id -u)" != 0 ]; then
	echo "camera-stack: needs root" >&2
	exit 1
fi

# 0. Copy this package's pieces into the Android tree (/var/lib/android, extracted from the
# tablet's own partitions and not packaged).
ANDROID=/var/lib/android

if [ ! -d "$ANDROID/system/lib64" ] || [ ! -x "$ANDROID/system/bin/linker64" ]; then
	echo "camera-stack: no Android tree at $ANDROID (see docs/CAMERA.md)" >&2
	exit 1
fi

for f in libselinux_shim.so libpmos_props.so libbridge.so; do
	[ -e "$LIBDIR/$f" ] || continue
	if [ ! -e "$ANDROID/system/lib64/$f" ] || [ "$LIBDIR/$f" -nt "$ANDROID/system/lib64/$f" ]; then
		cp "$LIBDIR/$f" "$ANDROID/system/lib64/$f" || exit 1
		say "installed $f"
	fi
done

if [ -e "$LIBDIR/pmos-props.conf" ]; then
	cp -u "$LIBDIR/pmos-props.conf" "$ANDROID/vendor/etc/pmos-props.conf"
fi
if [ -e "$LIBDIR/camxoverridesettings.txt" ]; then
	mkdir -p "$ANDROID/vendor/etc/camera"
	cp -u "$LIBDIR/camxoverridesettings.txt" \
		"$ANDROID/vendor/etc/camera/camxoverridesettings.txt"
fi

# The manifests must describe what this system provides: hwservicemanager treats a declared
# but absent HAL as slow to start and libhidlbase retries forever.
install_manifest() {
	src=$1
	dst=$2

	[ -e "$src" ] || return 0
	if [ -e "$dst" ] && [ ! -e "$dst.stock" ]; then
		cp -a "$dst" "$dst.stock" || return 1
	fi
	cmp -s "$src" "$dst" || cp "$src" "$dst"
}

install_manifest "$LIBDIR/vintf-manifest.xml" "$ANDROID/vendor/etc/vintf/manifest.xml"
install_manifest "$LIBDIR/vintf-framework-manifest.xml" "$ANDROID/system/etc/vintf/manifest.xml"

if [ -d "$ANDROID/vendor/etc/vintf/manifest" ] &&
	[ -n "$(ls -A "$ANDROID/vendor/etc/vintf/manifest" 2>/dev/null)" ]; then
	mkdir -p "$ANDROID/vendor/etc/vintf/manifest-disabled"
	mv "$ANDROID"/vendor/etc/vintf/manifest/* \
		"$ANDROID/vendor/etc/vintf/manifest-disabled/" 2>/dev/null
	say "moved vendor manifest fragments aside"
fi

if [ ! -e /system/lib64/libselinux_shim.so ] || [ ! -e /system/lib64/libpmos_props.so ]; then
	echo "camera-stack: preload libraries missing from /system/lib64" >&2
	exit 1
fi

# 1. binder
if ! grep -qs binderfs /proc/mounts; then
	mkdir -p /dev/binderfs
	mount -t binder binder /dev/binderfs || exit 1
fi
for n in binder hwbinder vndbinder; do
	[ -e "/dev/binderfs/$n" ] && ln -sf "/dev/binderfs/$n" "/dev/$n"
done
chmod 666 /dev/binderfs/* 2>/dev/null
say "binderfs: $(ls /dev/binderfs | tr '\n' ' ')"

# 2. The log socket: with nothing bound to /dev/socket/logdw, liblog retries every message.
if ! pgrep -f logdw-sink >/dev/null 2>&1; then
	# Fallback when the logdw-sink service is not running.
	setsid python3 "$LIBDIR/logdw-sink.py" >/var/log/logdw-sink.log 2>&1 &
	sleep 1
fi
pgrep -f logdw-sink >/dev/null 2>&1 \
	&& say "log socket: bound and discarding" \
	|| say "log socket: sink did not start (see /var/log/logdw-sink.log)"

# The decoding sink prints what the HAL logs; it costs about a quarter of a core.
if [ "$CAMERA_LOG_SINK" = yes ] && [ -x "$LIBDIR/logdw-listen.py" ]; then
	pkill -f logdw-sink 2>/dev/null
	setsid python3 "$LIBDIR/logdw-listen.py" >/var/log/android-hal.log 2>&1 &
	sleep 1
	say "android log: /var/log/android-hal.log"
fi

# 3. hwservicemanager, restarted fresh with the bridge: a session against a reused one hangs
# in CamX init. Kept when another bridge is streaming, since it holds a mapper from it.
if pgrep -f "linker64 /system/bin/sh" >/dev/null 2>&1; then
	say "another bridge is running: leaving the services alone"
	pgrep -f hwservicemanager >/dev/null 2>&1 || {
		echo "camera-stack: hwservicemanager is gone but a bridge is up" >&2
		exit 1
	}
	say "hwservicemanager: up"
	say "gralloc allocator: up"
	exit 0
fi

pkill -f hwservicemanager 2>/dev/null
sleep 1
LD_PRELOAD=$PRE setsid $LINKER /system/bin/hwservicemanager \
	>/var/log/hwservicemanager.log 2>&1 &
sleep 3
pgrep -f hwservicemanager >/dev/null 2>&1 || {
	echo "camera-stack: hwservicemanager did not start" >&2
	exit 1
}
say "hwservicemanager: up"

# 4. gralloc allocator
pkill -f allocator-service 2>/dev/null
sleep 1
LD_PRELOAD=$PRE setsid $LINKER \
	/vendor/bin/hw/vendor.qti.hardware.display.allocator-service \
	>/var/log/gralloc-allocator.log 2>&1 &
sleep 3
pgrep -f allocator-service >/dev/null 2>&1 || {
	echo "camera-stack: gralloc allocator did not start" >&2
	exit 1
}
say "gralloc allocator: up"

exit 0
