#!/bin/bash -e
# Start from the Raspberry Pi OS Lite rootfs built by stage2.
if [ ! -d "${ROOTFS_DIR}" ]; then
	copy_previous
fi
