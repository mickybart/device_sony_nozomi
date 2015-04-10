/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.broadcom.fm;

public class FmHardware {

    /*FM enable, RDS enable*/
    public static final int FM_RDS_SYSTEM = 0x00;

    /*Band select, mono/stereo blend, mono/stearo select*/
    public static final int FM_CTRL = 0x01;

    /*RDS/RDBS, flush FIFO*/
    public static final int RDS_CTRL = 0x02;

    /*Pause level and time constant*/
    public static final int FM_AUDIO_PAUSE = 0x04;

    /*Mute, volume, de-emphasis, route parameters, BW select*/
    public static final int FM_AUDIO_CTRL = 0x05;

    /*Search parameters such as stop level, up/down*/
    public static final int FM_SEARCH_CTRL = 0x07;

    /*Search/tune mode and stop*/
    public static final int FM_SEARCH_TUNE_MODE = 0x09;

    /*Set and get frequency*/
    public static final int FM_FREQ = 0x0a;

    /*Set alternate jump frequency*/
    public static final int FM_AF_FREQ = 0x0c;

    /*IF frequency error*/
    public static final int FM_CARRIER = 0x0e;

    /*Recived signal strength*/
    public static final int FM_RSSI = 0x0f;

    /*FM and RDS IRQ mask register*/
    public static final int FM_RDS_MASK = 0x10;

    /*FM and RDS flag register*/
    public static final int FM_RDS_FLAG = 0x12;

    /*FIFO water line set level*/
    public static final int RDS_WLINE = 0x14;

    /*Block B match pattern*/
    public static final int RDS_BLKB_MATCH = 0x16;

    /*Block B mask pattern*/
    public static final int RDS_BLKB_MASK = 0x18;

    /*PI match pattern*/
    public static final int RDS_PI_MATCH = 0x1a;

    /*PI mask pattern*/
    public static final int RDS_PI_MASK = 0x1c;
    
    /*Controls routing of FM audio output to either PCM or Bluetooth SCO*/
    public static final int FM_PCM_ROUTE = 0x4d;

    /*Read RDS tuples(3 bytes each)*/
    public static final int RDS_DATA = 0x80;

    /*Best tune mode enable/disable for AF jump*/
    public static final int FM_BEST_TUNE = 0x90;

    /*Select search methods: normal, preset, RSSI monitoring*/
    public static final int FM_SEARCH_METHOD = 0xfc;
    
    /*Adjust search steps in units of 1kHz to 100kHz*/
    public static final int FM_SEARCH_STEPS = 0xfd;
    
    /*Sets the maximum number of preset channels found for FM scan command*/
    public static final int FM_MAX_PRESET = 0xfe;
    
    /*Read the number of preset stations returned after a FM scan command*/
    public static final int FM_PRESET_STATION = 0xff;


    public static final int FM_RDS_SYSTEM_OFF = 0x00;
    public static final int FM_RDS_SYSTEM_FM = 0x01;
    public static final int FM_RDS_SYSTEM_RDS = 0x02;

    public static final int FM_CTRL_BAND_EUROPE_US = 0x00;
    public static final int FM_CTRL_BAND_JAPAN = 0x01;
    public static final int FM_CTRL_AUTO = 0x02;
    public static final int FM_CTRL_MANUAL = 0x00;
    public static final int FM_CTRL_STEREO = 0x04;
    public static final int FM_CTRL_MONO = 0x00;
    public static final int FM_CTRL_SWITCH = 0x08;
    public static final int FM_CTRL_BLEND = 0x00;

    public static final int FM_AUDIO_CTRL_RF_MUTE_ENABLE = 0x01;
    public static final int FM_AUDIO_CTRL_RF_MUTE_DISABLE = 0x00;
    public static final int FM_AUDIO_CTRL_MANUAL_MUTE_ON = 0x02;
    public static final int FM_AUDIO_CTRL_MANUAL_MUTE_OFF = 0x00;
    public static final int FM_AUDIO_CTRL_DAC_OUT_LEFT_ON = 0x04;
    public static final int FM_AUDIO_CTRL_DAC_OUT_LEFT_OFF = 0x00;
    public static final int FM_AUDIO_CTRL_DAC_OUT_RIGHT_ON = 0x08;
    public static final int FM_AUDIO_CTRL_DAC_OUT_RIGHT_OFF = 0x00;
    public static final int FM_AUDIO_CTRL_ROUTE_DAC_ENABLE = 0x10;
    public static final int FM_AUDIO_CTRL_ROUTE_DAC_DISABLE = 0x00;
    public static final int FM_AUDIO_CTRL_ROUTE_I2S_ENABLE = 0x20;
    public static final int FM_AUDIO_CTRL_ROUTE_I2S_DISABLE = 0x00;
    public static final int FM_AUDIO_CTRL_DEMPH_75US = 0x40;
    public static final int FM_AUDIO_CTRL_DEMPH_50US = 0x00;

    public static final int FM_SEARCH_CTRL_UP = 0x80;
    public static final int FM_SEARCH_CTRL_DOWN = 0x00;

    public static final int FM_TERMINATE_SEARCH_TUNE_MODE = 0x00;
    public static final int FM_PRE_SET_MODE = 0x01;
    public static final int FM_AUTO_SEARCH_MODE = 0x02;
    public static final int FM_AF_JUMP_MODE = 0x03;

    public static final int FM_FLAG_SEARCH_TUNE_FINISHED = 0x01;
    public static final int FM_FLAG_SEARCH_TUNE_FAIL = 0x02;
    public static final int FM_FLAG_RSSI_LOW = 0x04;
    public static final int FM_FLAG_CARRIER_ERROR_HIGH = 0x08;
    public static final int FM_FLAG_AUDIO_PAUSE_INDICATION = 0x10;
    public static final int FLAG_STEREO_DETECTION = 0x20;
    public static final int FLAG_STEREO_ACTIVE = 0x40;

    public static final int RDS_FLAG_FIFO_WLINE  = 0x02;
    public static final int RDS_FLAG_B_BLOCK_MATCH = 0x08;
    public static final int RDS_FLAG_SYNC_LOST = 0x10;
    public static final int RDS_FLAG_PI_MATCH = 0x20;

    public static final int SEARCH_NORMAL = 0x00;
    public static final int SEARCH_PRESET = 0x01;
    public static final int SEARCH_RSSI = 0x02;

    public static final int SEARCH_RSSI_60DB = 94;
    
    public static int Freq2HwFreq(int freq) {
    	return freq - 64000;
    }

    public static int HwFreq2Freq(int hwFreq) {
    	return hwFreq + 64000;
    }

    public static int Threshold2Rssi(int threshold) {
    	return 110 - threshold / 10;
    }

}
