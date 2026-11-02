#!/bin/bash -e
# Copy the PiPod OS sources (staged by os/build-image.sh) into the image for building.
rm -rf "${ROOTFS_DIR}/tmp/pi-pod-os-src"
cp -a files/src "${ROOTFS_DIR}/tmp/pi-pod-os-src"
