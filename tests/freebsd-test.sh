#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMG="/tmp/comphist.img"
MD_UNIT="42"
POOL="chist"
MNT="/tmp/chist"
BIN="$REPO_ROOT/zfs-comphist"

cleanup() {
    sudo zpool destroy "$POOL" 2>/dev/null || true
    sudo mdconfig -d -u "$MD_UNIT" 2>/dev/null || true
    sudo rm -f "$IMG"
}
trap cleanup EXIT

if [[ ! -x "$BIN" ]]; then
    echo "error: $BIN not found or not executable; run make/gmake first" >&2
    exit 1
fi

# 1) Create disposable pool-on-file
sudo truncate -s 4G "$IMG"
sudo mdconfig -a -t vnode -f "$IMG" -u "$MD_UNIT"
sudo zpool create -f -m "$MNT" "$POOL" "/dev/md$MD_UNIT"

# 2) Create datasets with different compression settings
sudo zfs create "$POOL/off"
sudo zfs set compression=off "$POOL/off"

sudo zfs create "$POOL/lz4"
sudo zfs set compression=lz4 "$POOL/lz4"

sudo zfs create "$POOL/lzjb"
sudo zfs set compression=lzjb "$POOL/lzjb"

# 3) Write data (compressible + incompressible)
sudo dd if=/dev/zero of="$MNT/lz4/zeros.bin" bs=1m count=128
sudo dd if=/dev/zero of="$MNT/lzjb/zeros.bin" bs=1m count=128
sudo dd if=/dev/urandom of="$MNT/off/random.bin" bs=1m count=64
sync

# 4) Run comphist
sudo "$BIN" "$POOL" -r -p --allow-live
sudo "$BIN" "$POOL" --allow-live --json
