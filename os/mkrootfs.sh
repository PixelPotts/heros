#!/bin/bash
# ── Build a minimal Linux rootfs for HerOS ───────────────────────
# Creates a self-contained root filesystem with just enough to run
# HerOS as PID 1 in its own namespace. No Ubuntu desktop needed.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HEROS_DIR="$(dirname "$SCRIPT_DIR")"
ROOTFS="$SCRIPT_DIR/rootfs"

echo "Building HerOS rootfs..."

# ── Clean slate ──────────────────────────────────────────────────
rm -rf "$ROOTFS"

# ── Directory skeleton ───────────────────────────────────────────
mkdir -p "$ROOTFS"/{bin,sbin,lib,lib64,opt/heros}
mkdir -p "$ROOTFS"/usr/{bin,lib,sbin}
mkdir -p "$ROOTFS"/usr/share/fonts/truetype/dejavu
mkdir -p "$ROOTFS"/{etc,dev,proc,sys,tmp,run}
mkdir -p "$ROOTFS"/home/heros/.heros/apps
mkdir -p "$ROOTFS"/var/tmp

# ── Copy a binary + all its shared library deps ─────────────────
copy_with_deps() {
    local src="$1"
    local dst="$2"
    cp "$src" "$dst"
    chmod +x "$dst"
    ldd "$src" 2>/dev/null | grep -oP '=> \K/\S+' | while read -r lib; do
        local dir
        dir="$ROOTFS$(dirname "$lib")"
        mkdir -p "$dir"
        [ -f "$ROOTFS$lib" ] || cp "$lib" "$ROOTFS$lib"
    done
    # Also grab the interpreter (ld-linux)
    local interp
    interp=$(ldd "$src" 2>/dev/null | grep -oP '/lib64/\S+' | head -1)
    if [ -n "$interp" ] && [ ! -f "$ROOTFS$interp" ]; then
        mkdir -p "$ROOTFS$(dirname "$interp")"
        cp "$interp" "$ROOTFS$interp"
    fi
}

# ── HerOS core binary ───────────────────────────────────────────
echo "  Copying heros-bin..."
copy_with_deps "$HEROS_DIR/heros-bin" "$ROOTFS/usr/bin/heros-bin"

# ── App plugin .so files ─────────────────────────────────────────
echo "  Copying app bundles..."
if [ -d "$HOME/.heros/apps" ]; then
    cp -r "$HOME/.heros/apps"/* "$ROOTFS/home/heros/.heros/apps/" 2>/dev/null || true
fi

# Copy .so deps from each app plugin
for so in "$HEROS_DIR"/build/apps/*.so; do
    [ -f "$so" ] || continue
    ldd "$so" 2>/dev/null | grep -oP '=> \K/\S+' | while read -r lib; do
        local dir
        dir="$ROOTFS$(dirname "$lib")"
        mkdir -p "$dir"
        [ -f "$ROOTFS$lib" ] || cp "$lib" "$ROOTFS$lib"
    done
done

# ── Shell + coreutils ────────────────────────────────────────────
echo "  Copying shell + coreutils..."
BINS=(
    bash sh
    ls cat mkdir rm cp mv ln chmod chown touch
    echo printf env clear stty tty
    head tail wc sort uniq cut tr tee
    grep sed awk
    find which dirname basename realpath readlink
    id whoami hostname uname date sleep
    ps top free df du mount umount
    vi less more
)

for name in "${BINS[@]}"; do
    local path
    path=$(command -v "$name" 2>/dev/null) || continue
    # Resolve symlinks
    path=$(readlink -f "$path")
    [ -f "$path" ] || continue
    local dest="$ROOTFS/usr/bin/$name"
    [ -f "$dest" ] && continue
    copy_with_deps "$path" "$dest"
done

# Symlinks that programs expect
ln -sf /usr/bin/bash "$ROOTFS/bin/bash"
ln -sf /usr/bin/sh "$ROOTFS/bin/sh"
ln -sf /usr/bin/env "$ROOTFS/usr/bin/env" 2>/dev/null || true
ln -sf /usr/bin/env "$ROOTFS/bin/env"

# ── Fonts ────────────────────────────────────────────────────────
echo "  Copying fonts..."
cp "$SCRIPT_DIR/fonts"/*.ttf "$ROOTFS/usr/share/fonts/truetype/dejavu/"

# ── Assets (wallpaper) ──────────────────────────────────────────
echo "  Copying assets..."
cp "$HEROS_DIR"/assets/* "$ROOTFS/opt/heros/assets/" 2>/dev/null || true

# ── Minimal /etc ─────────────────────────────────────────────────
echo "  Writing /etc..."

cat > "$ROOTFS/etc/hostname" <<< "heros"

cat > "$ROOTFS/etc/hosts" << 'HOSTS'
127.0.0.1  localhost heros
::1        localhost heros
HOSTS

# Copy host DNS so network works
cp /etc/resolv.conf "$ROOTFS/etc/resolv.conf" 2>/dev/null || \
    echo "nameserver 8.8.8.8" > "$ROOTFS/etc/resolv.conf"

cat > "$ROOTFS/etc/passwd" << 'PASSWD'
root:x:0:0:root:/root:/bin/bash
heros:x:1000:1000:HerOS User:/home/heros:/bin/bash
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
PASSWD

cat > "$ROOTFS/etc/group" << 'GROUP'
root:x:0:
heros:x:1000:
video:x:44:heros
render:x:108:heros
tty:x:5:heros
GROUP

cat > "$ROOTFS/etc/nsswitch.conf" << 'NSS'
passwd: files
group:  files
hosts:  files dns
NSS

# Terminfo for the terminal
if [ -d /usr/share/terminfo ]; then
    mkdir -p "$ROOTFS/usr/share/terminfo"
    # Just copy xterm and linux entries
    for t in x l v; do
        if [ -d "/usr/share/terminfo/$t" ]; then
            mkdir -p "$ROOTFS/usr/share/terminfo/$t"
            cp /usr/share/terminfo/$t/* "$ROOTFS/usr/share/terminfo/$t/" 2>/dev/null || true
        fi
    done
fi

# ── Locale ──────────────────────────────────────────────────────
mkdir -p "$ROOTFS/usr/lib/locale"
cp /usr/lib/locale/C.utf8 "$ROOTFS/usr/lib/locale/" -r 2>/dev/null || true

# ── Summary ──────────────────────────────────────────────────────
TOTAL=$(du -sh "$ROOTFS" | cut -f1)
NLIBS=$(find "$ROOTFS" -name '*.so*' | wc -l)
NBINS=$(find "$ROOTFS/usr/bin" "$ROOTFS/bin" -type f 2>/dev/null | wc -l)

echo ""
echo "HerOS rootfs built: $ROOTFS"
echo "  Size:      $TOTAL"
echo "  Libraries: $NLIBS"
echo "  Binaries:  $NBINS"
echo ""
echo "Run with: os/launch.sh"
