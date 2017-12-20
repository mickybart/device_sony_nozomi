name := nAOSProm-8.1.0
ifeq ($(TARGET_BUILD_TYPE),debug)
  name := $(name)_debug
endif
name := $(name)-b$(ROM_BUILD_NUM)

INTERNAL_OTA_PACKAGE_TARGET := $(PRODUCT_OUT)/$(name).zip

$(INTERNAL_OTA_PACKAGE_TARGET): KEY_CERT_PAIR := $(DEFAULT_KEY_CERT_PAIR)

$(INTERNAL_OTA_PACKAGE_TARGET): $(BUILT_TARGET_FILES_PACKAGE) \
                build/tools/releasetools/ota_from_target_files
	@echo "Package OTA: $@"
	PATH=$(foreach p,$(INTERNAL_USERIMAGES_BINARY_PATHS),$(p):)$$PATH MKBOOTIMG=$(BOARD_CUSTOM_BOOTIMG_MK) \
	   ./build/tools/releasetools/ota_from_target_files -v \
	   --block \
	   --extracted_input_target_files $(patsubst %.zip,%,$(BUILT_TARGET_FILES_PACKAGE)) \
	   -p $(HOST_OUT) \
	   -k $(KEY_CERT_PAIR) \
	   --no_separate_recovery=true \
	   --resize_system=true \
	   --backup=true \
	   $(if $(OEM_OTA_CONFIG), -o $(OEM_OTA_CONFIG)) \
	   $(BUILT_TARGET_FILES_PACKAGE) $@
