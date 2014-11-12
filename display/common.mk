#Common headers
common_includes := device/sony/nozomi/display/libgralloc
common_includes += device/sony/nozomi/display/liboverlay
common_includes += device/sony/nozomi/display/libcopybit
common_includes += device/sony/nozomi/display/libqdutils
common_includes += device/sony/nozomi/display/libhwcomposer
common_includes += device/sony/nozomi/display/libexternal
common_includes += device/sony/nozomi/display/libqservice

common_header_export_path := qcom/display

#Common libraries external to display HAL
common_libs := liblog libutils libcutils libhardware

#Common C flags
common_flags := -DDEBUG_CALC_FPS -Wno-missing-field-initializers
common_flags += -Werror -Wno-unused-parameter

ifeq ($(ARCH_ARM_HAVE_NEON),true)
    common_flags += -D__ARM_HAVE_NEON
endif

ifneq ($(filter msm8974 msm8x74 msm8226 msm8x26,$(TARGET_BOARD_PLATFORM)),)
    common_flags += -DVENUS_COLOR_FORMAT
    common_flags += -DMDSS_TARGET
endif

common_deps  :=
kernel_includes := 

# Executed only on QCOM BSPs
ifeq ($(call is-vendor-board-platform,QCOM),true)
    common_flags += -DQCOM_BSP
    common_deps += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
    kernel_includes += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
endif
