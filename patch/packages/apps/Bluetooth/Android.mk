LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
        $(call all-java-files-under, lib/mapapi)

LOCAL_MODULE := bluetooth.mapsapi

include $(BUILD_JAVA_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
        $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := Bluetooth
LOCAL_CERTIFICATE := platform

LOCAL_JNI_SHARED_LIBRARIES := libbluetooth_jni
LOCAL_JAVA_LIBRARIES := javax.obex telephony-common bluetooth.mapsapi
LOCAL_STATIC_JAVA_LIBRARIES := com.android.vcard

LOCAL_REQUIRED_MODULES := bluetooth.default
LOCAL_MULTILIB := 32

LOCAL_PROGUARD_ENABLED := disabled

ifeq ($(strip $(BOARD_HAVE_FMRADIO_BCM)),true)
LOCAL_SRC_FILES += \
	$(call all-java-files-under, fmradio)
LOCAL_JAVA_LIBRARIES += \
	com.stericsson.hardware.fm
LOCAL_FULL_MANIFEST_FILE := $(LOCAL_PATH)/fmradio/AndroidManifest.xml
endif

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
