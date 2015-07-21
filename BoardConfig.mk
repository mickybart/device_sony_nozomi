# Copyright 2010 The Android Open Source Project
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
# This file sets variables that control the way modules are built
# thorughout the system. It should not be used to conditionally
# disable makefiles (the proper mechanism to control what gets
# included in a build is to use PRODUCT_PACKAGES in a product
# definition file).
#

# cpu
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_SMP := true
TARGET_CPU_VARIANT := cortex-a8
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_ARCH_VARIANT_CPU := cortex-a8
ARCH_ARM_HAVE_TLS_REGISTER := true

# compile flag
TARGET_GLOBAL_CFLAGS += -mfpu=neon -mfloat-abi=softfp
TARGET_GLOBAL_CPPFLAGS += -mfpu=neon -mfloat-abi=softfp
COMMON_GLOBAL_CFLAGS += -DLEGACY_BLOB_COMPATIBLE

# display
USE_OPENGL_RENDERER := true
BOARD_EGL_CFG := device/sony/nozomi/config/egl.cfg

TARGET_USES_ION := true
TARGET_USES_OVERLAY := true
TARGET_USES_SF_BYBASS := true
TARGET_USES_C2D_COMPOSITION := true

# audio
BOARD_USES_ALSA_AUDIO := true

# bluetooth
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR ?= device/sony/nozomi/bluetooth

# sensor
BOARD_USES_GENERIC_INVENSENSE := false
BOARD_USES_SENSORS_DASH := yes
SOMC_CFG_SENSORS_ACCEL_BMA250NA_INPUT := yes
SOMC_CFG_SENSORS_COMPASS_AK8972 := yes
SOMC_CFG_SENSORS_LIGHT_LIBALS := yes
SOMC_CFG_SENSORS_PROXIMITY_APDS9702 := yes
SOMC_CFG_SENSORS_GYRO_MPU3050 := yes
SOMC_CFG_SENSORS_PICKUP_BMA250NA_MOTION := yes

# wifi
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WLAN_DEVICE           := bcmdhd
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
WIFI_DRIVER_FW_PATH_PARAM   := "/sys/module/bcmdhd/parameters/firmware_path"
WIFI_DRIVER_FW_PATH_STA     := "/system/vendor/firmware/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_AP      := "/system/vendor/firmware/fw_bcmdhd_apsta.bin"
WIFI_DRIVER_FW_PATH_P2P     := "/system/vendor/firmware/fw_bcmdhd.bin"

# camera
BOARD_USES_CAMERA_FAST_AUTOFOCUS := true
USE_DEVICE_SPECIFIC_CAMERA := true

# fm radio
BOARD_HAVE_FMRADIO := true
BOARD_HAVE_FMRADIO_BCM := true

# kernel
BOARD_KERNEL_MSM := true
KERNEL_DEFCONFIG := fuji_nozomi_defconfig

# board
TARGET_BOARD_PLATFORM := msm8660
TARGET_BOOTLOADER_BOARD_NAME := fuji
TARGET_VENDOR_PLATFORM := fuji
TARGET_BOARD_PLATFORM_GPU := qcom-adreno200

## Enable dex-preoptimization to speed up first boot sequence
#ifeq ($(HOST_OS),linux)
#  ifeq ($(TARGET_BUILD_VARIANT),user)
#    ifeq ($(WITH_DEXPREOPT),)
#      WITH_DEXPREOPT := true
#    endif
#  endif
#endif
#DONT_DEXPREOPT_PREBUILTS := true

TARGET_NO_BOOTLOADER := true
TARGET_NO_RECOVERY := false
TARGET_NO_RADIOIMAGE := true
TARGET_BOOTLOADER_TYPE := fastboot
BOARD_HAS_NO_MISC_PARTITION := true

# boot image
BOARD_KERNEL_ADDR	:= 0x40208000
BOARD_RAMDISK_ADDR	:= 0x41500000
BOARD_RPM_ADDR		:= 0x20000

# image
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1056964608
BOARD_USERDATAIMAGE_PARTITION_SIZE := 2147483648
BOARD_FLASH_BLOCK_SIZE := 131072

# fs (ext4,f2fs,dynamic)
BUILD_FS := dynamic

# fstab
TARGET_RECOVERY_FSTAB := device/sony/nozomi/config/recovery.$(BUILD_FS).fstab
RECOVERY_FSTAB_VERSION := 2

# OTA
TARGET_OTA_ASSERT_DEVICE := LT26i,nozomi

# custom boot
BOARD_CUSTOM_BOOTIMG_MK := device/sony/nozomi/custom/custombootimg.mk

# custom ota
BOARD_CUSTOM_OTA_MK := device/sony/nozomi/custom/customota.mk

# Superuser
TARGET_NO_SUPERUSER := false
ifneq ($(TARGET_NO_SUPERUSER),true)
SUPERUSER_EMBEDDED := true
endif

-include vendor/sony/nozomi/BoardConfigVendor.mk

# Enable Minikin text layout engine (will be the default soon)
USE_MINIKIN := true

# Include an expanded selection of fonts
EXTENDED_FONT_FOOTPRINT := true

