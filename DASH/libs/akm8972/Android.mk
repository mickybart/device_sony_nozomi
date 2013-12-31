ifeq ($(SOMC_CFG_SENSORS_COMPASS_AK8972),yes) 

LOCAL_PATH := $(call my-dir)

ifeq ($(SOMC_CFG_SENSORS_HAVE_LIBAK8972), yes)
# Add prebuilt libraries if available

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PREBUILT_LIBS := libsensors_akm8972.so
include $(BUILD_MULTI_PREBUILT)

else
# Or else build the dummy lib

include $(CLEAR_VARS)

LOCAL_MODULE := libsensors_akm8972
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES += libsensors_akm8972_dummy.c
include $(BUILD_SHARED_LIBRARY)

endif

endif
