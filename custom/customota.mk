LOCAL_PATH := $(call my-dir)

name := nAOSProm-5.0
ifeq ($(TARGET_BUILD_TYPE),debug)
  name := $(name)_debug
endif
name := $(name)-bxx

INTERNAL_OTA_PACKAGE_TARGET := $(PRODUCT_OUT)/$(name).zip

$(INTERNAL_OTA_PACKAGE_TARGET): KEY_CERT_PAIR := $(DEFAULT_KEY_CERT_PAIR)

MULTI_BOOT := boot.img

ifeq ($(TARGET_NO_MULTIKERNEL),false)
ifeq ($(BOARD_KERNEL_MSM_OC),true)
MULTI_BOOT := $(MULTI_BOOT),boot.img-oc
endif

ifeq ($(BOARD_KERNEL_MSM_OC_ULTRA),true)
MULTI_BOOT := $(MULTI_BOOT),boot.img-oc_ultra
endif
endif

ifeq ($(TARGET_NO_MULTIKERNEL),true)
$(INTERNAL_OTA_PACKAGE_TARGET): $(BUILT_TARGET_FILES_PACKAGE) $(DISTTOOLS)
	@echo "Package OTA: $@"
	$(hide) MKBOOTIMG=$(BOARD_CUSTOM_BOOTIMG_MK) \
	   ./build/tools/releasetools/ota_from_target_files -v \
	   -p $(HOST_OUT) \
	   -k $(KEY_CERT_PAIR) \
	   --no_separate_recovery=true \
	   $(BUILT_TARGET_FILES_PACKAGE) $@
else
$(INTERNAL_OTA_PACKAGE_TARGET): $(BUILT_TARGET_FILES_PACKAGE) $(DISTTOOLS)
	@echo "Package OTA: $@"
	$(hide) MKBOOTIMG=$(BOARD_CUSTOM_BOOTIMG_MK) \
	   ./build/tools/releasetools/ota_from_target_files -v \
	   -p $(HOST_OUT) \
	   -k $(KEY_CERT_PAIR) \
	   --no_separate_recovery=true \
	   --multiple_boot=$(MULTI_BOOT) \
	   --multiple_boot_scripts=device/sony/nozomi/custom \
	   $(BUILT_TARGET_FILES_PACKAGE) $@
endif

