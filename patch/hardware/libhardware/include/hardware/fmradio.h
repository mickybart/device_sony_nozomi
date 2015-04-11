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

#ifndef ANDROID_FMRADIO_INTERFACE_H
#define ANDROID_FMRADIO_INTERFACE_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <hardware/hardware.h>

__BEGIN_DECLS

/**
 * The id of this module
 */
#define FMRADIO_HARDWARE_MODULE_ID "fmradio"


/** FM Radio Device State */
typedef enum {
    FM_STATE_OFF,
    FM_STATE_ON
} fm_state_t;


/** FM Radio Error Status */
typedef enum {
    FM_STATUS_SUCCESS,
    FM_STATUS_FAIL,
    FM_STATUS_NOT_READY,
    FM_STATUS_NOMEM,
    FM_STATUS_BUSY,
    FM_STATUS_DONE,        /* request already completed */
    FM_STATUS_UNSUPPORTED,
    FM_STATUS_PARM_INVALID,
    FM_STATUS_UNHANDLED
} fm_status_t;


/** FM Radio Interface callbacks */

/** FM Radio Enable/Disable Callback. */
typedef void (*state_changed_callback)(fm_state_t state);

/** FM Radio Vendor Command Callback */
typedef void (*vendor_command_callback)(uint16_t opcode, uint8_t *buf, uint8_t len);

/** FM Radio callback structure. */
typedef struct {
    /** set to sizeof(fm_callbacks_t) */
    size_t size;
    state_changed_callback state_changed_cb;
    vendor_command_callback vendor_command_cb;
} fm_callbacks_t;


/** Represents the standard FM Radio interface. */
typedef struct {
    /** set to sizeof(fm_interface_t) */
    size_t size;

    /** Provides the callback routines */
    int (*set_callback)(fm_callbacks_t* callbacks);

    /** Enable FM Radio. */
    int (*enable)(void);

    /** Disable FM Radio. */
    int (*disable)(void);

    /* Send vendor-specific command to the controller. */
    int (*vendor_command)(uint16_t opcode, uint8_t *buf, uint8_t len);
} fm_interface_t;


typedef struct {
    struct hw_device_t common;
    const fm_interface_t* (*get_fmradio_interface)();
} fmradio_device_t;

typedef fmradio_device_t fmradio_module_t;

__END_DECLS

#endif  // ANDROID_FMRADIO_INTERFACE_H

