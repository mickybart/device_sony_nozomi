#!/system/bin/sh

state=$(cat /sys/block/zram0/initstate)

if [ $state -eq 0 ]; then
	echo "$(getprop persist.sys.zram_size)" > /sys/block/zram0/disksize
	mkswap /dev/block/zram0
	swapon -p 100 /dev/block/zram0
fi

