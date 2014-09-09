ifneq ($(filter msm8660,$(TARGET_BOARD_PLATFORM)),)

LOCAL_PATH := $(call my-dir)

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  common_cflags += -DWITH_A2DP
endif

ifeq ($(BOARD_HAVE_QCOM_FM),true)
  common_cflags += -DQCOM_FM_ENABLED
endif

ifeq ($(BOARD_QCOM_TUNNEL_LPA_ENABLED),true)
  common_cflags += -DQCOM_TUNNEL_LPA_ENABLED
endif

ifeq ($(BOARD_QCOM_VOIP_ENABLED),true)
  common_cflags += -DQCOM_VOIP_ENABLED
endif

ifeq ($(BOARD_QCOM_TUNNEL_PLAYBACK_ENABLED),true)
  common_cflags += -DTUNNEL_PLAYBACK
endif

ifeq ($(BOARD_USES_QCOM_HARDWARE),true)
  common_cflags += -DQCOM_ACDB_ENABLED
endif

ifeq ($(BOARD_HAVE_SONY_AUDIO),true)
  common_cflags += -DSONY_AUDIO
endif

ifeq ($(BOARD_HAVE_BACK_MIC_CAMCORDER),true)
  common_cflags += -DBACK_MIC_CAMCORDER
endif


include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := -D_POSIX_SOURCE

LOCAL_SRC_FILES := \
    AudioHardware.cpp \
    audio_hw_hal.cpp

LOCAL_SHARED_LIBRARIES := \
    libcutils       \
    libutils        \
    libmedia

LOCAL_LDFLAGS += \
    vendor/sony/$(TARGET_DEVICE)/proprietary/lib/libaudioalsa.so \
    vendor/sony/$(TARGET_DEVICE)/proprietary/lib/libacdbloader.so \
    vendor/sony/$(TARGET_DEVICE)/proprietary/lib/libacdbmapper.so

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_STATIC_LIBRARIES := \
    libmedia_helper \
    libaudiohw_legacy \
    libaudiopolicy_legacy \

LOCAL_MODULE := audio.primary.msm8660
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS += -fno-short-enums

LOCAL_C_INCLUDES += hardware/libhardware/include
LOCAL_C_INCLUDES += frameworks/base/include
LOCAL_C_INCLUDES += system/core/include

LOCAL_CFLAGS += $(common_cflags)

include $(BUILD_SHARED_LIBRARY)

endif
