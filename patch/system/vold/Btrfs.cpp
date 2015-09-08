/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) 2014 The CyanogenMod Project
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <linux/kdev_t.h>
#include <logwrap/logwrap.h>
#include "VoldUtil.h"

#define LOG_TAG "Vold"
#include <cutils/log.h>
#include <cutils/properties.h>

#include <private/android_filesystem_config.h>

#include "Btrfs.h"

static char BTRFS_MKFS[] = "/system/bin/mkfs.btrfs";

int Btrfs::doMount(const char *fsPath, const char *mountPoint, bool ro, bool
        remount, bool executable, const char *mountOpts) {
    int rc;
    unsigned long flags;
    char data[1024];

    data[0] = '\0';
    if (mountOpts)
        strlcat(data, mountOpts, sizeof(data));

    flags = MS_NOATIME | MS_NODEV | MS_NOSUID | MS_DIRSYNC;

    flags |= (executable ? 0 : MS_NOEXEC);
    flags |= (ro ? MS_RDONLY : 0);
    flags |= (remount ? MS_REMOUNT : 0);

    rc = mount(fsPath, mountPoint, "btrfs", flags, data);

    if (rc && errno == EROFS) {
        SLOGE("%s appears to be a read only filesystem - retrying mount RO", fsPath);
        flags |= MS_RDONLY;
        rc = mount(fsPath, mountPoint, "btrfs", flags, data);
    }

    return rc;
}

int Btrfs::check(const char *fsPath) {

    SLOGW("Skipping fs checks, fsck.btrfs not yet supported upstream.\n");

    return 0;
}

int Btrfs::format(const char *fsPath) {

    const char *args[4];
    int rc = -1;
    int status;

    if (access(BTRFS_MKFS, X_OK)) {
        SLOGE("Unable to format, mkfs.btrfs not found.");
        return -1;
    }

    args[0] = BTRFS_MKFS;
    args[1] = "-O ^extref,^skinny-metadata";
    args[2] = fsPath;
    args[3] = NULL;

    rc = android_fork_execvp(ARRAY_SIZE(args), (char **)args, &status, false,
            true);

    if (rc == 0) {
        SLOGI("Filesystem (BTRFS) formatted OK");
        return 0;
    } else {
        SLOGE("Format (BTRFS) failed (unknown exit code %d)", rc);
        errno = EIO;
        return -1;
    }
    return 0;
}
