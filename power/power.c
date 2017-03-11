/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (c) 2017 nAOSP ROM
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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LOG_TAG "nPowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define CPU1_ONLINE_PATH "/sys/devices/system/cpu/cpu1/online"

#define SCALING_GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define POWERSAVE_GOVERNOR "powersave"
#define CONSERVATIVE_GOVERNOR "conservative"
#define ONDEMAND_GOVERNOR "ondemand"
#define INTERACTIVE_GOVERNOR "interactive"
#define SMARTMAX_GOVERNOR "smartmax"

#define CPUFREQ_ONDEMAND "/sys/devices/system/cpu/cpufreq/ondemand/"
#define CPUFREQ_ONDEMAND_UP_THRESHOLD "/sys/devices/system/cpu/cpufreq/ondemand/up_threshold"
#define CPUFREQ_ONDEMAND_IO_IS_BUSY "/sys/devices/system/cpu/cpufreq/ondemand/io_is_busy"
#define CPUFREQ_ONDEMAND_SAMPLING_DOWN_FACTOR "/sys/devices/system/cpu/cpufreq/ondemand/sampling_down_factor"
#define CPUFREQ_ONDEMAND_DOWN_DIFFERENTIAL "/sys/devices/system/cpu/cpufreq/ondemand/down_differential"
#define CPUFREQ_ONDEMAND_SAMPLING_RATE "/sys/devices/system/cpu/cpufreq/ondemand/sampling_rate"

#define CPUFREQ_INTERACTIVE "/sys/devices/system/cpu/cpufreq/interactive/"
#define CPUFREQ_INTERACTIVE_MIN_SAMPLE_TIME "/sys/devices/system/cpu/cpufreq/interactive/min_sample_time"
#define CPUFREQ_INTERACTIVE_HISPEED_FREQ "/sys/devices/system/cpu/cpufreq/interactive/hispeed_freq"
#define CPUFREQ_INTERACTIVE_ABOVE_HISPEED_DELAY "/sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay"
#define CPUFREQ_INTERACTIVE_TIMER_RATE "/sys/devices/system/cpu/cpufreq/interactive/timer_rate"

#define BOOSTPULSE_ONDEMAND "/sys/devices/system/cpu/cpufreq/ondemand/boostpulse"
#define BOOSTPULSE_INTERACTIVE "/sys/devices/system/cpu/cpufreq/interactive/boostpulse"
#define SAMPLING_RATE_SCREEN_ON "50000"
#define SAMPLING_RATE_SCREEN_OFF "500000"
#define TIMER_RATE_SCREEN_ON "30000"
#define TIMER_RATE_SCREEN_OFF "500000"

typedef enum {
  POWERSAVE = 0,
  CONSERVATIVE,
  ONDEMAND,
  INTERACTIVE,
  SMARTMAX
} governor_t;

typedef enum {
  OFFLINE = 0,
  ONLINE
} cpu_online_t;

struct n_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    governor_t current_scaling_governor;
    int low_power;
    int interactive;
};

int sysfs_read(char *path, char *s, int num_bytes);
int sysfs_write(char *path, char *s);
void configure_cpufreq(struct n_power_module *pm);
void configure_gpu(struct n_power_module *pm);
void configure_dt2w(struct n_power_module *pm);
void set_cpu1_online(cpu_online_t online);
void boostpulse(struct n_power_module *pm, int duration);
void boostpulse_open(struct n_power_module *pm);
void boostpulse_close(struct n_power_module *pm);

int sysfs_read(char *path, char *s, int num_bytes)
{
    char buf[80];
    int count;
    int ret = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1;
    }
    if ((count = read(fd, s, num_bytes - 1)) < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
        ret = -1;
    } else {
        s[count] = '\0';
    }
    close(fd);
    return ret;
}

int sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int ret = 0;
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1 ;
    }
    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
        ret = -1;
    }
    close(fd);
    return ret;
}

int set_scaling_governor(struct n_power_module *pm, governor_t governor)
{
  const int size = 20;
  char scaling_governor[size];

  switch(pm->current_scaling_governor) {
    case INTERACTIVE:
    case ONDEMAND:
      boostpulse_close(pm);
    default:
      break;
  }

  switch(governor) {
    case CONSERVATIVE:
      strcpy(scaling_governor, CONSERVATIVE_GOVERNOR);
      break;
    case INTERACTIVE:
      strcpy(scaling_governor, INTERACTIVE_GOVERNOR);
      boostpulse_close(pm);
      break;
    case POWERSAVE:
      strcpy(scaling_governor, POWERSAVE_GOVERNOR);
      break;
    case ONDEMAND:
      strcpy(scaling_governor, ONDEMAND_GOVERNOR);
      break;
    case SMARTMAX:
      strcpy(scaling_governor, SMARTMAX_GOVERNOR);
      break;
    default:
      // Fallback to interactive
      strcpy(scaling_governor, INTERACTIVE_GOVERNOR);
      governor = INTERACTIVE;
  }

  if (sysfs_write(SCALING_GOVERNOR_PATH, scaling_governor) == -1) {
    // Can't set the scaling governor. Return.
    return -1;
  }

  pm->current_scaling_governor = governor;

  // post
  switch(pm->current_scaling_governor) {
    case INTERACTIVE:
    case ONDEMAND:
      boostpulse_open(pm);
      break;
    default:
      break;
  }

  return 0;
}

void power_set_interactive(struct power_module *module, int on)
{
  struct n_power_module *pm = (struct n_power_module *) module;

  ALOGD("Set Interactive : %d", on);

  pm->interactive = on;

  //TODO: Manage the cpu clock but need to handle conflict with thermanager and oc tools

  if (pm->low_power && on) {
    // interactive but in low power
    set_cpu1_online(OFFLINE);
    set_scaling_governor(pm, CONSERVATIVE);
  } else if (pm->low_power) {
    // non-interactive but in low power
    set_cpu1_online(OFFLINE);
    set_scaling_governor(pm, POWERSAVE);
  } else if (on) {
    // interactive
    set_cpu1_online(ONLINE);
    set_scaling_governor(pm, INTERACTIVE);
  } else {
    // non-interactive
    set_cpu1_online(OFFLINE);
    set_scaling_governor(pm, CONSERVATIVE);
  }

  configure_cpufreq(pm);
  configure_gpu(pm);
}

void configure_cpufreq(struct n_power_module *pm)
{
    switch(pm->current_scaling_governor) {
      case INTERACTIVE:
        sysfs_write(CPUFREQ_INTERACTIVE_MIN_SAMPLE_TIME, "90000");
        sysfs_write(CPUFREQ_INTERACTIVE_HISPEED_FREQ, "1134000");
        sysfs_write(CPUFREQ_INTERACTIVE_ABOVE_HISPEED_DELAY, "30000");
        sysfs_write(CPUFREQ_INTERACTIVE_TIMER_RATE,
                pm->interactive ? TIMER_RATE_SCREEN_ON : TIMER_RATE_SCREEN_OFF);
        break;
      case ONDEMAND:
        sysfs_write(CPUFREQ_ONDEMAND_UP_THRESHOLD, "90");
        sysfs_write(CPUFREQ_ONDEMAND_IO_IS_BUSY, "1");
        sysfs_write(CPUFREQ_ONDEMAND_SAMPLING_DOWN_FACTOR, "4");
        sysfs_write(CPUFREQ_ONDEMAND_DOWN_DIFFERENTIAL, "10");
        sysfs_write(CPUFREQ_ONDEMAND_SAMPLING_RATE,
                pm->interactive ? SAMPLING_RATE_SCREEN_ON : SAMPLING_RATE_SCREEN_OFF);
        break;
      default:
        break;
    }
}

void configure_gpu(struct n_power_module *pm)
{
  //TODO: implement clock and governor control
}

void configure_dt2w(struct n_power_module *pm)
{
  //TODO: implement android_touch control
}

void set_cpu1_online(cpu_online_t online)
{
  sysfs_write(CPU1_ONLINE_PATH, online == ONLINE ? "1" : "0");
}

void boostpulse(struct n_power_module *pm, int duration)
{
  if (duration != 0 && pm->boostpulse_fd > 0) {
    char buf[80];
    int len;
    snprintf(buf, sizeof(buf), "%d", duration);
    len = write(pm->boostpulse_fd, buf, strlen(buf));

    if (len < 0)
      boostpulse_close(pm);
  }
}

void boostpulse_close(struct n_power_module *pm)
{
  pthread_mutex_lock(&pm->lock);
  if (pm->boostpulse_fd > 0) {
    close(pm->boostpulse_fd);
    pm->boostpulse_fd = -1;
  }
  pthread_mutex_unlock(&pm->lock);
}

void boostpulse_open(struct n_power_module *pm)
{
  pthread_mutex_lock(&pm->lock);

  if (pm->boostpulse_fd < 0) {
    switch (pm->current_scaling_governor) {
      case INTERACTIVE:
        pm->boostpulse_fd = open(BOOSTPULSE_INTERACTIVE, O_WRONLY);
        break;
      case ONDEMAND:
        pm->boostpulse_fd = open(BOOSTPULSE_ONDEMAND, O_WRONLY);
        break;
      default:
        break;
    }
  }

  pthread_mutex_unlock(&pm->lock);
}

static void power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    struct n_power_module *pm = (struct n_power_module *) module;

    switch (hint) {
      case POWER_HINT_VSYNC:
        break;
      case POWER_HINT_INTERACTION:
        // User is interacting with the device
        boostpulse(pm, (int)data);
        break;
      case POWER_HINT_LOW_POWER:
        pm->low_power = data ? 1 : 0;
        ALOGD("Low power : %d", pm->low_power);
        configure_cpufreq(pm);
        configure_gpu(pm);
        configure_dt2w(pm);
        break;
      case POWER_HINT_SUSTAINED_PERFORMANCE:
      case POWER_HINT_VR_MODE:
        //TODO: smartmax, performance ? overclocking ? the target is to have stable and high performance
        break;
      default:
        break;
    }
}

static void power_init(struct power_module *module)
{
    struct n_power_module *pm = (struct n_power_module *) module;

    ALOGD("Init");

    set_scaling_governor(pm, INTERACTIVE);
    configure_cpufreq(pm);
    configure_gpu(pm);
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct n_power_module HAL_MODULE_INFO_SYM = {
    .base = {
        .common = {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = POWER_MODULE_API_VERSION_0_3,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = POWER_HARDWARE_MODULE_ID,
            .name = "nAOSP ROM Power HAL",
            .author = "mickybart",
            .methods = &power_module_methods,
        },
       .init = power_init,
       .setInteractive = power_set_interactive,
       .powerHint = power_hint,
    },
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .boostpulse_fd = -1,
    .current_scaling_governor = INTERACTIVE,
    .low_power = 0,
    .interactive = 1,
};
