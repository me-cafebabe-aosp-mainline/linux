#!/bin/bash
# Repack the stock Lineage 17.1 boot.img of these phones with another kernel.
# usage: repack-android-boot.sh <aik-dir> <zImage> <out.img> [extra cmdline]
# ramdisk offset is 0x01000000 here (0x02000000 for the TWRP recovery images).
a=$1; z=$2; out=$3; extra=$4; s=$a/split_img
$a/bin/linux/x86_64/mkbootimg --kernel $z --ramdisk $s/boot.img-ramdisk.cpio.gz \
  --cmdline "$(cat $s/boot.img-cmdline)${extra:+ $extra}" --board "" --base 0x80000000 --pagesize 2048 \
  --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --second_offset 0x00f00000 --tags_offset 0x00000100 \
  --os_version "$(cat $s/boot.img-osversion)" --os_patch_level "$(cat $s/boot.img-oslevel)" \
  --dt $s/boot.img-dt -o $out.tmp || exit 1
cat $out.tmp > $out; printf 'SEANDROIDENFORCE' >> $out; rm $out.tmp
