LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE       := losetup-static
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH  := $(TARGET_ROOT_OUT_SBIN)
LOCAL_SRC_FILES    := losetup-static
include $(BUILD_PREBUILT)
