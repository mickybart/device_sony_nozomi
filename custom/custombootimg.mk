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

MULTI_BOOTIMAGE := 

ifeq ($(TARGET_NO_MULTIKERNEL),false)
ifeq ($(BOARD_KERNEL_MSM_OC),true)
INSTALLED_BOOTIMAGE_OC_TARGET := $(INSTALLED_BOOTIMAGE_TARGET)-oc
MULTI_BOOTIMAGE := $(MULTI_BOOTIMAGE) $(INSTALLED_BOOTIMAGE_OC_TARGET)
endif

ifeq ($(BOARD_KERNEL_MSM_OC_ULTRA),true)
INSTALLED_BOOTIMAGE_OC_ULTRA_TARGET := $(INSTALLED_BOOTIMAGE_TARGET)-oc_ultra
MULTI_BOOTIMAGE := $(MULTI_BOOTIMAGE) $(INSTALLED_BOOTIMAGE_OC_ULTRA_TARGET)
endif
endif

$(INSTALLED_BOOTIMAGE_TARGET): $(MULTI_BOOTIMAGE) $(PRODUCT_OUT)/kernel $(INSTALLED_RAMDISK_TARGET) $(MKBOOTIMG) $(MINIGZIP) $(INTERNAL_BOOTIMAGE_FILES)
	$(call pretty,"Boot image: $@")
	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/kernel@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/ramdisk.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
	
ifeq ($(TARGET_NO_MULTIKERNEL),false)
ifeq ($(BOARD_KERNEL_MSM_OC),true)
$(INSTALLED_BOOTIMAGE_OC_TARGET): $(PRODUCT_OUT)/kernel-oc $(INSTALLED_RAMDISK_TARGET) $(MKBOOTIMG) $(MINIGZIP) $(INTERNAL_BOOTIMAGE_FILES)
	$(call pretty,"Boot image: $@")
	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/kernel-oc@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/ramdisk.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
endif

ifeq ($(BOARD_KERNEL_MSM_OC_ULTRA),true)
$(INSTALLED_BOOTIMAGE_OC_ULTRA_TARGET): $(PRODUCT_OUT)/kernel-oc_ultra $(INSTALLED_RAMDISK_TARGET) $(MKBOOTIMG) $(MINIGZIP) $(INTERNAL_BOOTIMAGE_FILES)
	$(call pretty,"Boot image: $@")
	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/kernel-oc_ultra@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/ramdisk.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
endif
endif

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

