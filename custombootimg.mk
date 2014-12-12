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
INITSH := device/sony/nozomi/combinedroot/init.sh
BOOTREC_DEVICE := device/sony/nozomi/recovery/bootrec-device

MULTI_KERNEL := $(PRODUCT_OUT)/kernel

ifeq ($(TARGET_NO_MULTIKERNEL),false)
ifeq ($(BOARD_KERNEL_MSM_OC),true)
MULTI_KERNEL := $(MULTI_KERNEL) $(PRODUCT_OUT)/kernel-oc
endif

ifeq ($(BOARD_KERNEL_MSM_OC_ULTRA),true)
MULTI_KERNEL := $(MULTI_KERNEL) $(PRODUCT_OUT)/kernel-oc_ultra
endif
endif

ifneq ($(TARGET_NO_RECOVERY),true)
$(INSTALLED_BOOTIMAGE_TARGET): $(MULTI_KERNEL) $(INSTALLED_RECOVERYIMAGE_TARGET) $(INSTALLED_RAMDISK_TARGET) $(INITSH) $(BOOTREC_DEVICE) $(PRODUCT_OUT)/utilities/busybox $(PRODUCT_OUT)/utilities/extract_elf_ramdisk $(MKBOOTIMG) $(MINIGZIP) $(INTERNAL_BOOTIMAGE_FILES)
	$(call pretty,"Boot image: $@")

	$(hide) rm -fr $(PRODUCT_OUT)/combinedroot
	$(hide) mkdir -p $(PRODUCT_OUT)/combinedroot/sbin

	$(hide) mv $(PRODUCT_OUT)/root/logo.rle $(PRODUCT_OUT)/combinedroot/logo.rle
	$(hide) zcat $(INSTALLED_RAMDISK_TARGET) > $(PRODUCT_OUT)/combinedroot/sbin/ramdisk.cpio
	$(hide) if [ -f $(recovery_ramdisk) ]; then zcat $(recovery_ramdisk) > $(PRODUCT_OUT)/combinedroot/sbin/ramdisk-recovery.cpio; fi
	$(hide) cp $(PRODUCT_OUT)/utilities/busybox $(PRODUCT_OUT)/combinedroot/sbin/
	$(hide) cp $(PRODUCT_OUT)/utilities/extract_elf_ramdisk $(PRODUCT_OUT)/combinedroot/sbin/

	$(hide) cp $(INITSH) $(PRODUCT_OUT)/combinedroot/sbin/init.sh
	$(hide) chmod 755 $(PRODUCT_OUT)/combinedroot/sbin/init.sh
	$(hide) ln -s sbin/init.sh $(PRODUCT_OUT)/combinedroot/init
	$(hide) cp $(BOOTREC_DEVICE) $(PRODUCT_OUT)/combinedroot/sbin/

	$(hide) $(MKBOOTFS) $(PRODUCT_OUT)/combinedroot/ | $(MINIGZIP) > $(PRODUCT_OUT)/combinedroot.img

	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/kernel@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/combinedroot.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
	
ifeq ($(TARGET_NO_MULTIKERNEL),false)
ifeq ($(BOARD_KERNEL_MSM_OC),true)
	$(call pretty,"Boot image: $@-oc")
	$(hide) rm -f $@-oc
	$(hide) python $(MKELF) -o $@-oc $(PRODUCT_OUT)/kernel-oc@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/combinedroot.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
endif

ifeq ($(BOARD_KERNEL_MSM_OC_ULTRA),true)
	$(call pretty,"Boot image: $@-oc_ultra")
	$(hide) rm -f $@-oc_ultra
	$(hide) python $(MKELF) -o $@-oc_ultra $(PRODUCT_OUT)/kernel-oc_ultra@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/combinedroot.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
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

else
$(INSTALLED_BOOTIMAGE_TARGET): $(PRODUCT_OUT)/kernel $(INSTALLED_RAMDISK_TARGET) $(MKBOOTIMG) $(MINIGZIP) $(INTERNAL_BOOTIMAGE_FILES)
	$(call pretty,"Boot image: $@")
	$(hide) python $(MKELF) -o $@ $(PRODUCT_OUT)/kernel@$(BOARD_KERNEL_ADDR) $(PRODUCT_OUT)/ramdisk.img@$(BOARD_RAMDISK_ADDR),ramdisk $(RPMBIN)@$(BOARD_RPM_ADDR),rpm
endif
