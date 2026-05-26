#!/bin/bash
# Launch HerOS with dedicated CPU cores (14-15) and 10GB RAM
# Uses systemd cgroups for hard resource isolation

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORES="14-15"
MEM_BYTES=10737418240  # 10GB

echo "Launching HerOS with dedicated resources:"
echo "  CPUs:   $CORES"
echo "  Memory: 10 GB (hard limit)"
echo ""

# Pin to cores 14-15 via taskset + cgroup memory/cpu isolation via systemd
exec sudo systemd-run \
    --scope \
    --unit=heros-os \
    --property=AllowedCPUs="$CORES" \
    --property=MemoryMax=$MEM_BYTES \
    --property=MemoryHigh=9663676416 \
    --setenv=HOME="$HOME" \
    --setenv=DISPLAY="$DISPLAY" \
    --setenv=XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    --setenv=PULSE_SERVER="$PULSE_SERVER" \
    --setenv=SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-pulseaudio}" \
    --uid="$(id -u)" --gid="$(id -g)" \
    taskset -c 14,15 "$SCRIPT_DIR/heros" "$@"
