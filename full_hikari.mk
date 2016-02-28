#
# Copyright 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Get the long list of APNs
PRODUCT_COPY_FILES += device/sample/etc/apns-full-conf.xml:system/etc/apns-conf.xml

# Override the audio_effects.conf
PRODUCT_COPY_FILES += device/sony/hikari/config/audio_effects.conf:system/etc/audio_effects.conf

# Inherit from the common Open Source product configuration
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_base_telephony.mk)

# Discard inherited values and use our own instead.
PRODUCT_NAME := full_hikari
PRODUCT_DEVICE := hikari
PRODUCT_BRAND := Sony
PRODUCT_MODEL := Xperia Acro S
PRODUCT_MANUFACTURER := Sony
PRODUCT_RESTRICT_VENDOR_FILES := false

#Disable Data roaming by default
PRODUCT_PROPERTY_OVERRIDES := \
    ro.com.android.dataroaming=false

# Inherit from hardware-specific part of the product configuration
$(call inherit-product, device/sony/hikari/device.mk)
$(call inherit-product-if-exists, vendor/sony/hikari/device-vendor.mk)
