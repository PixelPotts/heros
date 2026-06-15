#!/bin/bash
# Launch HerOS with dedicated CPU cores and memory limit
# Uses systemd cgroups for hard resource isolation

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORES="14-15"
NUM_CORES="14,15"
MEM_BYTES=6442450944   # 6GB
MEM_HIGH=5798205850    # 90% of 6GB (soft limit / pressure warning)
MEM_DISPLAY="6 GB"

echo "Launching HerOS with dedicated resources:"
echo "  CPUs:   $CORES (2 cores)"
echo "  Memory: $MEM_DISPLAY (hard limit)"
echo ""

# Pin to cores via taskset + cgroup memory/cpu isolation via systemd
exec sudo systemd-run \
    --scope \
    --unit=heros-os \
    --property=AllowedCPUs="$CORES" \
    --property=MemoryMax=$MEM_BYTES \
    --property=MemoryHigh=$MEM_HIGH \
    --setenv=HOME="$HOME" \
    --setenv=DISPLAY="$DISPLAY" \
    --setenv=XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    --setenv=DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-}" \
    --setenv=WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
    --setenv=SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-}" \
    --setenv=SDL_AUDIODRIVER=dummy \
    --uid="$(id -u)" --gid="$(id -g)" \
    taskset -c $NUM_CORES "$SCRIPT_DIR/heros-bin" "$@"
