# Samsung J3 Pro / J5 / J7 (China) mainline bring-up – handoff notes

**DNM** – do not merge. This directory exists so a new agent/human session can
pick the project up again. Written 2026-10-20. Also read
`arch/arm64/boot/dts/qcom/samsung-j3xpro-j5-j7-notes.txt` (measured hardware facts).

## 1. Goal and user constraints

Port to mainline Linux:

| codename | model | note |
|---|---|---|
| j3xprolte-chnopen / -chnctc | SM-J3110 / SM-J3119 | MSM8916, Rev02/03 downstream |
| j5ltechn | SM-J5008 | MSM8916, Rev01 |
| j7ltechn | SM-J7008 | MSM8929 (msm8939 family), Rev04 |

Minimum features: basic SoC, DRM display, GPU, touchscreen, buttons, USB device
mode, internal/external storage, battery, charging, LEDs (existing drivers).
Later extras were asked for: Wi-Fi/BT, anything "easily doable".

User rules (verbatim intent):
- Use `/t` for temporary files. **`/t` is tmpfs and vanishes on host reboot.**
  Keep the kernel out dirs in `/t`; keep other useful temporary files in a subdir
  of `/mnt/zpool/disk-1/tmp/`; notes go into the kernel tree (this dir).
- Commit every change; amending commits made by us is fine.
- GitHub only via ssh (no GitHub access should be needed any more).
- Do NOT broadly search other directories; use system-wide tools (LLVM, gcc, ...).
- Do not touch device partitions unless the user explicitly says so. They
  explicitly allowed: dumping and flashing the `boot` partition (done, backed
  up). Never touch other partitions.
- Build only what is needed (a full kernel build is fine now that booting is the goal).
- Commit trailer: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 2. Paths (persistent)

| what | path |
|---|---|
| mainline kernel (git worktree, branch `msm8916/7.0`) | `/android/common/kernel/mainline/msm8916-mainline` |
| downstream kernel (android-3.10, lineage-18.1), debug branch `mainline-port-debug` | `/mnt/zpool/nvme-0/workspace/linux-kernel/android-3.10` |
| lk2nd | `/mnt/zpool/disk-0/project/msm8916-mainline/lk2nd` |
| panel data + generator | `/mnt/zpool/disk-0/project/msm8916-mainline/linux-panel-drivers`, `.../linux-mdss-dsi-panel-driver-generator` |
| artifacts, logs, backups of this work | `/mnt/zpool/disk-1/tmp/j-mainline-bringup/` (see §8) |
| boot partition backups of the 3 phones | `/mnt/zpool/disk-1/tmp/j-boot-backup-20261019-231331/` |
| Android Image Kitchen | `~/android/tools/superr/tools/boot/AIK` (copy it before use; its ramdisk unpack needs sudo, split_img/ works without) |
| TWRP originals | `/mnt/remote/fs-zqm-android/recovery/old/j3-j5-j7/TWRP-3.3.1-*` |
| downstream toolchain | `~/android/kernel/toolchains/arm-eabi-7.2` |
| dtbTool source | `~/android/Repos/system/tools/dtbtool/dtbtool.c` (`gcc -O2 -o dtbTool dtbtool.c`) |
| downstream android build rules | `/mnt/zpool/disk-0/workspace/android/Repos/vendor/lineage/build/tasks/kernel.mk`, `.../config/BoardConfigKernel.mk` |
| audio mixer paths | `/mnt/zpool/disk-0/workspace/android/trees/Galaxy-MSM8916/device/samsung/msm8916-common/configs/audio/mixer_paths.xml` |

## 3. Work done (all local, nothing pushed)

Mainline kernel (`msm8916/7.0`, oldest to newest):

```
9f843f6057076 regulator: Add driver for the TI LM3632 LCD bias and backlight boost
7e39d168b6516 arm64: dts: qcom: Add Samsung Galaxy J3 Pro (SM-J3110/SM-J3119)
a31aa2534634e arm64: dts: qcom: Add Samsung Galaxy J5 (2015) (SM-J5008)
c057a9cb5e59c arm64: dts: qcom: Add Samsung Galaxy J7 (2015) (SM-J7008)
0ec72b32ca012 arm64: defconfig: msm8916: Enable SM5703 and LM3632 drivers
8ea2174b4784a arm64: dts: qcom: samsung-j3xpro/j5/j7: Add NFC and vibrator
31addf3efdf83 DNM: ... bring-up notes                  (+ this directory)
```
Below those (earlier in the series): MSM8916 panel drivers (J5/J7 panels), SM5703 MFD,
regulator, charger and fuel-gauge drivers (`drivers/mfd/sm5703.c`,
`drivers/regulator/sm5703-regulator.c`, `drivers/power/supply/sm5703_charger.c`,
`sm5703_fuel_gauge.c`) with bindings. `git log --oneline msm8916/7.0 -20` for the exact list.
Hashes change when commits are amended/rebased; trust `git log`.

Other repos:
- lk2nd: `8bcf44252` – `dtb-files` + panel selection for j3xprolte, j5ltechn, j7-chn
  (`lk2nd/device/dts/msm8916/msm8916-samsung.dts`, `msm8929-samsung.dts`).
- linux-panel-drivers: `e252a49` – configs/dtbs for samsung-j5ltechn, samsung-j7ltechn.
- downstream tree: branch `mainline-port-debug` = 2 DEBUG commits (patch files in the artifacts dir).

Design notes:
- Panel drivers are generated (linux-mdss-dsi-panel-driver-generator) from downstream
  DTBs, then post-processed for the mainline API: `scripts/gen-panels.sh`, `post.py`, `kins.py`.
- lk2nd picks the panel by downstream panel node name (`ss_dsi_panel_*` -> compatible).
  J5: ILI6236S_CPT50 / ILI9881C_SKI499017 / S6D7AA0X62_BV050HDM variants; the J5 unit
  tested has **S6D7AA0X62_BV050HDM** (LCD_ID 0x59B810). J7 has the same family
  (separate init-sequence driver `...-bv050hdm-j7ltechn`). J3 Pro: S6E8AA5X01_AMS497HY01 (OLED, existing driver).
- SM5703: MFD at I2C 0x49 (regmap-irq, USB LDOs, LDO1-3, buck, VBUS), charger cell, fuel
  gauge at 0x71 (own driver, DT tables for the Samsung battery model), MUIC via
  extcon-sm5502 (`siliconmitus,sm5703-muic`). LM3632 = LCD bias/boost, brightness via panel DCS.
- Shared DT: `msm8916-samsung-sm5703.dtsi` (also the vibrator via regulator-haptic on LDO2),
  J3 Pro uses `j3-common` -> `j5-common`; the shared trees were audited against the
  downstream data of the real units (see the notes txt; the only bug found was the J5
  accelerometer mount-matrix, fixed).
- K2HH mount-matrix convention: `out[axis_map_i] = +-raw_i` with the downstream `negate_*`.
- USB PHY init-seq is per device (see notes txt); J7 overrides msm8939.dtsi (0x3b vs 0x6b).
- Wi-Fi/BT: WCN3620A (IRIS id 0x5112) on all three, enabled via `msm8916-modem-qdsp6.dtsi`
  (J7 includes it explicitly) + `&wcnss`, `&wcnss_iris { compatible = "qcom,wcn3620"; }`.
  BT node comes with wcnss. Firmware/NV files must be provided by the OS.
- J7 CPU supply (FAN53555) deliberately NOT described: msm8939.dtsi has no CPU OPPs.

## 4. Environment gotchas (learned the hard way)

- Every Bash tool call is a fresh shell: re-export `ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-`
  and `unset O LD_PRELOAD` in each call that runs make. A stray `make` without ARCH tries to
  reconfigure the build dir for the host.
- Pass `O=/t/...` on the make command line (env `O=` is ignored by this tree -> in-tree build).
- zsh does not word-split `$var`: use `set -- $=var` or run loops under `bash -c`.
- `LD_PRELOAD=libproxychains` breaks adb: always `scripts/adb.sh` (or `env -u LD_PRELOAD adb`).
  Never `pgrep -f "<text that is in my own command line>"` (self-match, hangs).
- Never read `/proc/kmsg` over adb (blocks). Use `dmesg`.
- `adb push` of the ~7 MB image to the J5 recovery dropped the link repeatedly (cable/port); pulls were fine.
- Android boot images here are Samsung: header v0 + QCDT, page 2048, append the 16 bytes
  `SEANDROIDENFORCE`. Stock repack with the same pieces is byte-identical (verified).
- `tar -H ustar` archives with the file named `recovery.img` / `boot.img` work for Odin (AP slot).

## 5. Device inventory and current state (as of this note)

| device | adb serial | state |
|---|---|---|
| J5 (SM-J5008 Rev01) | 4345cc9f | recovery = debug TWRP; boot = debug downstream boot.img (user flashed it by hand) |
| J3 Pro (SM-J3110 Rev03) | 4bbccef1 | recovery = debug TWRP; boot = debug downstream boot.img |
| J7 (SM-J7008 Rev04) | 383bfc60 | recovery = debug TWRP; boot = debug downstream boot.img |

Restore originals: Odin AP with `*-boot-orig-RESTORE.tar` from the boot backup dir.
The user sometimes only has some phones connected; always verify `ro.product.device` before writing.

The debug TWRP images carry `ro.secure=0`, `ro.adb.secure=0` and the debug downstream kernel
(prints `DBG-USB`, `DBG-FB`, `DBG-SPLASH`, `DBG-WCNSS`). Full dmesg of all three are in the artifacts dir.

## 6. Build and packaging recipes

All scripts are in `scripts/`; they use /t for outputs and the artifacts dir for results.

- Mainline test kernel + initramfs + boot.img per device: `scripts/build-mainline-test.sh`
  (defconfig + `config/test.config` + `config/builtin.config`; the latter temporarily builds the
  display/GPU/panel/touch/SM5703/LM3632/haptic/accelerometer drivers in because the initramfs has
  no module loader, and the initramfs carries the Adreno 3xx firmware from
  `/mnt/zpool/disk-0/source/linux-firmware`. test.config: VT, fbcon + TER16x32 font, fbdev emulation, USB configfs
  ACM/ECM/RNDIS, chipidea MSM, SM5703 built in). The first test (modules off) used simpledrm only.
- `initramfs/init.c`: static aarch64 init, no busybox. Mounts proc/sys/devtmpfs, logs to
  /dev/kmsg (visible on the fb console), builds a configfs gadget (ACM + ECM), binds the UDC,
  then runs a tiny shell on `/dev/ttyGS0` (`help`, `ls`, `cat`, `dmesg`, `echo x > file`,
  `ev [secs]` (prints key/touch events of all input devices, like evtest), `insmod`, `reboot`, `poweroff`).
- lk2nd: `scripts/build-lk2nd.sh` (`make TOOLCHAIN_PREFIX=arm-none-eabi- lk2nd-msm8916`,
  image < 512 KiB). lk2nd goes into the `boot` partition; its next stage lives at +512 KiB of
  the same partition, or use `fastboot boot <boot.img>` from lk2nd's fastboot.
- cmdline needed: `lk2nd.pass-simplefb` (lk2nd adds the simple-framebuffer node and the
  reserved-memory entry), `clk_ignore_unused pd_ignore_unused regulator_ignore_unused`
  (otherwise MDSS clocks/GDSC and the display regulators get gated and the screen goes
  black), `console=tty0`. The init also understands `jdbg.stop=early|gadget|diag`: it prints
  its diagnostics (UDC state, extcon, I2C driver bindings, deferred probes, power supplies)
  into the kernel log and then sleeps forever so the screen can be read; without it the
  USB serial shell starts. `build-mainline-test.sh` starts the USB serial shell by default
  (`CMD_STOP="jdbg.stop=diag" ./build-mainline-test.sh` to stop after the diagnostics). The default console font is used.
- Downstream debug kernel: `scripts/build-downstream-debug.sh <j3xpro|j5|j7>`. Variant defconfigs:
  `msm8916_sec_defconfig` + `msm8916_sec_j3xprolte_chnopen_defconfig` / `..._j5lte_chncmcc_...` /
  `msm8929_sec_j7_chncmcc_...` + `selinux_defconfig`; toolchain arm-eabi-7.2.
- Repack TWRP: `scripts/repack-twrp.sh`; repack the Android boot.img: `scripts/repack-android-boot.sh`.
  Patching `/default.prop` in the ramdisk was done by editing the newc cpio in Python
  (flip `ro.secure=1`/`ro.adb.secure=1` to 0; same length so nothing else changes).
- Dump/flash `boot` (only with explicit permission): `dd if=/dev/block/bootdevice/by-name/boot of=/tmp/x.img bs=4096`
  (partition is 13631488 bytes), pull, compare sha256 with the on-device value; to flash push to
  /tmp, check sha256 on the device, `dd of=...by-name/boot bs=4096; sync`, read back with
  `dd bs=4096 count=N | head -c SIZE | sha256sum` (a bs=1 readback times out).

## 7. Next steps (what the user asked for)

1. Flash `lk2nd-msm8916-odin.tar` (AP slot in Odin) on one phone, boot lk2nd, then
   `fastboot boot <device>.boot.img` (images in the artifacts dir). Expected: kernel log on the
   screen via fbcon; with a USB cable a serial port `/dev/ttyACM*` on the host and a CDC-ECM
   network device; the init shell answers on the serial port.
2. Once basic features are confirmed (display, USB, buttons, touch, storage, charging), the
   user will do Android device tree bring-up on top of this mainline kernel and connect via ADB.
   Then: build DRM_MSM + panel modules into the test (`insmod` from the shell or make them =y),
   verify touch (ist3038c), accelerometer, fuel gauge/charger (SM5703), Wi-Fi/BT firmware, modem, audio.
3. Things to verify on hardware first: panel driver init (J5/J7), DSI panel regulators
   (J5 `vdd3` gpio16, J7 gpio20 + S4), touch supplies, SM5703 IRQ/mrstb, MUIC cable detection,
   charger enable polarity (J5 gpio108, J7 gpio33, J3 gpio51; active low).

## 7a. First boot results (2026-10-20, lk2nd + fastboot boot, J3 Pro and J7)

- Kernel log shows on the screen via simpledrm/fbcon, then after ~3 s the display goes blank with no
  backlight. No USB device appeared on the host (phones are usually on a Windows PC and shared to
  the Linux host via usbip: a re-enumerated device must be attached again on the Windows side,
  `usbipd list` / `usbipd attach`; the gadget is VID:PID 1d6b:0104). J5 has a physically broken USB port.
- Causes found in the code, fixed in the commits: `regulator-fixed` drives its GPIO low at probe unless
  `regulator-boot-on` is set, which cut the panel supplies; the LM3632 driver requested its
  EN/ENP/ENN GPIOs as `GPIOD_OUT_LOW`, which switched the bias/backlight off (now `GPIOD_ASIS` and
  `regulator-boot-on` on `lcd_power`). The regulator core also disables unused regulators after 30 s
  (hence `regulator_ignore_unused` on the cmdline).
- USB: the display fix worked; the init diagnostics showed `extcon USB=1` (cable detected through the
  SM5703 MUIC, so extcon/PHY/UDC probing is fine) but the gadget had no functions: configfs
  resolves symlink targets relative to the process cwd, so the relative `../../functions/x` links
  in init.c failed. Fixed with absolute targets. USB then worked (serial + network device on the host).
- Second test (all drivers built in): J3 Pro display OK (OLED, 500 MHz DSI clock). J7 blank although every panel rail, backlight and the DSI/MDP state looked fine; its only odd value was the DSI link rate (415 MHz vs the downstream 481 MHz burst clock), because mainline derives the link rate from the pixel clock. The J7 mode got the J5 porches (80.3 MHz pixel clock = 481.8 MHz link). GPU on 8939/8929 is an Adreno 405 and needs `a420_pm4.fw/a420_pfp.fw` (8916: a300). Serial helper used: pyserial script that sends a command to `/dev/ttyACM*` and prints the reply.
- J7 root cause (found by writing /sys/class/backlight/*/brightness over the USB serial shell, the
  screen lit up): the generated panel drivers never send the DCS brightness (0x51) after power-up and
  the panel resets to 0, so the display is black with the backlight off and no errors anywhere. Fixed
  in all four generated drivers (backlight_update_status() at the end of prepare(), 1-byte 0x51 like
  downstream `51 ff`, DSI LPM flag restored). The J7 mode with the J5 porches (80.3 MHz pixclk, 481.8 MHz
  link) was what was tested; the downstream-derived porches (415 MHz link) were never tested with brightness set.
- Next test: DRM_MSM, panels, touch, SM5703, LM3632 built in (`config/builtin.config`).

## 7a2. Input test (ev shell command, 60 s, J3 Pro + J7)

Volume up/down, home and (J3 Pro) power report KEY_VOLUMEUP/DOWN, KEY_HOMEPAGE, KEY_POWER with press and
release; touch gives proper multitouch events. The J7 power button is physically damaged (not tested) and
its home button is half broken. The console only types text for keys that have a VT keymap entry
(volume down), so silent keys on screen do not mean dead keys. IST3038C raw range: J3 Pro and J5 720x1280
(downstream "ist30xx_probe ... x:720, y:1280"), **J7 1152x2047**, now set in the J7 DT (the old 720x1280
made coordinates exceed the declared range).

## 7b. Sound card names and alsa-ucm-conf

Card names (DT `&sound { model = ... }`, limited to 15 characters because of the ASoC driver name):
`samsung-j3xpro`, `samsung-j5chn`, `samsung-j7chn`. UCM configs live in
`/mnt/zpool/disk-0/source/alsa-ucm-conf`, branch `samsung-j-chn` (based on `msm8916/master`, one
commit per device, worktree was `/t/ucm-wt`; the main checkout of that repo stays on `lineage-24.0`).
They use the shared `codecs/msm8916-wcd` pieces (internal speaker, earpiece, headphones, primary + headset mic).
The secondary mic (AMIC3) is not described because it is unknown whether the phones have one.

## 8. Artifacts dir `/mnt/zpool/disk-1/tmp/j-mainline-bringup/`

- `images/` – lk2nd.img, `lk2nd-msm8916-odin.tar`, `<dtb-name>.boot.img` (4 mainline test images)
- `mainline-test/` – init.c, test.config, built initramfs
- `twrp-debug/` – debug TWRP odin tars (recovery.img inside)
- `boot-debug/` – debug downstream boot.img (see also the boot backup dir)
- `downstream/` – decoded downstream DTS (xz) of J3 Pro Rev02/J5 Rev00,01/J7 Rev04 and QCDT blobs
- `logs/` – full dmesg of every phone (recovery and Android), `dmesg-dbg` (DBG lines only), live gpio/regulator dumps
- `patches/downstream-debug-kernel/` – the 2 DEBUG commits for android-3.10
- `scripts-tmp/` – older copies of the helper scripts

## 8b. Display blinking on high-toggle content (J3 Pro) - investigated, worked around

Symptom (kernels 6.12.1 and 7.0): with the screen mostly filled with colour (noise, solid 0x55 / 0xAA fills) the
J3 Pro CTC panel blinks and shows artifacts on the right side of the lines. Fills 0x00 / 0x20 / 0x80 / 0xC0 / 0xFF are fine.

Findings:
- The panel itself reports DSI errors (DCS 0x05 error counter, bit 7 = overflow, a read clears it; DCS 0x0e bit 0
  "error on DSI"): ~7-11 on black/white fills, 54-78 + overflow on 0x55; reads fail entirely with noise on screen.
  The DSI controller reports nothing (FIFO_STATUS idle, no ACK/timeout errors), so the link is corrupted on the wire.
- **The link is marginal at 500 Mbit/s per lane** (4 lanes, RGB888, 83.4 MHz pixel clock, 60 Hz): at 45 Hz (375 Mbit/s)
  and at 55 Hz (459 Mbit/s) it is clean, at 58 Hz the blinking area shrinks but is still there. The J3 Pro panel
  (s6e8aa5x01-ams497hy01) is now a 55 Hz mode in the driver.
- Lane rates at 60 Hz of the other panels: J7 s6d7aa0x62-bv050hdm-j7ltechn 482 (clean at 60 Hz), J5 ili9881c 478,
  s6d7aa0x62-bv050hdm 482, ili6236s-cpt50 393 (J5 untested; lk2nd selects the panel from the bootloader panel id).
- Downstream drives the same panel at the same 500 Mbit/s without errors, **the cause of the difference is unknown**.
  Everything readable matched downstream and made no difference: DSI controller/PHY registers (read through the
  downstream debugfs `/sys/kernel/debug/mdp/dsi0_{ctrl,phy}_{off,reg}`), PHY timing, regulator ctrl, drive strength,
  PLL static registers, MDP INTF timing, MDP clock (80 MHz downstream vs 320 MHz), memory bandwidth votes
  (interconnect), S4 at 2.1 V ("vddb"), regulator load votes on L2/L6, programmable fetch, RGB vs DMA pipe,
  panel init sequence, clock lane forced to HS only after the panel init (samsung_lp11_init), CPU idle states.
- Things worth knowing:
  - Downstream's per-lane LN_CFG_4 = 0/5/10/15 **blanks the J7 screen** on mainline (J7 is fine with 0); it did not
    help the J3 Pro either.
  - The MSM8939 MDP master is under the `snoc_mm` interconnect provider (`&snoc` gives a path that never resolves and
    the MDP probe defers forever: "of_icc_get_by_index: invalid path=-517").
  - `CONFIG_INTERCONNECT_QCOM=y` is needed, the msm8916/39 driver options are silently dropped without it.
  - Downstream debugfs: `/sys/kernel/debug/mdp/off` takes "<hex offset without 0x> <decimal byte count>", offsets are
    mainline + 0x1000; PLL registers cannot be read (only ctrl/phy are exposed).
- The experiments (runtime module parameters for the PHY / MDP / DSI / panel, DSI error counter read, MDP register
  dump, the error counter scripts) were removed from the main branch; they are preserved on the branch
  `msm8916/7.0-display-debug` and the tag `backup/display-debug-20261024`.
- Open: why downstream is clean at 500 Mbit/s (analog / timing effect not visible in any register), J5 and J3 Pro Open
  untested.

## 9. Open items / unknowns

- Nothing from the mainline side has been booted on hardware yet.
- Headset jack polarity/thresholds (Samsung sec_jack, not MBHC) and the sound card name
  (`msm8916-1mic` vs two microphones) are unverified.
- No mainline driver: CM36672P (J3 Pro/J7) and GP2A (J5) proximity/light, SM5703 flash LED,
  NFC clock BBCLK2, J7 CPU DVFS (FAN53555 + CPR).
- J5 Rev00 units have no NFC; J3 Pro Rev02/03 differ only in proximity thresholds.
- Checks the user can run on an Android-booted phone to help: `cat /sys/kernel/debug/gpio` and the
  regulator summary while: idle/screen on, cable plugged, headset plugged/unplugged (+button),
  vibration, flashlight, hand over the proximity sensor, Wi-Fi/BT on.
