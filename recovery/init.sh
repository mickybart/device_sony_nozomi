#!/sbin/busybox-static sh

###########################################################################

BUSYBOX=/sbin/busybox-static
EXTRACT_RAMDISK=/sbin/extract_elf_ramdisk
KEYCHECK=/dev/keycheck
RAMDISK=/ramdisk.cpio

LED_RED=/sys/class/leds/red/brightness
LED_GREEN=/sys/class/leds/green/brightness
LED_BLUE=/sys/class/leds/blue/brightness

###########################################################################

$BUSYBOX mknod -m 666 /dev/null c 1 3

$BUSYBOX mkdir -m 755 /recovery

# extract ram disk
$BUSYBOX mkdir -m 755 /dev/block
$BUSYBOX mknod -m 600 /dev/block/mmcblk0p11 b 179 11
$EXTRACT_RAMDISK -i /dev/block/mmcblk0p11 -o /recovery$RAMDISK -c

if [ -s /recovery$RAMDISK ]; then
    LOAD_RECOVERY=
    $BUSYBOX mount -t sysfs sysfs /sys
    $BUSYBOX mount -t proc proc /proc

    if $BUSYBOX grep -q warmboot=0x77665502 /proc/cmdline; then
        LOAD_RECOVERY=1
    else
        # trigger amber LED
        $BUSYBOX echo 255 > $LED_RED
        $BUSYBOX echo 0 > $LED_GREEN
        $BUSYBOX echo 255 > $LED_BLUE

        # key check
        $BUSYBOX mkdir -m 755 /dev/input
        $BUSYBOX mknod -m 660 /dev/input/event0 c 13 64
        $BUSYBOX cat /dev/input/event0 > $KEYCHECK&
        $BUSYBOX sleep 3
        kill $!
        if [ -s $KEYCHECK ]; then
            LOAD_RECOVERY=1
        fi
    fi

    if [ $LOAD_RECOVERY ]; then
        # trigger blue LED
        $BUSYBOX echo 0 > $LED_RED
        $BUSYBOX echo 0 > $LED_GREEN
        $BUSYBOX echo 255 > $LED_BLUE
    else
        # turn off LED
        $BUSYBOX echo 0 > $LED_RED
        $BUSYBOX echo 0 > $LED_GREEN
        $BUSYBOX echo 0 > $LED_BLUE
    fi

    $BUSYBOX umount /proc
    $BUSYBOX umount /sys

    if [ $LOAD_RECOVERY ]; then
        $BUSYBOX mkdir -m 755 /recovery/sbin
        $BUSYBOX ln $BUSYBOX /recovery$BUSYBOX
        $BUSYBOX chroot /recovery $BUSYBOX cpio -i -F $RAMDISK
        $BUSYBOX rm /recovery$RAMDISK
        exec $BUSYBOX chroot /recovery /init
    fi
fi

$BUSYBOX rm -rf /recovery
$BUSYBOX rm -rf /dev

exec /init