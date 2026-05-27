#!/bin/bash
# ── HerOS init (runs inside the namespace) ───────────────────────
# This is the first process in the new PID namespace.
# Sets up mounts, chroots into rootfs, execs heros-bin.
set -e

ROOTFS="$1"
if [ -z "$ROOTFS" ] || [ ! -d "$ROOTFS" ]; then
    echo "init.sh: no rootfs specified"
    exit 1
fi

# ── Hostname ─────────────────────────────────────────────────────
hostname heros 2>/dev/null || true

# ── Make rootfs a mount point (needed for pivot_root) ────────────
mount --bind "$ROOTFS" "$ROOTFS"

# ── Mount essential virtual filesystems ──────────────────────────
mount -t proc proc "$ROOTFS/proc"
mount -t sysfs sysfs "$ROOTFS/sys" 2>/dev/null || true
mount -t tmpfs tmpfs "$ROOTFS/tmp"
mount -t tmpfs tmpfs "$ROOTFS/run"

# /dev: bind from host (need /dev/dri, /dev/pts, /dev/null, etc.)
mount --rbind /dev "$ROOTFS/dev"
mount --make-rslave "$ROOTFS/dev"

# ── Display socket passthrough ───────────────────────────────────
# X11
if [ -d /tmp/.X11-unix ]; then
    mkdir -p "$ROOTFS/tmp/.X11-unix"
    mount --bind /tmp/.X11-unix "$ROOTFS/tmp/.X11-unix"
fi

# Wayland
XDG_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
if [ -d "$XDG_DIR" ]; then
    mkdir -p "$ROOTFS/run/user/1000"
    mount --bind "$XDG_DIR" "$ROOTFS/run/user/1000"
fi

# ── Pivot root ───────────────────────────────────────────────────
# Swap the real root for our rootfs
cd "$ROOTFS"
mkdir -p .pivot_old
pivot_root . .pivot_old

# Unmount old root (lazy — don't block if busy)
umount -l /.pivot_old 2>/dev/null || true

# ── Environment ──────────────────────────────────────────────────
export HOME=/home/heros
export USER=heros
export LOGNAME=heros
export HOSTNAME=heros
export SHELL=/bin/bash
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
export TERM=xterm-256color
export LANG=C.UTF-8
export LC_ALL=C.UTF-8
export XDG_RUNTIME_DIR=/run/user/1000

# Pass through display variables from host
# (inherited from parent process via unshare)
# DISPLAY and WAYLAND_DISPLAY are already in env

# SDL: use whatever display backend is available
export SDL_AUDIODRIVER=dummy

# ── Launch HerOS ─────────────────────────────────────────────────
cd /home/heros
echo "HerOS namespace ready. Starting desktop..."
exec /usr/bin/heros-bin
