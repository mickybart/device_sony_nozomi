/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2011 Diogo Ferreira <defer@cyanogenmod.com>
 * Copyright (C) 2012 Andreas Makris <andreas.makris@gmail.com>
 * Copyright (C) 2012 The CyanogenMod Project <http://www.cyanogenmod.com>
 * Copyright (C) 2015 nAOSP ROM
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

#define LOG_TAG "lights.lt26"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>
#include "lights.h"

/* Synchronization primities */
static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* Mini-led state machine */
static struct light_state_t g_notification;
static struct light_state_t g_battery;

/* Buttons-backlight state machine */
#define DIM_BRIGHTNESS 5
#define DIM_BRIGHTNESS_MAX_DROP 4
#define MAX_BUTTONS_BRIGHTNESS 15
static int g_buttons_notification_support = 1;
static int g_buttons_force_off_after = MAX_BUTTONS_BRIGHTNESS;
static struct light_state_t g_buttons;
static struct light_state_t g_buttons_before_dim;
static struct light_state_t g_buttons_notification;

/* low power */
static int g_low_power = 1;

static int read_int (const char *path) {
	int fd;
	char buffer[20];
	
	fd = open(path, O_RDONLY);
	if (fd < 0) {
	  return -1;
	}
	
	int cnt = read(fd, buffer, 20);
	close(fd);
	
	if(cnt > 0) {
	  return atoi(buffer);
	}
	return -1;
}


static int write_int (const char *path, int value) {
	int fd;
	static int already_warned = 0;

	fd = open(path, O_RDWR);
	if (fd < 0) {
		if (already_warned == 0) {
			ALOGE("write_int failed to open %s\n", path);
			already_warned = 1;
		}
		return -errno;
	}

	char buffer[20];
	int bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
	int written = write (fd, buffer, bytes);
	close(fd);

	return written == -1 ? -errno : 0;
}

static int write_string (const char *path, const char *value) {
	int fd;
	static int already_warned = 0;

	fd = open(path, O_RDWR);
	if (fd < 0) {
		if (already_warned == 0) {
			ALOGE("write_string failed to open %s\n", path);
			already_warned = 1;
		}
		return -errno;
	}

	char buffer[20];
	int bytes = snprintf(buffer, sizeof(buffer), "%s\n", value);
	int written = write (fd, buffer, bytes);
	close(fd);

	return written == -1 ? -errno : 0;
}


/* Color tools */
static int is_lit (struct light_state_t const* state) {
	return state->color & 0x00ffffff;
}

static int rgb_to_brightness (struct light_state_t const* state) {
	int color = state->color & 0x00ffffff;
	return ((77*((color>>16)&0x00ff))
			+ (150*((color>>8)&0x00ff)) + (29*(color&0x00ff))) >> 8;
}

/* The actual lights controlling section */
static int set_light_backlight (struct light_device_t *dev, struct light_state_t const *state) {
	int brightness = rgb_to_brightness(state);
	pthread_mutex_lock(&g_lock);
	write_int (LCD_BACKLIGHT_FILE, brightness);
	pthread_mutex_unlock(&g_lock);
	return 0;
}

static void set_shared_buttons_locked (struct light_device_t *dev, struct light_state_t *state) {
	int brightness;
	int delayOn,delayOff;
	
	brightness = rgb_to_brightness(state);
	delayOn = state->flashOnMS;
	delayOff = state->flashOffMS;
	
	if (state->flashMode != LIGHT_FLASH_NONE) {
		write_string (BUTTON_BACKLIGHT_FILE_TRIGGER, "timer");
		write_int (BUTTON_BACKLIGHT_FILE_DELAYON, delayOn);
		write_int (BUTTON_BACKLIGHT_FILE_DELAYOFF, delayOff);
	} else {
		write_string (BUTTON_BACKLIGHT_FILE_TRIGGER, "none");
	}

	if (g_low_power && brightness > g_buttons_force_off_after)
		write_int (BUTTON_BACKLIGHT_FILE, 0);
	else
		write_int (BUTTON_BACKLIGHT_FILE, brightness);
}

static void set_shared_light_locked (struct light_device_t *dev, struct light_state_t *state) {
	int r, g, b;
	int delayOn,delayOff;

	r = (state->color >> 16) & 0xFF;
	g = (state->color >> 8) & 0xFF;
	b = (state->color) & 0xFF;

	delayOn = state->flashOnMS;
	delayOff = state->flashOffMS;

	if (state->flashMode != LIGHT_FLASH_NONE) {
		write_string (RED_LED_FILE_TRIGGER, "timer");
		write_string (GREEN_LED_FILE_TRIGGER, "timer");
		write_string (BLUE_LED_FILE_TRIGGER, "timer");

		write_int (RED_LED_FILE_DELAYON, delayOn);
		write_int (GREEN_LED_FILE_DELAYON, delayOn);
		write_int (BLUE_LED_FILE_DELAYON, delayOn);

		write_int (RED_LED_FILE_DELAYOFF, delayOff);
		write_int (GREEN_LED_FILE_DELAYOFF, delayOff);
		write_int (BLUE_LED_FILE_DELAYOFF, delayOff);
	} else {
		write_string (RED_LED_FILE_TRIGGER, "none");
		write_string (GREEN_LED_FILE_TRIGGER, "none");
		write_string (BLUE_LED_FILE_TRIGGER, "none");
	}

	write_int (RED_LED_FILE, r);
	write_int (GREEN_LED_FILE, g);
	write_int (BLUE_LED_FILE, b);

}

static void handle_shared_battery_locked (struct light_device_t *dev) {
	if (is_lit (&g_notification)) {
		set_shared_light_locked (dev, &g_notification);
	} else {
		set_shared_light_locked (dev, &g_battery);
	}
}

static void handle_shared_buttons_locked (struct light_device_t *dev) {
	if (g_buttons_notification_support) {
		if (is_lit (&g_buttons)) {
			set_shared_buttons_locked (dev, &g_buttons);
		} else {
			set_shared_buttons_locked (dev, &g_buttons_notification);
		}
	} else {
		set_shared_buttons_locked (dev, &g_buttons);
	}
}

static int set_light_buttons (struct light_device_t *dev, struct light_state_t const* state) {
	pthread_mutex_lock(&g_lock);
	int brightness = rgb_to_brightness(&g_buttons);
	int new_brightness = rgb_to_brightness(state);
	if (new_brightness == DIM_BRIGHTNESS && brightness > (DIM_BRIGHTNESS + DIM_BRIGHTNESS_MAX_DROP)) {
		//Seems to go in DIM state
		g_buttons = *state;
	} else if (new_brightness < DIM_BRIGHTNESS) {
		//from DIM to Off
		g_buttons = *state;
	} else {
		//keep up to date
		g_buttons_before_dim = *state;
		g_buttons = *state;
	}
	handle_shared_buttons_locked(dev);
	pthread_mutex_unlock(&g_lock);
	return 0;
}

static int set_light_battery (struct light_device_t *dev, struct light_state_t const* state) {
	pthread_mutex_lock (&g_lock);
	g_battery = *state;
	handle_shared_battery_locked(dev);
	pthread_mutex_unlock (&g_lock);
	return 0;
}

static int set_light_notifications (struct light_device_t *dev, struct light_state_t const* state) {
	pthread_mutex_lock (&g_lock);

	g_notification = *state;
	g_buttons_notification = *state;
	if (is_lit (&g_notification)) {
		if (g_low_power)
			//use the known brightness before DIM state (buttons brightness linked with backlight brightness into nAOSP ROM)
			g_buttons_notification.color = g_buttons_before_dim.color;
		else
			g_buttons_notification.color = 255;
	}

	handle_shared_battery_locked(dev);
	handle_shared_buttons_locked(dev);
	pthread_mutex_unlock (&g_lock);
	return 0;
}

/* Initializations */
void init_globals () {
	// mutex
	pthread_mutex_init (&g_lock, NULL);
	
	// shared states
	g_notification.color = 0;
	g_notification.flashMode = LIGHT_FLASH_NONE;
	g_battery.color = 0;
	g_battery.flashMode = LIGHT_FLASH_NONE;
	g_buttons.color = 0;
	g_buttons.flashMode = LIGHT_FLASH_NONE;
	g_buttons_before_dim.color = 0;
	g_buttons_before_dim.flashMode = LIGHT_FLASH_NONE;
	g_buttons_notification.color = 0;
	g_buttons_notification.flashMode = LIGHT_FLASH_NONE;
	
	// buttons notification support ?
	char value[PROPERTY_VALUE_MAX];
	property_get("persist.sys.lightbar_flash", value, "1");
	g_buttons_notification_support = atoi(value);
	property_get("persist.sys.lightbar_lp", value, "1");
	g_low_power = atoi(value);
}

/* Glueing boilerplate */
static int close_lights (struct light_device_t *dev) {
	if (dev)
		free(dev);

	return 0;
}

static int open_lights (const struct hw_module_t* module, char const* name,
						struct hw_device_t** device) {
	int (*set_light)(struct light_device_t* dev,
					 struct light_state_t const *state);

	if (0 == strcmp(LIGHT_ID_BACKLIGHT, name)) {
		set_light = set_light_backlight;
	} else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
		set_light = set_light_buttons;
	} else if (0 == strcmp(LIGHT_ID_BATTERY, name)) {
		set_light = set_light_battery;
	} else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
		set_light = set_light_notifications;
	} else {
		return -EINVAL;
	}

	pthread_once (&g_init, init_globals);
	struct light_device_t *dev = malloc(sizeof (struct light_device_t));
	memset(dev, 0, sizeof(*dev));

	dev->common.tag 	= HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module 	= (struct hw_module_t*)module;
	dev->common.close 	= (int (*)(struct hw_device_t*))close_lights;
	dev->set_light 		= set_light;

	*device = (struct hw_device_t*)dev;
	return 0;
}

static struct hw_module_methods_t lights_module_methods = {
	.open = open_lights,
};


struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.version_major = 1,
	.version_minor = 0,
	.id = LIGHTS_HARDWARE_MODULE_ID,
	.name = "Sony lights module",
	.author = "Diogo Ferreira <defer@cyanogenmod.com>, Andreas Makris <Andreas.Makris@gmail.com>",
	.methods = &lights_module_methods,
};
