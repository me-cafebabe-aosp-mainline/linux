#!/bin/bash
# Build the mainline kernel + the debug initramfs and pack boot.img files that lk2nd can load.
# Output dirs live in /t (tmpfs, lost on reboot): kernel O= dir /t/kmain, work dir /t/boot.
# Final images go to $OUT (persistent).
set -e
unset O LD_PRELOAD
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
HERE=$(cd "$(dirname "$0")/.." && pwd)
SRC=$(cd "$HERE/.." && pwd)
OUT=${OUT:-/mnt/zpool/disk-1/tmp/j-mainline-bringup/images}
K=/t/kmain; W=/t/boot
mkdir -p $K $W $OUT
cd $SRC
make O=$K msm8916_defconfig
scripts/kconfig/merge_config.sh -m -O $K $K/.config $HERE/config/test.config $HERE/config/builtin.config
make O=$K olddefconfig
make O=$K -j$(nproc) Image.gz modules \
  qcom/msm8916-samsung-j3xprolte-chnopen.dtb qcom/msm8916-samsung-j3xprolte-chnctc.dtb \
  qcom/msm8916-samsung-j5ltechn.dtb qcom/msm8929-samsung-j7ltechn.dtb
# initramfs: tiny static init (USB ACM+ECM gadget, shell on /dev/ttyGS0, logs to the fb console)
aarch64-linux-gnu-gcc -static -Os -o $W/init $HERE/initramfs/init.c
rm -rf $W/rootfs; mkdir -p $W/rootfs/{dev,proc,sys,tmp,lib/firmware/qcom}; cp $W/init $W/rootfs/init
# Adreno 306 (a300_*, MSM8916) and Adreno 405 (a420_*, MSM8939/8929) GPU firmware; the built-in msm driver loads it from the initramfs
FW_DIR=${FW_DIR:-/mnt/zpool/disk-0/source/linux-firmware}
cp $FW_DIR/qcom/a300_pm4.fw $FW_DIR/qcom/a300_pfp.fw $FW_DIR/qcom/a420_pm4.fw $FW_DIR/qcom/a420_pfp.fw $W/rootfs/lib/firmware/qcom/
(cd $W/rootfs && find . | cpio -o -H newc --quiet | gzip -9 > $W/initramfs.cpio.gz)
# cmdline: console on the framebuffer; keep display clocks/power domains/regulators on for simpledrm
# (the bootloader enabled them and nothing owns them yet); lk2nd.pass-simplefb makes lk2nd add the
# simple-framebuffer node. CMD_STOP="jdbg.stop=diag" makes the init print diagnostics and stop (see init.c);
# by default the init prints the diagnostics and then starts the USB serial shell.
CMD_STOP=${CMD_STOP-}
CMD="console=tty0 loglevel=7 printk.time=1 clk_ignore_unused pd_ignore_unused regulator_ignore_unused lk2nd.pass-simplefb $CMD_STOP"
for n in msm8916-samsung-j3xprolte-chnopen msm8916-samsung-j3xprolte-chnctc msm8916-samsung-j5ltechn msm8929-samsung-j7ltechn; do
  cat $K/arch/arm64/boot/Image.gz $K/arch/arm64/boot/dts/qcom/$n.dtb > $W/$n.Image.gz-dtb
  mkbootimg --kernel $W/$n.Image.gz-dtb --ramdisk $W/initramfs.cpio.gz --cmdline "$CMD" \
    --base 0x80000000 --pagesize 2048 --kernel_offset 0x00080000 --ramdisk_offset 0x02000000 \
    --second_offset 0x00f00000 --tags_offset 0x01e00000 -o $OUT/$n.boot.img
done
ls -la $OUT
