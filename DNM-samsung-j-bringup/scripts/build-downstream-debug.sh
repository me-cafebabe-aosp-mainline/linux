#!/bin/bash
# Build the downstream (android-3.10) debug kernel for one device and pack a debug TWRP image.
# usage: build-downstream-debug.sh <j3xpro|j5|j7>
# Downstream tree: /mnt/zpool/nvme-0/workspace/linux-kernel/android-3.10, branch mainline-port-debug
# (2 DEBUG commits: USB PHY/link + regulators + MDSS splash dumps, WCNSS/IRIS variant dump).
# The patches are saved in /mnt/zpool/disk-1/tmp/j-mainline-bringup/patches/downstream-debug-kernel.
set -e
unset O LD_PRELOAD
d=$1
export PATH=$HOME/android/kernel/toolchains/arm-eabi-7.2/bin:$PATH
cd /mnt/zpool/nvme-0/workspace/linux-kernel/android-3.10
case $d in
 j3xpro) V=msm8916_sec_j3xprolte_chnopen_defconfig ;;
 j5)     V=msm8916_sec_j5lte_chncmcc_defconfig ;;
 j7)     V=msm8929_sec_j7_chncmcc_defconfig ;;
esac
K=/t/dk/$d; mkdir -p $K
make O=$K ARCH=arm CROSS_COMPILE=arm-eabi- VARIANT_DEFCONFIG=$V SELINUX_DEFCONFIG=selinux_defconfig msm8916_sec_defconfig
scripts/config --file $K/.config --set-val LOG_BUF_SHIFT 21
make O=$K ARCH=arm CROSS_COMPILE=arm-eabi- olddefconfig
make O=$K ARCH=arm CROSS_COMPILE=arm-eabi- -j$(nproc) zImage dtbs
# QCDT: build dtbTool from ~/android/Repos/system/tools/dtbtool/dtbtool.c (gcc -O2 -o dtbTool dtbtool.c)
# dtbTool -2 -s 2048 -p $K/scripts/dtc/ -o /t/dk/$d-dt.img $K/arch/arm/boot/dts/
