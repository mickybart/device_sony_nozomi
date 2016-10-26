/*
 * Copyright (C) 2013-2014 The Android Open Source Project
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

#define LOG_TAG "msm8660_platform"
/*#define LOG_NDEBUG 0*/

#include <stdlib.h>
#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <audio_hw.h>
#include <platform_api.h>
#include "platform.h"

//#define SUPPORT_SPEAKER_REVERSE
//#define SUPPORT_BT_SCO_WB

#define LIB_ACDB_LOADER "libacdbloader.so"
#define LIB_MSM_CLIENT "libaudioalsa.so"

#define DUALMIC_CONFIG_NONE 0      /* Target does not contain 2 mics */
#define DUALMIC_CONFIG_ENDFIRE 1
#define DUALMIC_CONFIG_BROADSIDE 2

/*
 * This is the sysfs path for the HDMI audio data block
 */
#define AUDIO_DATA_BLOCK_PATH "/sys/class/graphics/fb1/audio_data_block"
#define MIXER_XML_PATH "/system/etc/mixer_paths.xml"

/*
 * This file will have a maximum of 38 bytes:
 *
 * 4 bytes: number of audio blocks
 * 4 bytes: total length of Short Audio Descriptor (SAD) blocks
 * Maximum 10 * 3 bytes: SAD blocks
 */
#define MAX_SAD_BLOCKS      10
#define SAD_BLOCK_SIZE      3

/* EDID format ID for LPCM audio */
#define EDID_FORMAT_LPCM    1

#define MAX_VOL_INDEX 5
#define MIN_VOL_INDEX 0
#define percent_to_index(val, min, max) \
                (val/20)

struct audio_block_header
{
    int reserved;
    int length;
};


typedef void (*acdb_deallocate_t)();
typedef int  (*acdb_init_t)();
typedef void (*acdb_send_audio_cal_t)(int, int);
typedef void (*acdb_send_voice_cal_t)(int, int);

#define VOICE_SESSION_NAME "Voice session"
//#define LEGACY_QCOM_VOICE

typedef int (*msm_set_voice_rx_vol_t)(int);
typedef void (*msm_set_voice_tx_mute_t)(int);
typedef void (*msm_start_voice_t)(void);
typedef int (*msm_end_voice_t)(void);
typedef int (*msm_mixer_open_t)(const char*, int);
typedef void (*msm_mixer_close_t)(void);
typedef int (*msm_reset_all_device_t)(void);
#ifndef LEGACY_QCOM_VOICE
typedef int (*msm_get_voc_session_t)(const char*);
typedef int (*msm_start_voice_ext_t)(int);
typedef int (*msm_end_voice_ext_t)(int);
typedef int (*msm_set_voice_tx_mute_ext_t)(int, int);
typedef int (*msm_set_voice_rx_vol_ext_t)(int, int);
#endif

/* Audio calibration related functions */
struct platform_data {
    struct audio_device *adev;
    bool fluence_in_spkr_mode;
    bool fluence_in_voice_call;
    bool fluence_in_voice_rec;
    int  dualmic_config;
    bool speaker_lr_swap;

    void *acdb_handle;
    acdb_init_t acdb_init;
    acdb_deallocate_t acdb_deallocate;
    acdb_send_audio_cal_t acdb_send_audio_cal;
    acdb_send_voice_cal_t acdb_send_voice_cal;

    /* msm functions for voice call */
    void *msm_client;
    msm_set_voice_rx_vol_t msm_set_voice_rx_vol;
    msm_set_voice_tx_mute_t msm_set_voice_tx_mute;
    msm_start_voice_t msm_start_voice;
    msm_end_voice_t msm_end_voice;
    msm_mixer_open_t msm_mixer_open;
    msm_mixer_close_t msm_mixer_close;
    msm_reset_all_device_t msm_reset_all_device;
#ifndef LEGACY_QCOM_VOICE
    msm_get_voc_session_t msm_get_voc_session;
    msm_start_voice_ext_t msm_start_voice_ext;
    msm_end_voice_ext_t msm_end_voice_ext;
    msm_set_voice_tx_mute_ext_t msm_set_voice_tx_mute_ext;
    msm_set_voice_rx_vol_ext_t msm_set_voice_rx_vol_ext;
#endif
    
    int voice_session_id;
};

static const int pcm_device_table[AUDIO_USECASE_MAX][2] = {
    [USECASE_AUDIO_PLAYBACK_DEEP_BUFFER] = {0, 0},
    [USECASE_AUDIO_PLAYBACK_LOW_LATENCY] = {1, 1},
    [USECASE_AUDIO_PLAYBACK_MULTI_CH] = {2, 2},
    [USECASE_AUDIO_PLAYBACK_OFFLOAD] = {3, 3},
    [USECASE_AUDIO_RECORD] = {0, 0},
    [USECASE_AUDIO_RECORD_LOW_LATENCY] = {1, 1},
    [USECASE_VOICE_CALL] = {4, 4},
};

/* Array to store sound devices */
static const char * const device_table[SND_DEVICE_MAX] = {
    [SND_DEVICE_NONE] = "none",
    /* Playback sound devices */
    [SND_DEVICE_OUT_HANDSET] = "handset",
    [SND_DEVICE_OUT_SPEAKER] = "speaker",
    [SND_DEVICE_OUT_SPEAKER_REVERSE] = "speaker",
    [SND_DEVICE_OUT_HEADPHONES] = "headset",
    [SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES] = "speaker-and-headset",
    [SND_DEVICE_OUT_VOICE_SPEAKER] = "speaker",
    [SND_DEVICE_OUT_VOICE_HEADPHONES] = "headset",
    [SND_DEVICE_OUT_HDMI] = "hdmi",
    [SND_DEVICE_OUT_SPEAKER_AND_HDMI] = "speaker-and-hdmi",
    [SND_DEVICE_OUT_BT_SCO] = "bt-sco-headset",
    [SND_DEVICE_OUT_BT_SCO_WB] = "bt-sco-headset",
    [SND_DEVICE_OUT_VOICE_TTY_FULL_HEADPHONES] = "voice-tty-headset",
    [SND_DEVICE_OUT_VOICE_TTY_VCO_HEADPHONES] = "voice-tty-headset",
    [SND_DEVICE_OUT_VOICE_TTY_HCO_HANDSET] = "voice-tty-headset",

    /* Capture sound devices */
    [SND_DEVICE_IN_HANDSET_MIC] = "handset-mic",
    [SND_DEVICE_IN_SPEAKER_MIC] = "speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC] = "headset-mic",
    [SND_DEVICE_IN_HANDSET_MIC_AEC] = "handset-mic",
    [SND_DEVICE_IN_SPEAKER_MIC_AEC] = "speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC_AEC] = "headset-mic",
    [SND_DEVICE_IN_VOICE_SPEAKER_MIC] = "speaker-mic",
    [SND_DEVICE_IN_VOICE_HEADSET_MIC] = "headset-mic",
    [SND_DEVICE_IN_HDMI_MIC] = "hdmi-mic",
    [SND_DEVICE_IN_BT_SCO_MIC] = "bt-sco-mic",
    [SND_DEVICE_IN_BT_SCO_MIC_WB] = "bt-sco-mic",
    [SND_DEVICE_IN_CAMCORDER_MIC] = "camcorder-mic",
    [SND_DEVICE_IN_VOICE_DMIC_EF] = "voice-dmic-ef",
    [SND_DEVICE_IN_VOICE_DMIC_BS] = "voice-dmic-bs",
    [SND_DEVICE_IN_VOICE_SPEAKER_DMIC_EF] = "voice-speaker-dmic-ef",
    [SND_DEVICE_IN_VOICE_SPEAKER_DMIC_BS] = "voice-speaker-dmic-bs",
    [SND_DEVICE_IN_VOICE_TTY_FULL_HEADSET_MIC] = "voice-tty-headset-mic",
    [SND_DEVICE_IN_VOICE_TTY_VCO_HANDSET_MIC] = "voice-tty-headset-mic",
    [SND_DEVICE_IN_VOICE_TTY_HCO_HEADSET_MIC] = "voice-tty-headset-mic",
    [SND_DEVICE_IN_VOICE_REC_MIC] = "voice-rec-mic",
    [SND_DEVICE_IN_VOICE_REC_DMIC_EF] = "voice-rec-dmic-ef",
    [SND_DEVICE_IN_VOICE_REC_DMIC_BS] = "voice-rec-dmic-bs",
    [SND_DEVICE_IN_VOICE_REC_DMIC_EF_FLUENCE] = "voice-rec-dmic-ef-fluence",
    [SND_DEVICE_IN_VOICE_REC_DMIC_BS_FLUENCE] = "voice-rec-dmic-bs-fluence",
    [SND_DEVICE_IN_FM_RADIO] = "fm-radio",
};

/* ACDB IDs (audio DSP path configuration IDs) for each sound device */
static const int acdb_device_table[SND_DEVICE_MAX] = {
    [SND_DEVICE_NONE] = -1,
    [SND_DEVICE_OUT_HANDSET] = 7,
    [SND_DEVICE_OUT_SPEAKER] = 263,
    [SND_DEVICE_OUT_SPEAKER_REVERSE] = 263,
    [SND_DEVICE_OUT_HEADPHONES] = 10,
    [SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES] = 272,
    [SND_DEVICE_OUT_VOICE_SPEAKER] = 263,
    [SND_DEVICE_OUT_VOICE_HEADPHONES] = 10,
    [SND_DEVICE_OUT_HDMI] = 18,
    [SND_DEVICE_OUT_BT_SCO] = 22,
    [SND_DEVICE_OUT_BT_SCO_WB] = 22,
    [SND_DEVICE_OUT_VOICE_TTY_FULL_HEADPHONES] = 17,
    [SND_DEVICE_OUT_VOICE_TTY_VCO_HEADPHONES] = 17,
    [SND_DEVICE_OUT_VOICE_TTY_HCO_HANDSET] = 17,

    [SND_DEVICE_IN_HANDSET_MIC] = 4,
    [SND_DEVICE_IN_SPEAKER_MIC] = 11,
    [SND_DEVICE_IN_HEADSET_MIC] = 8,
    [SND_DEVICE_IN_HANDSET_MIC_AEC] = 4,
    [SND_DEVICE_IN_SPEAKER_MIC_AEC] = 11,
    [SND_DEVICE_IN_HEADSET_MIC_AEC] = 8,
    [SND_DEVICE_IN_VOICE_SPEAKER_MIC] = 11,
    [SND_DEVICE_IN_VOICE_HEADSET_MIC] = 8,
    [SND_DEVICE_IN_HDMI_MIC] = 11,
    [SND_DEVICE_IN_BT_SCO_MIC] = 21,
    [SND_DEVICE_IN_BT_SCO_MIC_WB] = 21,
    [SND_DEVICE_IN_CAMCORDER_MIC] = 544,
    [SND_DEVICE_IN_VOICE_DMIC_EF] = 6,
    [SND_DEVICE_IN_VOICE_DMIC_BS] = 5,
    [SND_DEVICE_IN_VOICE_SPEAKER_DMIC_EF] = 13,
    [SND_DEVICE_IN_VOICE_SPEAKER_DMIC_BS] = 12,
    [SND_DEVICE_IN_VOICE_TTY_FULL_HEADSET_MIC] = 16,
    [SND_DEVICE_IN_VOICE_TTY_VCO_HANDSET_MIC] = 16,
    [SND_DEVICE_IN_VOICE_TTY_HCO_HEADSET_MIC] = 16,
    [SND_DEVICE_IN_VOICE_REC_MIC] = 528,
    /* TODO: Update with proper acdb ids */
    [SND_DEVICE_IN_VOICE_REC_DMIC_EF] = 6,
    [SND_DEVICE_IN_VOICE_REC_DMIC_BS] = 5,
    [SND_DEVICE_IN_VOICE_REC_DMIC_EF_FLUENCE] = 6,
    [SND_DEVICE_IN_VOICE_REC_DMIC_BS_FLUENCE] = 5,
    [SND_DEVICE_IN_FM_RADIO] = 255,
};

#define DEEP_BUFFER_PLATFORM_DELAY (29*1000LL)
#define LOW_LATENCY_PLATFORM_DELAY (13*1000LL)


void *platform_init(struct audio_device *adev)
{
    char platform[PROPERTY_VALUE_MAX];
    char baseband[PROPERTY_VALUE_MAX];
    char value[PROPERTY_VALUE_MAX];
    struct platform_data *my_data;

    adev->mixer = mixer_open(MIXER_CARD);

    if (!adev->mixer) {
        ALOGE("Unable to open the mixer, aborting.");
        return NULL;
    }

    adev->audio_route = audio_route_init(MIXER_CARD, MIXER_XML_PATH);
    if (!adev->audio_route) {
        ALOGE("%s: Failed to init audio route controls, aborting.", __func__);
        return NULL;
    }

    my_data = calloc(1, sizeof(struct platform_data));

    my_data->adev = adev;
    my_data->dualmic_config = DUALMIC_CONFIG_NONE;
    my_data->fluence_in_spkr_mode = false;
    my_data->fluence_in_voice_call = false;
    my_data->fluence_in_voice_rec = false;

    property_get("persist.audio.dualmic.config",value,"");
    if (!strcmp("broadside", value)) {
        my_data->dualmic_config = DUALMIC_CONFIG_BROADSIDE;
        adev->acdb_settings |= DMIC_FLAG;
    } else if (!strcmp("endfire", value)) {
        my_data->dualmic_config = DUALMIC_CONFIG_ENDFIRE;
        adev->acdb_settings |= DMIC_FLAG;
    }

    if (my_data->dualmic_config != DUALMIC_CONFIG_NONE) {
        property_get("persist.audio.fluence.voicecall",value,"");
        if (!strcmp("true", value)) {
            my_data->fluence_in_voice_call = true;
        }

        property_get("persist.audio.fluence.voicerec",value,"");
        if (!strcmp("true", value)) {
            my_data->fluence_in_voice_rec = true;
        }

        property_get("persist.audio.fluence.speaker",value,"");
        if (!strcmp("true", value)) {
            my_data->fluence_in_spkr_mode = true;
        }
    }

    my_data->acdb_handle = dlopen(LIB_ACDB_LOADER, RTLD_NOW);
    if (my_data->acdb_handle == NULL) {
        ALOGE("%s: DLOPEN failed for %s", __func__, LIB_ACDB_LOADER);
    } else {
        ALOGV("%s: DLOPEN successful for %s", __func__, LIB_ACDB_LOADER);
        my_data->acdb_deallocate = (acdb_deallocate_t)dlsym(my_data->acdb_handle,
                                                    "acdb_loader_deallocate_ACDB");
        my_data->acdb_send_audio_cal = (acdb_send_audio_cal_t)dlsym(my_data->acdb_handle,
                                                    "acdb_loader_send_audio_cal");
        if (!my_data->acdb_send_audio_cal)
            ALOGW("%s: Could not find the symbol acdb_send_audio_cal from %s",
                  __func__, LIB_ACDB_LOADER);
        my_data->acdb_send_voice_cal = (acdb_send_voice_cal_t)dlsym(my_data->acdb_handle,
                                                    "acdb_loader_send_voice_cal");
        my_data->acdb_init = (acdb_init_t)dlsym(my_data->acdb_handle,
                                                    "acdb_loader_init_ACDB");
        if (my_data->acdb_init == NULL)
            ALOGE("%s: dlsym error %s for acdb_loader_init_ACDB", __func__, dlerror());
        else
            my_data->acdb_init();
    }

    my_data->msm_client = dlopen(LIB_MSM_CLIENT, RTLD_NOW);
    if (my_data->msm_client == NULL) {
      ALOGE("%s: DLOPEN failed for %s", __func__, LIB_MSM_CLIENT);
    } else {
      ALOGV("%s: DLOPEN successful for %s", __func__, LIB_MSM_CLIENT);
      my_data->msm_set_voice_rx_vol = (msm_set_voice_rx_vol_t)dlsym(my_data->msm_client,
					  "msm_set_voice_rx_vol");
      my_data->msm_set_voice_tx_mute = (msm_set_voice_tx_mute_t)dlsym(my_data->msm_client,
					  "msm_set_voice_tx_mute");
      my_data->msm_start_voice = (msm_start_voice_t)dlsym(my_data->msm_client,
					  "msm_start_voice");
      my_data->msm_end_voice = (msm_end_voice_t)dlsym(my_data->msm_client,
					  "msm_end_voice");
      my_data->msm_mixer_open = (msm_mixer_open_t)dlsym(my_data->msm_client,
					  "msm_mixer_open");
      my_data->msm_mixer_close = (msm_mixer_close_t)dlsym(my_data->msm_client,
					  "msm_mixer_close");
      my_data->msm_reset_all_device = (msm_reset_all_device_t)dlsym(my_data->msm_client,
					  "msm_reset_all_device");
#ifndef LEGACY_QCOM_VOICE
      my_data->msm_get_voc_session = (msm_get_voc_session_t)dlsym(my_data->msm_client,
					  "msm_get_voc_session");
      my_data->msm_start_voice_ext = (msm_start_voice_ext_t)dlsym(my_data->msm_client,
					  "msm_start_voice_ext");
      my_data->msm_end_voice_ext = (msm_end_voice_ext_t)dlsym(my_data->msm_client,
					  "msm_end_voice_ext");
      my_data->msm_set_voice_tx_mute_ext = (msm_set_voice_tx_mute_ext_t)dlsym(my_data->msm_client,
					  "msm_set_voice_tx_mute_ext");
      my_data->msm_set_voice_rx_vol_ext = (msm_set_voice_rx_vol_ext_t)dlsym(my_data->msm_client,
					  "msm_set_voice_rx_vol_ext");
#endif
    }
    
    my_data->voice_session_id = 0;
    
    if(my_data->msm_mixer_open == NULL || my_data->msm_mixer_open("/dev/snd/controlC0", 0) < 0)
      ALOGE("ERROR opening the device");
    
    //End any voice call if it exists. This is to ensure the next request
    //to voice call after a mediaserver crash or sub system restart
    //is not ignored by the voice driver.
    if (my_data->msm_end_voice == NULL || my_data->msm_end_voice() < 0)
      ALOGE("msm_end_voice() failed");

    if(my_data->msm_reset_all_device == NULL || my_data->msm_reset_all_device() < 0)
      ALOGE("msm_reset_all_device() failed");

    return my_data;
}

void platform_deinit(void *platform)
{
    struct platform_data *my_data = (struct platform_data *)platform;

    if(my_data->msm_mixer_close != NULL)
	my_data->msm_mixer_close();
 
    if(my_data->acdb_deallocate != NULL)
	my_data->acdb_deallocate();

    //dlclose
    dlclose(LIB_MSM_CLIENT);
    dlclose(LIB_ACDB_LOADER);

    free(platform);
}

const char *platform_get_snd_device_name(snd_device_t snd_device)
{
    if (snd_device >= SND_DEVICE_MIN && snd_device < SND_DEVICE_MAX)
        return device_table[snd_device];
    else
        return "none";
}

void platform_add_backend_name(void *platform __unused, char *mixer_path,
                               snd_device_t snd_device)
{
    if (snd_device == SND_DEVICE_IN_BT_SCO_MIC)
        strcat(mixer_path, " bt-sco");
    else if(snd_device == SND_DEVICE_OUT_BT_SCO)
        strcat(mixer_path, " bt-sco");
    else if (snd_device == SND_DEVICE_OUT_HDMI)
        strcat(mixer_path, " hdmi");
    else if (snd_device == SND_DEVICE_OUT_SPEAKER_AND_HDMI)
        strcat(mixer_path, " speaker-and-hdmi");
#ifdef SUPPORT_BT_SCO_WB
    else if (snd_device == SND_DEVICE_OUT_BT_SCO_WB ||
             snd_device == SND_DEVICE_IN_BT_SCO_MIC_WB)
        strcat(mixer_path, " bt-sco-wb");
#endif
    else if (snd_device == SND_DEVICE_IN_FM_RADIO)
        strcat(mixer_path, " fm-radio");
}

int platform_get_pcm_device_id(audio_usecase_t usecase, int device_type)
{
    int device_id;
    if (device_type == PCM_PLAYBACK)
        device_id = pcm_device_table[usecase][0];
    else
        device_id = pcm_device_table[usecase][1];
    return device_id;
}

int platform_get_snd_device_index(char *snd_device_index_name __unused)
{
    return -ENODEV;
}

int platform_set_snd_device_acdb_id(snd_device_t snd_device __unused,
                                    unsigned int acdb_id __unused)
{
    return -ENODEV;
}

int platform_get_snd_device_acdb_id(snd_device_t snd_device __unused)
{
    ALOGE("%s: Not implemented", __func__);
    return -ENOSYS;
}

void platform_add_operator_specific_device(snd_device_t snd_device __unused,
                                           const char *operator __unused,
                                           const char *mixer_path __unused,
                                           unsigned int acdb_id __unused)
{
}

static int send_audio_calibration(void *platform, snd_device_t snd_device)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    int acdb_dev_id, acdb_dev_type;

    acdb_dev_id = acdb_device_table[snd_device];
    if (acdb_dev_id < 0) {
        ALOGE("%s: Could not find acdb id for device(%d)",
              __func__, snd_device);
        return -EINVAL;
    }
    if (my_data->acdb_send_audio_cal) {
        ALOGV("%s: sending audio calibration for snd_device(%d) acdb_id(%d)",
              __func__, snd_device, acdb_dev_id);
        if (snd_device >= SND_DEVICE_OUT_BEGIN &&
                snd_device < SND_DEVICE_OUT_END)
            acdb_dev_type = ACDB_DEV_TYPE_OUT;
        else
            acdb_dev_type = ACDB_DEV_TYPE_IN;
        my_data->acdb_send_audio_cal(acdb_dev_id, acdb_dev_type);
    }
    return 0;
}

int platform_send_audio_calibration(void *platform, snd_device_t snd_device)
{
    int ret;
    
    if (snd_device != SND_DEVICE_OUT_SPEAKER_AND_HDMI)
        return send_audio_calibration(platform, snd_device);
    
    ret = send_audio_calibration(platform, SND_DEVICE_OUT_SPEAKER);
    if (ret < 0)
        return ret;
    return send_audio_calibration(platform, SND_DEVICE_OUT_HDMI);
}

int platform_switch_voice_call_device_pre(void *platform)
{
    return 0;
}

int platform_switch_voice_call_device_post(void *platform,
                                           snd_device_t out_snd_device,
                                           snd_device_t in_snd_device)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    const char *mixer_ctl_name_rx = "voice-rx";
    const char *mixer_ctl_name_tx = "voice-tx";
    struct mixer_ctl *ctl;
    int rc;

    ctl = mixer_get_ctl_by_name(my_data->adev->mixer, mixer_ctl_name_rx);
    if (!ctl) {
        ALOGE("%s: Could not get ctl for mixer cmd - %s",
              __func__, mixer_ctl_name_rx);
        return -EINVAL;
    }
    rc = mixer_ctl_set_enum_by_string(ctl, device_table[out_snd_device]);
    if (rc < 0)
        return rc;

    ctl = mixer_get_ctl_by_name(my_data->adev->mixer, mixer_ctl_name_tx);
    if (!ctl) {
        ALOGE("%s: Could not get ctl for mixer cmd - %s",
              __func__, mixer_ctl_name_tx);
        return -EINVAL;
    }
    rc = mixer_ctl_set_enum_by_string(ctl, device_table[in_snd_device]);
    if (rc < 0)
        return rc;

    if (my_data->acdb_send_voice_cal) {
        int rx_id = acdb_device_table[out_snd_device];
        int tx_id = acdb_device_table[in_snd_device];
        if ((rx_id < 0) || (tx_id < 0)) {
            ALOGE("%s: Could not find acdb id for voice device(rx=%d tx=%d)",
                __func__, out_snd_device, in_snd_device);
            return -EINVAL;
        }

        ALOGV("%s: sending voice calibration for out_snd_device(%d) acdb_id(%d) and in_snd_device(%d) acdb_id(%d)",
              __func__, out_snd_device, rx_id, in_snd_device, tx_id);
        my_data->acdb_send_voice_cal(rx_id, tx_id);
    }

    return 0;
}

int platform_start_voice_call(void *platform, uint32_t vsid __unused)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    int ret = 0;

    if (my_data->msm_client) {
#ifdef LEGACY_QCOM_VOICE
        if (my_data->msm_start_voice == NULL) {
            ALOGE("dlsym error for msm_client_start_voice");
            ret = -ENOSYS;
        } else {
	  my_data->msm_start_voice();
        }
#else
	if (my_data->msm_start_voice_ext == NULL || 
	  my_data->msm_get_voc_session == NULL ||
	  my_data->msm_set_voice_tx_mute_ext == NULL) {
            ALOGE("dlsym error for msm_client_*");
            ret = -ENOSYS;
        } else {
	  my_data->voice_session_id = my_data->msm_get_voc_session(VOICE_SESSION_NAME);
	  if(my_data->voice_session_id <=0) {
	      ALOGE("voice session invalid");
	      return 0;
	  }
	  ret = my_data->msm_start_voice_ext(my_data->voice_session_id);
	  if (ret < 0) {
	    ALOGE("%s: msm_start_voice_ext error %d", __func__, ret);
	  }
        }
#endif
    }

    return ret;
}

int platform_stop_voice_call(void *platform, uint32_t vsid __unused)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    int ret = 0;

    if (my_data->msm_client) {
#ifdef LEGACY_QCOM_VOICE
        if (my_data->msm_end_voice == NULL) {
            ALOGE("dlsym error for msm_end_voice");
        } else {
            ret = my_data->msm_end_voice();
            if (ret < 0) {
                ALOGE("%s: msm_end_voice error %d\n", __func__, ret);
            }
        }
#else
	if (my_data->msm_end_voice_ext == NULL) {
            ALOGE("dlsym error for msm_end_voice_ext");
        } else {
            ret = my_data->msm_end_voice_ext(my_data->voice_session_id);
            if (ret < 0) {
                ALOGE("%s: msm_end_voice_ext error %d\n", __func__, ret);
            }
	    my_data->voice_session_id = 0;
        }
#endif
    }

    return ret;
}

void platform_set_speaker_gain_in_combo(struct audio_device *adev __unused,
                                        snd_device_t snd_device  __unused,
                                        bool enable __unused) {
}

int platform_set_voice_volume(void *platform, int volume)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    int ret = 0;
 
    // Voice volume levels are mapped to adsp volume levels as follows.
    // 100 -> 5, 80 -> 4, 60 -> 3, 40 -> 2, 20 -> 1  0 -> 0
    // But this values don't changed in kernel. So, below change is need.
    volume = (int)percent_to_index(volume, MIN_VOL_INDEX, MAX_VOL_INDEX);

    if (my_data->msm_client) {
#ifdef LEGACY_QCOM_VOICE
        if (my_data->msm_set_voice_rx_vol == NULL) {
            ALOGE("%s: dlsym error for msm_set_voice_rx_vol", __func__);
        } else {
            ret = my_data->msm_set_voice_rx_vol(volume);
            if (ret < 0) {
                ALOGE("%s: msm_set_voice_rx_vol error %d", __func__, ret);
            }
        }
#else
	if (my_data->msm_set_voice_rx_vol_ext == NULL) {
            ALOGE("%s: dlsym error for msm_set_voice_rx_vol_ext", __func__);
        } else {
            ret = my_data->msm_set_voice_rx_vol_ext(volume,my_data->voice_session_id);
            if (ret < 0) {
                ALOGE("%s: msm_set_voice_rx_vol error_ext %d", __func__, ret);
            }
        }
#endif
    } else {
        ALOGE("%s: No MSM Client present", __func__);
    }
    return 0;
}

int platform_set_mic_mute(void *platform, bool state)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    int ret = 0;

    if (my_data->adev->mode == AUDIO_MODE_IN_CALL) {
        if (my_data->msm_client) {
#ifdef LEGACY_QCOM_VOICE
            if (my_data->msm_set_voice_tx_mute == NULL) {
                ALOGE("%s: dlsym error for msm_set_voice_tx_mute", __func__);
            } else {
                my_data->msm_set_voice_tx_mute(state);
            }
#else
	    if (my_data->msm_set_voice_tx_mute_ext == NULL) {
                ALOGE("%s: dlsym error for msm_set_voice_tx_mute_ext", __func__);
            } else {
                ret = my_data->msm_set_voice_tx_mute_ext(state,my_data->voice_session_id);
                if (ret < 0) {
		  ALOGE("%s: msm_set_voice_tx_mute error %d", __func__, ret);
                }
            }
#endif
        } else {
            ALOGE("%s: No MSM Client present", __func__);
        }
    }

    return ret;
}

int platform_set_device_mute(void *platform __unused, bool state __unused, char *dir __unused)
{
    ALOGE("%s: Not implemented", __func__);
    return -ENOSYS;
}

snd_device_t platform_get_output_snd_device(void *platform, audio_devices_t devices)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    struct audio_device *adev = my_data->adev;
    audio_mode_t mode = adev->mode;
    snd_device_t snd_device = SND_DEVICE_NONE;

    ALOGV("%s: enter: output devices(%#x)", __func__, devices);
    if (devices == AUDIO_DEVICE_NONE ||
        devices & AUDIO_DEVICE_BIT_IN) {
        ALOGV("%s: Invalid output devices (%#x)", __func__, devices);
        goto exit;
    }

    if (voice_is_in_call(adev)) {
        if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
            devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            if (adev->voice.tty_mode == TTY_MODE_FULL)
                snd_device = SND_DEVICE_OUT_VOICE_TTY_FULL_HEADPHONES;
            else if (adev->voice.tty_mode == TTY_MODE_VCO)
                snd_device = SND_DEVICE_OUT_VOICE_TTY_VCO_HEADPHONES;
            else if (adev->voice.tty_mode == TTY_MODE_HCO)
                snd_device = SND_DEVICE_OUT_VOICE_TTY_HCO_HANDSET;
            else
                snd_device = SND_DEVICE_OUT_VOICE_HEADPHONES;
        } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
#ifdef SUPPORT_BT_SCO_WB
            if (adev->bt_wb_speech_enabled) {
                snd_device = SND_DEVICE_OUT_BT_SCO_WB;
            } else {
                snd_device = SND_DEVICE_OUT_BT_SCO;
            }
#else
            snd_device = SND_DEVICE_OUT_BT_SCO;
#endif
        } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_OUT_VOICE_SPEAKER;
        } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_OUT_HANDSET;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) == 2) {
        if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                        AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                               AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else if (devices == (AUDIO_DEVICE_OUT_AUX_DIGITAL |
                               AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HDMI;
        } else {
            ALOGE("%s: Invalid combo device(%#x)", __func__, devices);
            goto exit;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) != 1) {
        ALOGE("%s: Invalid output devices(%#x)", __func__, devices);
        goto exit;
    }

    if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
        devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
        snd_device = SND_DEVICE_OUT_HEADPHONES;
    } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
        if (my_data->speaker_lr_swap)
            snd_device = SND_DEVICE_OUT_SPEAKER_REVERSE;
        else
            snd_device = SND_DEVICE_OUT_SPEAKER;
    } else if (devices & AUDIO_DEVICE_OUT_ALL_SCO) {
#ifdef SUPPORT_BT_SCO_WB
        if (adev->bt_wb_speech_enabled) {
            snd_device = SND_DEVICE_OUT_BT_SCO_WB;
        } else {
            snd_device = SND_DEVICE_OUT_BT_SCO;
        }
#else
        snd_device = SND_DEVICE_OUT_BT_SCO;
#endif
    } else if (devices & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
        snd_device = SND_DEVICE_OUT_HDMI ;
    } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
        snd_device = SND_DEVICE_OUT_HANDSET;
    } else {
        ALOGE("%s: Unknown device(s) %#x", __func__, devices);
    }
exit:
    ALOGV("%s: exit: snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

snd_device_t platform_get_input_snd_device(void *platform, audio_devices_t out_device)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    struct audio_device *adev = my_data->adev;
    audio_source_t  source = (adev->active_input == NULL) ?
                                AUDIO_SOURCE_DEFAULT : adev->active_input->source;

    audio_mode_t    mode   = adev->mode;
    audio_devices_t in_device = ((adev->active_input == NULL) ?
                                    AUDIO_DEVICE_NONE : adev->active_input->device)
                                & ~AUDIO_DEVICE_BIT_IN;
    audio_channel_mask_t channel_mask = (adev->active_input == NULL) ?
                                AUDIO_CHANNEL_IN_MONO : adev->active_input->channel_mask;
    snd_device_t snd_device = SND_DEVICE_NONE;

    ALOGV("%s: enter: out_device(%#x) in_device(%#x)",
          __func__, out_device, in_device);
    if ((out_device != AUDIO_DEVICE_NONE) && voice_is_in_call(adev)) {
        if (adev->voice.tty_mode != TTY_MODE_OFF) {
            if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                switch (adev->voice.tty_mode) {
                case TTY_MODE_FULL:
                    snd_device = SND_DEVICE_IN_VOICE_TTY_FULL_HEADSET_MIC;
                    break;
                case TTY_MODE_VCO:
                    snd_device = SND_DEVICE_IN_VOICE_TTY_VCO_HANDSET_MIC;
                    break;
                case TTY_MODE_HCO:
                    snd_device = SND_DEVICE_IN_VOICE_TTY_HCO_HEADSET_MIC;
                    break;
                default:
                    ALOGE("%s: Invalid TTY mode (%#x)", __func__, adev->voice.tty_mode);
                }
                goto exit;
            }
        }
        if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
            out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            if (my_data->fluence_in_voice_call == false) {
                snd_device = SND_DEVICE_IN_HANDSET_MIC;
            } else {
                if (my_data->dualmic_config == DUALMIC_CONFIG_ENDFIRE)
                    snd_device = SND_DEVICE_IN_VOICE_DMIC_EF;
                else if(my_data->dualmic_config == DUALMIC_CONFIG_BROADSIDE)
                    snd_device = SND_DEVICE_IN_VOICE_DMIC_BS;
                else
                    snd_device = SND_DEVICE_IN_HANDSET_MIC;
            }
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
#ifdef SUPPORT_BT_SCO_WB
            if (adev->bt_wb_speech_enabled) {
                snd_device = SND_DEVICE_IN_BT_SCO_MIC_WB;
            } else {
                snd_device = SND_DEVICE_IN_BT_SCO_MIC;
            }
#else
            snd_device = SND_DEVICE_IN_BT_SCO_MIC ;
#endif
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            if (my_data->fluence_in_voice_call && my_data->fluence_in_spkr_mode &&
                    my_data->dualmic_config == DUALMIC_CONFIG_ENDFIRE) {
                snd_device = SND_DEVICE_IN_VOICE_SPEAKER_DMIC_EF;
            } else if (my_data->fluence_in_voice_call && my_data->fluence_in_spkr_mode &&
                    my_data->dualmic_config == DUALMIC_CONFIG_BROADSIDE) {
                snd_device = SND_DEVICE_IN_VOICE_SPEAKER_DMIC_BS;
            } else {
                snd_device = SND_DEVICE_IN_VOICE_SPEAKER_MIC;
            }
        }
    } else if (source == AUDIO_SOURCE_CAMCORDER) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC ||
            in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_CAMCORDER_MIC;
        }
    } else if (source == AUDIO_SOURCE_VOICE_RECOGNITION) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            if (my_data->dualmic_config == DUALMIC_CONFIG_ENDFIRE) {
                if (channel_mask == AUDIO_CHANNEL_IN_FRONT_BACK)
                    snd_device = SND_DEVICE_IN_VOICE_REC_DMIC_EF;
                else if (my_data->fluence_in_voice_rec)
                    snd_device = SND_DEVICE_IN_VOICE_REC_DMIC_EF_FLUENCE;
            } else if (my_data->dualmic_config == DUALMIC_CONFIG_BROADSIDE) {
                if (channel_mask == AUDIO_CHANNEL_IN_FRONT_BACK)
                    snd_device = SND_DEVICE_IN_VOICE_REC_DMIC_BS;
                else if (my_data->fluence_in_voice_rec)
                    snd_device = SND_DEVICE_IN_VOICE_REC_DMIC_BS_FLUENCE;
            }

            if (snd_device == SND_DEVICE_NONE) {
                snd_device = SND_DEVICE_IN_VOICE_REC_MIC;
            }
        }
    } else if (source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
        if (out_device & AUDIO_DEVICE_OUT_SPEAKER)
            in_device = AUDIO_DEVICE_IN_BACK_MIC;
        if (adev->active_input) {
            if (adev->active_input->enable_aec) {
                if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
                    snd_device = SND_DEVICE_IN_SPEAKER_MIC_AEC;
                } else if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
                    snd_device = SND_DEVICE_IN_HANDSET_MIC_AEC;
                } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
                    snd_device = SND_DEVICE_IN_HEADSET_MIC_AEC;
                }
            }
        }
    } else if (source == AUDIO_SOURCE_DEFAULT) {
        goto exit;
    }


    if (snd_device != SND_DEVICE_NONE) {
        goto exit;
    }

    if (in_device != AUDIO_DEVICE_NONE &&
            !(in_device & AUDIO_DEVICE_IN_VOICE_CALL) &&
            !(in_device & AUDIO_DEVICE_IN_COMMUNICATION)) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
#ifdef SUPPORT_BT_SCO_WB
            if (adev->bt_wb_speech_enabled) {
                snd_device = SND_DEVICE_IN_BT_SCO_MIC_WB;
            } else {
                snd_device = SND_DEVICE_IN_BT_SCO_MIC;
            }
#else
            snd_device = SND_DEVICE_IN_BT_SCO_MIC;
#endif
        } else if (in_device & AUDIO_DEVICE_IN_AUX_DIGITAL) {
            snd_device = SND_DEVICE_IN_HDMI_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_FM_TUNER) {
            snd_device = SND_DEVICE_IN_FM_RADIO;
        } else {
            ALOGE("%s: Unknown input device(s) %#x", __func__, in_device);
            ALOGW("%s: Using default handset-mic", __func__);
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        }
    } else {
        if (out_device & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
#ifdef SUPPORT_BT_SCO_WB
            if (adev->bt_wb_speech_enabled) {
                snd_device = SND_DEVICE_IN_BT_SCO_MIC_WB;
            } else {
                snd_device = SND_DEVICE_IN_BT_SCO_MIC;
            }
#else
            snd_device = SND_DEVICE_IN_BT_SCO_MIC;
#endif
        } else if (out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL) {
            snd_device = SND_DEVICE_IN_HDMI_MIC;
        } else {
            ALOGE("%s: Unknown output device(s) %#x", __func__, out_device);
            ALOGW("%s: Using default handset-mic", __func__);
            snd_device = SND_DEVICE_IN_HANDSET_MIC;
        }
    }
exit:
    ALOGV("%s: exit: in_snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

int platform_set_hdmi_channels(void *platform,  int channel_count)
{
    struct platform_data *my_data = (struct platform_data *)platform;
    struct audio_device *adev = my_data->adev;
    struct mixer_ctl *ctl;
    const char *channel_cnt_str = NULL;
    const char *mixer_ctl_name = "HDMI_RX Channels";
    switch (channel_count) {
    case 8:
        channel_cnt_str = "Eight"; break;
    case 7:
        channel_cnt_str = "Seven"; break;
    case 6:
        channel_cnt_str = "Six"; break;
    case 5:
        channel_cnt_str = "Five"; break;
    case 4:
        channel_cnt_str = "Four"; break;
    case 3:
        channel_cnt_str = "Three"; break;
    default:
        channel_cnt_str = "Two"; break;
    }
    ctl = mixer_get_ctl_by_name(adev->mixer, mixer_ctl_name);
    if (!ctl) {
        ALOGE("%s: Could not get ctl for mixer cmd - %s",
              __func__, mixer_ctl_name);
        return -EINVAL;
    }
    ALOGV("HDMI channel count: %s", channel_cnt_str);
    mixer_ctl_set_enum_by_string(ctl, channel_cnt_str);
    return 0;
}

int platform_edid_get_max_channels(void *platform __unused)
{
    FILE *file;
    struct audio_block_header header;
    char block[MAX_SAD_BLOCKS * SAD_BLOCK_SIZE];
    char *sad = block;
    int num_audio_blocks;
    int channel_count;
    int max_channels = 0;
    int i;

    file = fopen(AUDIO_DATA_BLOCK_PATH, "rb");
    if (file == NULL) {
        ALOGE("Unable to open '%s'", AUDIO_DATA_BLOCK_PATH);
        return 0;
    }

    /* Read audio block header */
    fread(&header, 1, sizeof(header), file);

    /* Read SAD blocks, clamping the maximum size for safety */
    if (header.length > (int)sizeof(block))
        header.length = (int)sizeof(block);
    fread(&block, header.length, 1, file);

    fclose(file);

    /* Calculate the number of SAD blocks */
    num_audio_blocks = header.length / SAD_BLOCK_SIZE;

    for (i = 0; i < num_audio_blocks; i++) {
        /* Only consider LPCM blocks */
        if ((sad[0] >> 3) != EDID_FORMAT_LPCM)
            continue;

        channel_count = (sad[0] & 0x7) + 1;
        if (channel_count > max_channels)
            max_channels = channel_count;

        /* Advance to next block */
        sad += 3;
    }

    return max_channels;
}

int platform_set_incall_recording_session_id(void *platform __unused,
                                             uint32_t session_id __unused, int rec_mode __unused)
{
    ALOGE("%s: Not implemented", __func__);
    return -ENOSYS;
}

int platform_stop_incall_recording_usecase(void *platform __unused)
{
    ALOGE("%s: Not implemented", __func__);
    return -ENOSYS;
}

int platform_start_incall_music_usecase(void *platform __unused)
{
    ALOGE("%s: Not implemented", __func__);
    return -ENOSYS;
}

int platform_stop_incall_music_usecase(void *platform __unused)
{
    ALOGE("%s: Not implemented", __func__);
    return -ENOSYS;
}

int platform_set_parameters(void *platform __unused,
                            struct str_parms *parms __unused)
{
    ALOGE("%s: Not implemented", __func__);
    return -ENOSYS;
}

/* Delay in Us */
int64_t platform_render_latency(audio_usecase_t usecase)
{
    switch (usecase) {
        case USECASE_AUDIO_PLAYBACK_DEEP_BUFFER:
            return DEEP_BUFFER_PLATFORM_DELAY;
        case USECASE_AUDIO_PLAYBACK_LOW_LATENCY:
            return LOW_LATENCY_PLATFORM_DELAY;
        default:
            return 0;
    }
}

int platform_switch_voice_call_enable_device_config(void *platform __unused,
                                                    snd_device_t out_snd_device __unused,
                                                    snd_device_t in_snd_device __unused)
{
    return 0;
}

int platform_switch_voice_call_usecase_route_post(void *platform __unused,
                                                  snd_device_t out_snd_device __unused,
                                                  snd_device_t in_snd_device __unused)
{
    return 0;
}

int platform_get_sample_rate(void *platform __unused, uint32_t *rate __unused)
{
    return -ENOSYS;
}

int platform_get_usecase_index(const char * usecase __unused)
{
    return -ENOSYS;
}

int platform_set_usecase_pcm_id(audio_usecase_t usecase __unused, int32_t type __unused,
                                int32_t pcm_id __unused)
{
    return -ENOSYS;
}

int platform_set_snd_device_backend(snd_device_t device __unused,
                                    const char *backend __unused,
                                    const char *hw_interface __unused)
{
    return -ENOSYS;
}

void platform_set_echo_reference(struct audio_device *adev __unused,
                                 bool enable __unused,
                                 audio_devices_t out_device __unused)
{
    return;
}

int platform_swap_lr_channels(struct audio_device *adev, bool swap_channels)
{
#ifdef SUPPORT_SPEAKER_REVERSE    // only update the selected device if there is active pcm playback
    struct audio_usecase *usecase;
    struct listnode *node;
    struct platform_data *my_data = (struct platform_data *)adev->platform;
    int status = 0;

    if (my_data->speaker_lr_swap != swap_channels) {
        my_data->speaker_lr_swap = swap_channels;

        list_for_each(node, &adev->usecase_list) {
            usecase = node_to_item(node, struct audio_usecase, list);
            if (usecase->type == PCM_PLAYBACK &&
                usecase->stream.out->devices & AUDIO_DEVICE_OUT_SPEAKER) {
                const char *mixer_path;
                if (swap_channels) {
                    mixer_path = platform_get_snd_device_name(SND_DEVICE_OUT_SPEAKER_REVERSE);
                    audio_route_apply_and_update_path(adev->audio_route, mixer_path);
                } else {
                    mixer_path = platform_get_snd_device_name(SND_DEVICE_OUT_SPEAKER);
                    audio_route_apply_and_update_path(adev->audio_route, mixer_path);
                }
                break;
            }
        }
    }
    return status;
#else
    return 0;#endif
}

bool platform_send_gain_dep_cal(void *platform __unused,
                                int level __unused)
{
    return 0;
}

bool platform_can_split_snd_device(snd_device_t in_snd_device __unused,
                                   int *num_devices __unused,
                                   snd_device_t *out_snd_devices __unused)
{
    return false;
}

bool platform_check_backends_match(snd_device_t snd_device1 __unused,
                                   snd_device_t snd_device2 __unused)
{
    return true;
}
