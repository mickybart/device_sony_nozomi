LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_SRC_FILES += \
	src/com/stericsson/hardware/fm/IFmReceiver.aidl \
	src/com/stericsson/hardware/fm/IFmTransmitter.aidl \
	src/com/stericsson/hardware/fm/IOnAutomaticSwitchListener.aidl \
	src/com/stericsson/hardware/fm/IOnBlockScanListener.aidl \
	src/com/stericsson/hardware/fm/IOnErrorListener.aidl \
	src/com/stericsson/hardware/fm/IOnExtraCommandListener.aidl \
	src/com/stericsson/hardware/fm/IOnForcedPauseListener.aidl \
	src/com/stericsson/hardware/fm/IOnForcedResetListener.aidl \
	src/com/stericsson/hardware/fm/IOnRDSDataFoundListener.aidl \
	src/com/stericsson/hardware/fm/IOnScanListener.aidl \
	src/com/stericsson/hardware/fm/IOnSignalStrengthListener.aidl \
	src/com/stericsson/hardware/fm/IOnStartedListener.aidl \
	src/com/stericsson/hardware/fm/IOnStateChangedListener.aidl \
	src/com/stericsson/hardware/fm/IOnStereoListener.aidl
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := com.stericsson.hardware.fm
include $(BUILD_JAVA_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := com.stericsson.hardware.fm.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/permissions
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)
