#!/bin/bash
# Build lk2nd for msm8916/8929 and pack it as an Odin tar (file name inside must be the partition image name).
set -e
unset LD_PRELOAD
L=/mnt/zpool/disk-0/project/msm8916-mainline/lk2nd
OUT=${OUT:-/mnt/zpool/disk-1/tmp/j-mainline-bringup/images}
cd $L && make TOOLCHAIN_PREFIX=arm-none-eabi- -j$(nproc) lk2nd-msm8916
mkdir -p $OUT /t/lk2nd-pkg && cp $L/build-lk2nd-msm8916/lk2nd.img $OUT/lk2nd.img
cp $L/build-lk2nd-msm8916/lk2nd.img /t/lk2nd-pkg/boot.img
(cd /t/lk2nd-pkg && tar -H ustar -cf $OUT/lk2nd-msm8916-odin.tar boot.img)
ls -la $OUT
