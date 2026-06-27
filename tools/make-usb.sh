#!/bin/bash
# Noctua OS - Create bootable USB drive
# Usage: sudo ./make-usb.sh /dev/sdX
# WARNING: This will erase ALL data on the target device!

set -e

if [ $# -ne 1 ]; then
    echo "Usage: sudo $0 /dev/sdX"
    echo "  /dev/sdX = USB drive device (e.g., /dev/sdb)"
    echo ""
    echo "WARNING: This will erase ALL data on the target device!"
    exit 1
fi

DEVICE="$1"

if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE is not a block device"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo)"
    exit 1
fi

echo "==> Building Noctua OS..."
make -C "$(dirname "$0")/../kernel" iso 2>&1

echo "==> Writing ISO to $DEVICE (dd)..."
echo "WARNING: All data on $DEVICE will be destroyed!"
echo "Press Ctrl+C now to abort, or Enter to continue..."
read

ISO="$(dirname "$0")/../kernel/noctua.iso"
if [ ! -f "$ISO" ]; then
    echo "Error: ISO not found at $ISO"
    exit 1
fi

# Unmount any mounted partitions
for part in ${DEVICE}*; do
    if mount | grep -q "$part"; then
        umount "$part" 2>/dev/null || true
    fi
done

# Write ISO to USB drive
dd if="$ISO" of="$DEVICE" bs=4M status=progress conv=fsync

echo ""
echo "Done! Boot from $DEVICE on your real hardware."
echo "Make sure to enable USB boot in BIOS/UEFI (Legacy/CSM mode)."
