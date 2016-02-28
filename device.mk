#
# Copyright 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Local variables (can be duplicate from BoardConfig.mk)
TARGET_NO_SUPERUSER := false
TARGET_RECOVERY_TWRP := true

# overlay
DEVICE_PACKAGE_OVERLAYS += device/sony/hikari/overlay

PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xhdpi

PRODUCT_CHARACTERISTICS := nosdcard

# kernel
PRODUCT_PACKAGES += \
    kernel

# Light
PRODUCT_PACKAGES += \
    lights.semc

# Display
PRODUCT_PACKAGES += \
    liboverlay \
    hwcomposer.msm8660 \
    gralloc.msm8660 \
    memtrack.msm8660 \
    copybit.msm8660

# Power
PRODUCT_PACKAGES += \
    power.msm8660

# Media
PRODUCT_PACKAGES += \
    libdivxdrmdecrypt \
    libOmxVdec \
    libOmxVenc \
    libOmxCore \
    libstagefrighthw \
    libc2dcolorconvert

# Audio
PRODUCT_PACKAGES += \
    audio.primary.msm8660 \
    audio.a2dp.default \
    audio.usb.default \
    audio.r_submix.default \
    libaudio-resampler

# Audio Effects
PRODUCT_PACKAGES += \
    ViPER4Android \
    libv4a_fx_ics \
    libqcomvoiceprocessing

# Stlport for proprietary Adreno OpenGL lib
PRODUCT_PACKAGES +=  \
    libstlport \
    libCrypto

PRODUCT_COPY_FILES += \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:system/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
    $(LOCAL_PATH)/config/media_codecs.xml:system/etc/media_codecs.xml \
    $(LOCAL_PATH)/config/media_codecs_performance.xml:system/etc/media_codecs_performance.xml \
    $(LOCAL_PATH)/config/media_profiles.xml:system/etc/media_profiles.xml \
    $(LOCAL_PATH)/config/audio_policy.conf:system/etc/audio_policy.conf \
    $(LOCAL_PATH)/config/mixer_paths.xml:system/etc/mixer_paths.xml

# Reduce client buffer size for fast audio output tracks
PRODUCT_PROPERTY_OVERRIDES += \
    af.fast_track_multiplier=1

# NFC
PRODUCT_PACKAGES += \
    nfc.msm8660 \
    libnfc \
    libnfc_jni \
    Nfc \
    Tag \
    com.android.nfc_extras

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/nfcee_access.xml:system/etc/nfcee_access.xml

# Bluetooth
PRODUCT_PACKAGES += \
    libbt-vendor \
    libbluedroid \
    brcm_patchram_plus \
    bt_vendor.conf

# Wifi
PRODUCT_PACKAGES += \
    libnetcmdiface \
    libwpa_client \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/calibration:system/etc/wifi/calibration

WIFI_BAND := 802_11_BG
$(call inherit-product-if-exists, hardware/broadcom/wlan/bcmdhd/firmware/bcm4330/device-bcm.mk)

# DASH
PRODUCT_PACKAGES += \
    sensors.msm8660

PRODUCT_COPY_FILES += \
   $(LOCAL_PATH)/config/sensors.conf:system/etc/sensors.conf

# GPS
PRODUCT_COPY_FILES += \
     $(LOCAL_PATH)/config/gps.conf:system/etc/gps.conf

# These are the hardware-specific features
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.audio.low_latency.xml:system/etc/permissions/android.hardware.audio.low_latency.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.software.print.xml:system/etc/permissions/android.software.print.xml \
    frameworks/native/data/etc/android.software.midi.xml:system/etc/permissions/android.software.midi.xml \
    frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/com.android.nfc_extras.xml:system/etc/permissions/com.android.nfc_extras.xml \
    frameworks/native/data/etc/com.nxp.mifare.xml:system/etc/permissions/com.nxp.mifare.xml

# Configuration scripts
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/flashled_calc_parameters.cfg:system/etc/flashled_calc_parameters.cfg \
    $(LOCAL_PATH)/config/iddd.conf:system/etc/iddd.conf \
    $(LOCAL_PATH)/config/qosmgr_rules.xml:system/etc/qosmgr_rules.xml \

# Common Qualcomm / Sony scripts
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/init.netconfig.sh:system/etc/init.netconfig.sh \
    $(LOCAL_PATH)/config/init.swap.sh:root/init.swap.sh \
    $(LOCAL_PATH)/config/hw_config.sh:system/etc/hw_config.sh \
    $(LOCAL_PATH)/config/clearpad_fwloader.sh:system/etc/clearpad_fwloader.sh

# Custom init / uevent
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/init.semc.rc:root/init.semc.rc \
    $(LOCAL_PATH)/config/ueventd.semc.rc:root/ueventd.semc.rc \
    $(LOCAL_PATH)/config/init.semc.usb.rc:root/init.semc.usb.rc

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/fstab.semc:root/fstab.semc

# Recovery: Custom init for twrp
ifeq ($(TARGET_RECOVERY_TWRP),true)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/init.recovery.semc.rc:root/init.recovery.semc.rc
endif

# USB
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp

PRODUCT_PACKAGES += \
    com.android.future.usb.accessory

# Fm radio
PRODUCT_PACKAGES += \
    FMRadio

# Key layouts and touchscreen
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/clearpad.idc:system/usr/idc/clearpad.idc \
    $(LOCAL_PATH)/config/clearpad.kl:system/usr/keylayout/clearpad.kl \
    $(LOCAL_PATH)/config/fuji-keypad.kl:system/usr/keylayout/fuji-keypad.kl \
    $(LOCAL_PATH)/config/gpio-key.kl:system/usr/keylayout/gpio-key.kl \
    $(LOCAL_PATH)/config/keypad-pmic-fuji.kl:system/usr/keylayout/keypad-pmic-fuji.kl \
    $(LOCAL_PATH)/config/pmic8058_pwrkey.kl:system/usr/keylayout/pmic8058_pwrkey.kl \
    $(LOCAL_PATH)/config/simple_remote.kl:system/usr/keylayout/simple_remote.kl \
    $(LOCAL_PATH)/config/simple_remote_appkey.kl:system/usr/keylayout/simple_remote_appkey.kl

# Software
PRODUCT_PACKAGES += \
    libemoji \
    Email \
    Stk

# Busybox
PRODUCT_PACKAGES += \
    busybox

# Boot Animation
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/bootanimation.zip:system/media/bootanimation.zip

# Hw keys
# layout : 1=> Modern Android 4.4+, 0=> Deprecated Android 2.3
# music  : control music with volume and camera keys (1=> enable, 0=> disable)
PRODUCT_PROPERTY_OVERRIDES += \
    persist.qemu.hw.mainkeys=1 \
    persist.qemu.hw.mainkeys_layout=1 \
    persist.qemu.hw.mainkeys_music=1 \
    persist.qemu.hw.camerakey=1

# Recovery
PRODUCT_PACKAGES += \
    busybox-recovery \
    extract_elf_ramdisk \
    init.sh

# Superuser
ifneq ($(TARGET_NO_SUPERUSER),true)

PRODUCT_PACKAGES += \
    su \
    supolicy \
    libsupol

PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.root_access=3

endif

# Filesystem tools
PRODUCT_PACKAGES += \
    e2fsck \
    fsck_msdos \
    fsck.f2fs \
    mkfs.f2fs \
    make_f2fs

# Wake Up
PRODUCT_PACKAGES += \
    WakeUp

# Live Wallpapers
PRODUCT_PACKAGES += \
    LiveWallpapersPicker \
    librs_jni

# Wallpaper Picker
PRODUCT_PACKAGES += \
    WallpaperPicker

# Thermal management
PRODUCT_PACKAGES += \
    thermanager

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/config/thermanager.xml:system/etc/thermanager.xml

# Terminal
PRODUCT_PACKAGES += \
    Terminal

# ROM Updater
ifeq ($(ROM_BUILD_NUM),)
  $(error No ROM_BUILD_NUM defined. please export the value (export ROM_BUILD_NUM=xx))
endif

PRODUCT_PACKAGES += \
    ROMUpdater

PRODUCT_PROPERTY_OVERRIDES += \
    ro.build.version.updater=nAOSProm-7.1.2-b$(ROM_BUILD_NUM) \
    persist.rom.updater.uri=https://localhost

# Dalvik
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.dex2oat-swap=false

PRODUCT_TAGS += dalvik.gc.type-precise

# Tethering
PRODUCT_PROPERTY_OVERRIDES += \
    net.tethering.noprovisioning=true \
    persist.sys.dun.override=0

$(call inherit-product, frameworks/native/build/phone-xhdpi-1024-dalvik-heap.mk)

# Backup Tool
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/custom/backuptool/backuptool.sh:install/bin/backuptool.sh \
    $(LOCAL_PATH)/custom/backuptool/backuptool.functions:install/bin/backuptool.functions \
    $(LOCAL_PATH)/custom/backuptool/50-base.sh:system/addon.d/50-base.sh
