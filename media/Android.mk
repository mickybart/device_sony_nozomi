QCOM_MEDIA_ROOT := $(call my-dir)
ifneq ($(filter msm8660,$(TARGET_BOARD_PLATFORM)),)
include $(QCOM_MEDIA_ROOT)/mm-video-legacy/Android.mk
include $(QCOM_MEDIA_ROOT)/libc2dcolorconvert/Android.mk
endif
