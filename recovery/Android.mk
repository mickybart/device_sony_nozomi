ifneq ($(filter nozomi,$(TARGET_DEVICE)),)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PATH_SAVE    := $(LOCAL_PATH)
LOCAL_PATH         := $(PRODUCT_OUT)/utilities
LOCAL_MODULE       := busybox-static
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH  := $(TARGET_ROOT_OUT_SBIN)
LOCAL_SRC_FILES    := busybox
include $(BUILD_PREBUILT)
LOCAL_PATH         := $(LOCAL_PATH_SAVE)

include $(CLEAR_VARS)
LOCAL_MODULE       := init.sh
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH  := $(TARGET_ROOT_OUT)
LOCAL_SRC_FILES    := init.sh
include $(BUILD_PREBUILT)

include $(call all-makefiles-under,$(LOCAL_PATH))
endif
