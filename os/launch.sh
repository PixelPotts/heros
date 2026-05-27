#!/bin/bash
# ── Launch HerOS in its own Linux namespace ──────────────────────
# Creates PID + mount + UTS namespaces with an isolated rootfs.
# HerOS becomes PID 1 in its own isolated world.
# No reboot needed. No VM. Just Linux namespaces.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOTFS="$SCRIPT_DIR/rootfs"
REAL_USER=$(id -u)
REAL_GROUP=$(id -g)

if [ ! -d "$ROOTFS/usr/bin" ]; then
    echo "ERROR: Rootfs not found. Build it first:"
    echo "  ./os/mkrootfs.sh"
    exit 1
fi

echo "Launching HerOS in isolated namespace..."
echo "  Rootfs:   $ROOTFS"
echo "  CPU:      cores 14-15"
echo "  PID ns:   yes (HerOS = PID 1)"
echo "  Mount ns: yes (own rootfs)"
echo "  UTS ns:   yes (hostname: heros)"
echo ""

# ── Allow root to access our X11 display ─────────────────────────
xhost +local: >/dev/null 2>&1 || true

# ── Need root for namespace setup (same as old systemd-run launcher)
# Drops back to real user inside the namespace for heros-bin.
exec sudo \
    DISPLAY="$DISPLAY" \
    XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}" \
    WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-}" \
    REAL_USER="$REAL_USER" \
    REAL_GROUP="$REAL_GROUP" \
    unshare --pid --fork --mount --uts \
    "$SCRIPT_DIR/init.sh" "$ROOTFS"
