# Inherit some common CM stuff.
$(call inherit-product, vendor/cm/config/common_full_phone.mk)

# Enhanced NFC
$(call inherit-product, vendor/cm/config/nfc_enhanced.mk)

# Inherit device configuration
$(call inherit-product, device/sony/nozomi/full_nozomi.mk)

## Device identifier. This must come after all inclusions
PRODUCT_DEVICE := nozomi
PRODUCT_NAME := cm_nozomi
PRODUCT_BRAND := Sony
PRODUCT_MODEL := Xperia S
PRODUCT_MANUFACTURER := Sony

# Enable Torch
PRODUCT_PACKAGES += Torch

