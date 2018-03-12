# Copyright (C) 2012 FXP (FreeXperia)
# Copyright (C) 2013 The Open SEMC Team
# Copyright (C) 2014 nAOSP (near AOSP ROM)
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

LOCAL_PATH := $(call my-dir)

MKELF := device/sony/nozomi/tools/mkelf.py
RPMBIN := vendor/sony/nozomi/proprietary/boot/RPM.bin

$(INSTALLED_BOOTIMAGE_TARGET): $(PRODUCT_OUT)/kernel $(INSTALLED_RAMDISK_TARGET) $(MKBOOTIMG) $(MINIGZIP) $(INTERNAL_BOOTIMAGE_FILES)
	$(call pretty,"Target boot image: $@")
	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/kernel@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/ramdisk.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE))

ifeq ($(TARGET_RECOVERY_TWRP),true)
$(INSTALLED_RECOVERYIMAGE_TARGET): $(PRODUCT_OUT)/kernel $(MKBOOTFS) $(MKBOOTIMG) $(MINIGZIP) $(ADBD) \
		$(INSTALLED_RAMDISK_TARGET) \
		$(INSTALLED_BOOTIMAGE_TARGET) \
		$(INTERNAL_RECOVERYIMAGE_FILES) \
		$(recovery_initrc) $(recovery_sepolicy) $(recovery_kernel) \
		$(INSTALLED_2NDBOOTLOADER_TARGET) \
		$(recovery_build_prop) $(recovery_resource_deps) \
		$(recovery_fstab) \
		$(RECOVERY_INSTALL_OTA_KEYS) \
		$(INSTALLED_VENDOR_DEFAULT_PROP_TARGET) \
		$(BOARD_RECOVERY_KERNEL_MODULES) \
		$(DEPMOD)
	@echo ----- Making recovery image ------
	$(hide) mkdir -p $(TARGET_RECOVERY_OUT)
	$(hide) mkdir -p $(TARGET_RECOVERY_ROOT_OUT)/etc $(TARGET_RECOVERY_ROOT_OUT)/sdcard $(TARGET_RECOVERY_ROOT_OUT)/tmp
	@echo Copying baseline ramdisk...
	# Use rsync because "cp -Rf" fails to overwrite broken symlinks on Mac.
	$(hide) rsync -a --exclude=etc --exclude=sdcard $(IGNORE_RECOVERY_SEPOLICY) $(IGNORE_CACHE_LINK) $(TARGET_ROOT_OUT) $(TARGET_RECOVERY_OUT)
	# Copy adbd from system/bin to recovery/root/sbin
	$(hide) cp -f $(TARGET_OUT_EXECUTABLES)/adbd $(TARGET_RECOVERY_ROOT_OUT)/sbin/adbd
	@echo Modifying ramdisk contents...
	$(hide) rm -f $(TARGET_RECOVERY_ROOT_OUT)/init*.rc
	$(hide) cp -f $(recovery_initrc) $(TARGET_RECOVERY_ROOT_OUT)/
	$(hide) cp $(TARGET_ROOT_OUT)/init.recovery.*.rc $(TARGET_RECOVERY_ROOT_OUT)/ || true # Ignore error when the src file doesn't exist.
	$(hide) mkdir -p $(TARGET_RECOVERY_ROOT_OUT)/res
	$(hide) rm -rf $(TARGET_RECOVERY_ROOT_OUT)/res/*
	$(hide) cp -rf $(recovery_resources_common)/* $(TARGET_RECOVERY_ROOT_OUT)/res
	$(hide) cp -f $(recovery_font) $(TARGET_RECOVERY_ROOT_OUT)/res/images/font.png
	$(hide) $(foreach item,$(TARGET_PRIVATE_RES_DIRS), \
		cp -rf $(item) $(TARGET_RECOVERY_ROOT_OUT)/$(newline))
	$(hide) $(foreach item,$(recovery_fstab), \
		cp -f $(item) $(TARGET_RECOVERY_ROOT_OUT)/etc/recovery.fstab)
	$(hide) cp -f $(TWRP_RECOVERY_FSTAB) $(TARGET_RECOVERY_ROOT_OUT)/etc/twrp.fstab
	$(hide) cp $(RECOVERY_INSTALL_OTA_KEYS) $(TARGET_RECOVERY_ROOT_OUT)/res/keys
	$(hide) cat $(INSTALLED_DEFAULT_PROP_TARGET) \
		> $(TARGET_RECOVERY_ROOT_OUT)/prop.default
	$(if $(INSTALLED_VENDOR_DEFAULT_PROP_TARGET), \
		$(hide) cat $(INSTALLED_VENDOR_DEFAULT_PROP_TARGET) \
			>> $(TARGET_RECOVERY_ROOT_OUT)/prop.default)
	$(hide) cat $(recovery_build_props) \
		>> $(TARGET_RECOVERY_ROOT_OUT)/prop.default
	$(hide) ln -sf prop.default $(TARGET_RECOVERY_ROOT_OUT)/default.prop
	$(BOARD_RECOVERY_IMAGE_PREPARE)
	$(hide) rm -f $(TARGET_RECOVERY_ROOT_OUT)/system/bin
	$(hide) ln -s ../sbin $(TARGET_RECOVERY_ROOT_OUT)/system/bin
	$(hide) rm -f $(TARGET_RECOVERY_ROOT_OUT)/sbin/extract_elf_ramdisk
	$(hide) $(MKBOOTFS) -d $(TARGET_OUT) $(TARGET_RECOVERY_ROOT_OUT) | $(MINIGZIP) > $(recovery_ramdisk)
	$(hide) touch $(PRODUCT_OUT)/fake-kernel
	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/fake-kernel@$(BOARD_KERNEL_ADDR) $(recovery_ramdisk)@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
	$(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE))
	@echo ----- Made recovery image: $@ --------
else
$(INSTALLED_RECOVERYIMAGE_TARGET): $(INSTALLED_RAMDISK_TARGET) \
		$(recovery_kernel) \
		$(recovery_fstab)
	$(call pretty,"Fake recovery image: $@")
	$(hide) rm -rf $(TARGET_RECOVERY_OUT)
	$(hide) mkdir -p $(TARGET_RECOVERY_ROOT_OUT)
	$(hide) mkdir -p $(TARGET_RECOVERY_ROOT_OUT)/etc
	$(hide) mkdir -p $(TARGET_RECOVERY_ROOT_OUT)/res
	$(hide) touch $(TARGET_RECOVERY_ROOT_OUT)/res/use_fotakernel
	$(hide) $(foreach item,$(recovery_fstab), \
	  cp -f $(item) $(TARGET_RECOVERY_ROOT_OUT)/etc/recovery.fstab)	
	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/kernel@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/ramdisk.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
	$(hide) $(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE),raw)
endif

