#!/system/bin/sh

# zram
if (getprop persist.sys.zram_enable | grep -q true); then

	state=$(cat /sys/block/zram0/initstate || echo -1)

	if [ $state -eq 0 ]; then
		getprop persist.sys.zram_size 167772160 > /sys/block/zram0/disksize
		mkswap /dev/block/zram0
		swapon -p 100 /dev/block/zram0
	fi

fi

# zswap
if [ -d /sys/module/zswap/parameters ]; then

	getprop persist.sys.zswap_size 22 > /sys/module/zswap/parameters/max_pool_percent
	getprop persist.sys.zswap_zmin 70 > /sys/module/zswap/parameters/max_compression_ratio

fi

# vnswap
if (getprop persist.sys.vnswap_enable | grep -q true); then

	state=$(cat /sys/block/vnswap0/disksize || echo -1)

	if [ $state -eq 0 ]; then
		getprop persist.sys.vnswap_size 5242880 > /sys/block/vnswap0/disksize
                mkswap /dev/block/vnswap0
                swapon -p 80 /dev/block/vnswap0
	fi

fi

# swap: block device
if (getprop persist.sys.swap_enable | grep -q true); then

	device=$(getprop persist.sys.swap_device)

	if (blkid $device | grep -q 'TYPE="swap"'); then
		swapon -p 50 $device

		# override page-cluster and swapiness set on init.semc.rc early-boot
		# due to a low speed block device introduced
		echo 3 > /proc/sys/vm/page-cluster
		echo 70 > /proc/sys/vm/swappiness
	fi

fi

