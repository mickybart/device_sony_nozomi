ifeq ($(strip $(BOARD_HAVE_FMRADIO)),true)
LOCAL_PATH := $(call my-dir)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif