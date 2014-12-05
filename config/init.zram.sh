#!/system/bin/sh

ZRAM_ENABLE=$(/system/bin/getprop sys.zram.enable)
ZRAM_DISKSIZE=$(/system/bin/getprop sys.zram.disksize)


case "$ZRAM_ENABLE" in
    "true")
        echo $(($ZRAM_DISKSIZE*1024*1024)) > /sys/block/zram0/disksize
	echo 0 > /proc/sys/vm/page-cluster
        /system/bin/mkswap /dev/block/zram0
        /system/bin/swapon /dev/block/zram0
        ;;
    "false")
        /system/bin/swapoff /dev/block/zram0
        echo 1 > /sys/block/zram0/reset
        ;;
esac

exit 0
