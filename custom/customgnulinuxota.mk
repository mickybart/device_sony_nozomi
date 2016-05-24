INTERNAL_OTA_PACKAGE_TARGET := $(PRODUCT_OUT)/$(GNULINUX_OTA_OS)-$(GNULINUX_OTA_ARCH)-$(TARGET_DEVICE).zip

$(INTERNAL_OTA_PACKAGE_TARGET): KEY_CERT_PAIR := $(DEFAULT_KEY_CERT_PAIR)

$(INTERNAL_OTA_PACKAGE_TARGET): $(DISTTOOLS) $(BUILT_TARGET_FILES_PACKAGE) $(GNULINUX_SYSTEM_IMAGE_OUT) $(GNULINUX_CHROOT_IMAGE_OUT)
	@echo "Package GNU/Linux OTA: $@"
	    ./build/tools/releasetools/ota_from_target_files -v \
            -p $(HOST_OUT) \
            -k $(KEY_CERT_PAIR) \
            -n \
            --gnulinux \
            --gnulinux_os=$(GNULINUX_OTA_OS) \
            --gnulinux_chroot="$(GNULINUX_CHROOT_IMAGE_OUT)" \
            --gnulinux_image=$(GNULINUX_SYSTEM_IMAGE_OUT) \
            --no_separate_recovery=true \
            $(BUILT_TARGET_FILES_PACKAGE) $@


