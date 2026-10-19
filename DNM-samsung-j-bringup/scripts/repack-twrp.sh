#!/bin/bash
# Repack a TWRP recovery.img with a different kernel/dt/ramdisk (Samsung, QCDT, page 2048).
# usage: [DT=qcdt.img] [RD=ramdisk.cpio.gz] repack-twrp.sh <aik-dir> <zImage> <out.img> [extra cmdline]
# <aik-dir> = copy of Android Image Kitchen (~/android/tools/superr/tools/boot/AIK) in which
#   unpackimg.sh --nosudo recovery.img was run (only split_img/ is needed; ramdisk extraction needs sudo and is skipped).
# Reproduces the stock recovery.img byte-for-byte when given the stock zImage.
a=$1; z=$2; out=$3; extra=$4; s=$a/split_img
$a/bin/linux/x86_64/mkbootimg --kernel $z --ramdisk ${RD:-$s/recovery.img-ramdisk.cpio.gz} \
  --cmdline "$(cat $s/recovery.img-cmdline)${extra:+ $extra}" --board "" --base 0x80000000 --pagesize 2048 \
  --kernel_offset 0x00008000 --ramdisk_offset 0x02000000 --second_offset 0x00f00000 --tags_offset 0x00000100 \
  --dt ${DT:-$s/recovery.img-dt} -o $out.tmp || exit 1
cat $out.tmp > $out; printf 'SEANDROIDENFORCE' >> $out; rm $out.tmp
