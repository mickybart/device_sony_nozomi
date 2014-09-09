ifneq ($(filter nozomi,$(TARGET_DEVICE)),)
LOCAL_PATH := $(call my-dir)
include $(call all-makefiles-under,$(LOCAL_PATH))
include kernel/sony/msm8x60/AndroidKernel.mk
endif
