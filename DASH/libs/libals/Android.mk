ifeq ($(SOMC_CFG_SENSORS_LIGHT_LIBALS),yes)

LOCAL_PATH := $(call my-dir)

ifeq ($(SOMC_CFG_SENSORS_HAVE_LIBALS), yes)
# Add prebuilt libraries if available

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := libals.so
include $(BUILD_MULTI_PREBUILT)

else
# Or else build the dummy lib

include $(CLEAR_VARS)
LOCAL_MODULE := libals
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES += libals_dummy.c
include $(BUILD_SHARED_LIBRARY)

endif

endif

