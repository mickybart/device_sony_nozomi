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

#define LOG_TAG "FmReceiverServiceJni"
//#define LOG_NDEBUG 0

#include "com_android_bluetooth.h"
#include "hardware/bt_av.h"
#include "utils/Log.h"
#include "android_runtime/AndroidRuntime.h"

#include <string.h>

namespace android {
static jmethodID method_onStateChanged;
static jmethodID method_onCommandCallback;

static const fm_interface_t *sFmInterface = NULL;
static jobject mCallbacksObj = NULL;
static JNIEnv *sCallbackEnv = NULL;

static bool checkCallbackThread() {
    // Always fetch the latest callbackEnv from AdapterService.
    // Caching this could cause this sCallbackEnv to go out-of-sync
    // with the AdapterService's ENV if an ASSOCIATE/DISASSOCIATE event
    // is received
    sCallbackEnv = getCallbackEnv();

    JNIEnv* env = AndroidRuntime::getJNIEnv();
    if (sCallbackEnv != env || sCallbackEnv == NULL) return false;
    return true;
}

static void fm_state_callback(fm_state_t state) {
    jbyteArray addr;

    if (!checkCallbackThread()) {                                       \
        ALOGE("Callback: '%s' is not called on the correct thread", __FUNCTION__); \
        return;                                                         \
    }
    
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onStateChanged, (jint) state);
    checkAndClearExceptionFromCallback(sCallbackEnv, __FUNCTION__);
}

static void fm_command_callback(uint16_t opcode, uint8_t *buf, uint8_t len) {
    jbyteArray param;

    if (!checkCallbackThread()) {                                       \
        ALOGE("Callback: '%s' is not called on the correct thread", __FUNCTION__); \
        return;                                                         \
    }
    param = sCallbackEnv->NewByteArray(len);
    if (!param) {
        ALOGE("Fail to new jbyteArray param for fm command callback");
        checkAndClearExceptionFromCallback(sCallbackEnv, __FUNCTION__);
        return;
    }

    sCallbackEnv->SetByteArrayRegion(param, 0, len, (jbyte*)buf);
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onCommandCallback, (jint)opcode,
                                 param);
    checkAndClearExceptionFromCallback(sCallbackEnv, __FUNCTION__);
    sCallbackEnv->DeleteLocalRef(param);
}

static fm_callbacks_t sFmCallbacks = {
    sizeof(sFmCallbacks),
    fm_state_callback,
    fm_command_callback
};

static void classInitNative(JNIEnv* env, jclass clazz) {
    method_onStateChanged =
        env->GetMethodID(clazz, "onStateChanged", "(I)V");

    method_onCommandCallback =
        env->GetMethodID(clazz, "onCommandCallback", "(I[B)V");
}

static void initNative(JNIEnv *env, jobject object) {
    if (mCallbacksObj != NULL) {
         ALOGW("Cleaning up FM Radio callback object");
         env->DeleteGlobalRef(mCallbacksObj);
         mCallbacksObj = NULL;
    }

    mCallbacksObj = env->NewGlobalRef(object);
}

static void cleanupNative(JNIEnv *env, jobject object) {
    if (sFmInterface != NULL) {
        sFmInterface->disable();
        sFmInterface = NULL;
    }

    if (mCallbacksObj != NULL) {
        env->DeleteGlobalRef(mCallbacksObj);
        mCallbacksObj = NULL;
    }
}

static jboolean enableNative(JNIEnv *env, jobject object) {
	int status;

    if (!sFmInterface) {
        if ((sFmInterface = getFmInterface()) == NULL) {
            ALOGE("FM Radio module is not loaded");
            return JNI_FALSE;
        }

        if ((status = sFmInterface->set_callback(&sFmCallbacks)) != FM_STATUS_SUCCESS) {
            ALOGE("Failed to initialize FM Radio module, status: %d", status);
            sFmInterface = NULL;
            return JNI_FALSE;
        }
    }

    if ((status = sFmInterface->enable()) != FM_STATUS_SUCCESS) {
        ALOGE("Failed FM Radio enable, status: %d", status);
    }
    return (status == FM_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean disableNative(JNIEnv *env, jobject object) {
	int status;

    if (!sFmInterface) return JNI_FALSE;

    if ((status = sFmInterface->disable()) != FM_STATUS_SUCCESS) {
        ALOGE("Failed FM Radio enable, status: %d", status);
    }
    return (status == FM_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean sendCmdNative(JNIEnv *env, jobject object, jint opcode, jbyteArray param) {
    jbyte *p;
    jsize len;
    int status;

    if (!sFmInterface) return JNI_FALSE;

    p = env->GetByteArrayElements(param, NULL);
    if (!p) {
        jniThrowIOException(env, EINVAL);
        return JNI_FALSE;
    }
    
    len = env->GetArrayLength(param);   

    if ((status = sFmInterface->vendor_command((uint16_t)opcode, (uint8_t *)p, len)) != BT_STATUS_SUCCESS) {
        ALOGE("Failed HF disconnection, status: %d", status);
    }
    env->ReleaseByteArrayElements(param, p, 0);
    return (status == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void *) classInitNative},
    {"initNative", "()V", (void *) initNative},
    {"cleanupNative", "()V", (void *) cleanupNative},
    {"enableNative", "()Z", (void *) enableNative},
    {"disableNative", "()Z", (void *) disableNative},
    {"sendCmdNative", "(I[B)Z", (void *) sendCmdNative},
};

int register_com_broadcom_fm(JNIEnv* env) {
    return jniRegisterNativeMethods(env, "com/broadcom/fm/FmReceiverHandler", 
                                    sMethods, NELEM(sMethods));
}

}
