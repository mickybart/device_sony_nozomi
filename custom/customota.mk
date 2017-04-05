LOCAL_PATH := $(call my-dir)

name := nAOSProm-7.1.2
ifeq ($(TARGET_BUILD_TYPE),debug)
  name := $(name)_debug
endif
name := $(name)-b$(ROM_BUILD_NUM)

INTERNAL_OTA_PACKAGE_TARGET := $(PRODUCT_OUT)/$(name).zip

$(INTERNAL_OTA_PACKAGE_TARGET): KEY_CERT_PAIR := $(DEFAULT_KEY_CERT_PAIR)

$(INTERNAL_OTA_PACKAGE_TARGET): $(BUILT_TARGET_FILES_PACKAGE) $(DISTTOOLS)
	@echo "Package OTA: $@"
	$(hide) MKBOOTIMG=$(BOARD_CUSTOM_BOOTIMG_MK) \
	   ./build/tools/releasetools/ota_from_target_files -v \
	   --block \
	   -n \
	   -p $(HOST_OUT) \
	   -k $(KEY_CERT_PAIR) \
	   --no_separate_recovery=true \
	   --custom_recovery_partition=/dev/block/mmcblk0p11 \
	   --backup=true \
	   --no_preserve_themes \
	   --resize_system=true \
	   $(BUILT_TARGET_FILES_PACKAGE) $@

