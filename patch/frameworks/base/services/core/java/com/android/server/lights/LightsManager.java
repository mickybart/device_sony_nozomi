/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server.lights;

public abstract class LightsManager {
    public static final int LIGHT_ID_BACKLIGHT = 0;
    public static final int LIGHT_ID_KEYBOARD = 1;
    public static final int LIGHT_ID_BUTTONS = 2;
    public static final int LIGHT_ID_BATTERY = 3;
    public static final int LIGHT_ID_NOTIFICATIONS = 4;
    public static final int LIGHT_ID_ATTENTION = 5;
    public static final int LIGHT_ID_BLUETOOTH = 6;
    public static final int LIGHT_ID_WIFI = 7;
    public static final int LIGHT_ID_CAPS = 8;
    public static final int LIGHT_ID_FUNC = 9;
    public static final int LIGHT_ID_COUNT = 10;
    
    public static final int BACKLIGHT_LINKED = 256;
    
    private int mColorLcdBacklight = 0;
    private int mColorButtonBacklight = 0;
    private final Object mLock = new Object();

    public abstract Light getLight(int id);
    
    public void setBacklightBrightness(int brightness) {
        synchronized (mLock) {
            if (mColorLcdBacklight != brightness) {
                mColorLcdBacklight = brightness;
                
                getLight(LIGHT_ID_BACKLIGHT).setBrightness(brightness);
                
                if (mColorButtonBacklight == BACKLIGHT_LINKED) {
                    getLight(LIGHT_ID_BUTTONS).setBrightness(brightness);
                }
            }
        }
    }
    
    public void turnOnButtons(int brightness) {
        synchronized (mLock) {
            if (mColorButtonBacklight != brightness) {
                mColorButtonBacklight = brightness;
                
                if (mColorButtonBacklight == BACKLIGHT_LINKED) {
                    getLight(LIGHT_ID_BUTTONS).setBrightness(mColorLcdBacklight);
                } else {
                    getLight(LIGHT_ID_BUTTONS).setBrightness(mColorButtonBacklight);
                }
            }
        }
    }
    
    public void turnOffButtons() {
        synchronized (mLock) {
            mColorButtonBacklight = 0;
            getLight(LIGHT_ID_BUTTONS).turnOff();
        }
    }
}
