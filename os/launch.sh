#!/bin/bash
# ── Launch HerOS in its own Linux namespace ──────────────────────
# Creates PID + mount + UTS + user namespaces.
# HerOS becomes PID 1 in its own isolated world.
# No reboot needed. No VM. Just Linux namespaces.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOTFS="$SCRIPT_DIR/rootfs"

if [ ! -d "$ROOTFS/usr/bin" ]; then
    echo "ERROR: Rootfs not found. Build it first:"
    echo "  ./os/mkrootfs.sh"
    exit 1
fi

# ── Pin to isolated CPUs before entering namespace ───────────────
# (sched_setaffinity needs real capabilities, do it outside)
taskset -pc 14,15 $$ >/dev/null 2>&1 || true

echo "Launching HerOS in isolated namespace..."
echo "  Rootfs:   $ROOTFS"
echo "  CPU:      cores 14-15"
echo "  PID ns:   yes (HerOS = PID 1)"
echo "  Mount ns: yes (own rootfs)"
echo "  UTS ns:   yes (hostname: heros)"
echo ""

# ── Enter namespaces ─────────────────────────────────────────────
# --user:           own UID/GID mapping (our UID → root inside)
# --map-root-user:  map current user to root in namespace
# --pid:            own PID namespace (heros-bin = PID 1 inside)
# --fork:           fork so PID namespace takes effect
# --mount:          own mount namespace (can mount /proc, bind X11)
# --uts:            own hostname
exec unshare --user --map-root-user --pid --fork --mount --uts \
    "$SCRIPT_DIR/init.sh" "$ROOTFS"
