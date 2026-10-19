#!/usr/bin/env bash
# usage: gen-panels.sh samsung-j5ltechn samsung-j7ltechn  (names = config/<name>.sh in linux-panel-drivers)
# Generate only the specified panel configs into the kernel tree's msm8916-generated dir
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
shopt -s extglob
BASE=/mnt/zpool/disk-0/project/msm8916-mainline/linux-panel-drivers
LMDPDG=/mnt/zpool/disk-0/project/msm8916-mainline/linux-mdss-dsi-panel-driver-generator/lmdpdg.py
KDIR=/android/common/kernel/mainline/msm8916-mainline/drivers/gpu/drm/panel/msm8916-generated
declare -a OPTIONS; declare -A PANELS
OUT=${TMPDIR:-/t}/j-panel-out; rm -rf $OUT; mkdir -p $OUT
for name in "$@"; do
	unset OPTIONS PANELS; declare -a OPTIONS; declare -A PANELS
	source $BASE/config/$name.sh
	mkdir -p $OUT/$name; cd $OUT/$name
	python3 $LMDPDG "${OPTIONS[@]}" $BASE/dtb/$name.dtb >/dev/null 2>&1
	for panel in "${!PANELS[@]}"; do
		compatible="${PANELS[$panel]}"
		panel_id="${compatible/-panel-/-}"
		vendor="${panel_id%%,*}"
		pname="${panel_id##*,}"; pname="${pname//-/ }"; pname="${vendor^} ${pname^^}"
		kconfig="CONFIG_DRM_PANEL_${panel_id//[,-]/_}"; kconfig="${kconfig^^}"
		driver="panel-${panel_id/,/-}"
		cp "$panel"/panel-!(simple-*).c "$KDIR/$driver.c"
		sed -Ei "s/\.compatible = \".+\"/.compatible = \"$compatible\"/g; s/\.name = \".+\"/.name = \"$driver\"/g; s#<linux/device-id/of.h>#<linux/mod_devicetable.h>#" "$KDIR/$driver.c"
		python3 "$HERE/post.py" "$KDIR/$driver.c"
		python3 "$HERE/kins.py" "$KDIR" "${kconfig#CONFIG_}" "$pname" "$driver"
		echo "$driver ${kconfig#CONFIG_}"
	done
done
