#!/bin/bash
# adb wrapper: the shell has LD_PRELOAD=libproxychains which breaks adb
unset LD_PRELOAD
exec timeout ${ADB_T:-120} adb "$@"
