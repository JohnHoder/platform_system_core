/*
 * Copyright (C) 2016 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// This file contains files implementation that can be shared between
// platforms as long as the correct headers are included.
#define _GNU_SOURCE 1 // for asprintf

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cutils/files.h>

#ifndef TEMP_FAILURE_RETRY // _WIN32 does not define
#define TEMP_FAILURE_RETRY(exp) (exp)
#endif

int android_get_control_file(const char* path) {
    if (!path) return -1;

    char *key = NULL;
    if (asprintf(&key, ANDROID_FILE_ENV_PREFIX "%s", path) < 0) return -1;
    if (!key) return -1;

    char *cp = key;
    while (*cp) {
        if (!isalnum(*cp)) *cp = '_';
        ++cp;
    }

    const char* val = getenv(key);
    free(key);
    if (!val) return -1;

    errno = 0;
    long fd = strtol(val, NULL, 10);
    if (errno) return -1;

    // validity checking
    if ((fd < 0) || (fd > INT_MAX)) return -1;
#if defined(_SC_OPEN_MAX)
    if (fd >= sysconf(_SC_OPEN_MAX)) return -1;
#elif defined(OPEN_MAX)
    if (fd >= OPEN_MAX) return -1;
#elif defined(_POSIX_OPEN_MAX)
    if (fd >= _POSIX_OPEN_MAX) return -1;
#endif

#if defined(F_GETFD)
    if (TEMP_FAILURE_RETRY(fcntl(fd, F_GETFD)) < 0) return -1;
#elif defined(F_GETFL)
    if (TEMP_FAILURE_RETRY(fcntl(fd, F_GETFL)) < 0) return -1;
#else
    struct stat s;
    if (TEMP_FAILURE_RETRY(fstat(fd, &s)) < 0) return -1;
#endif

#if defined(__linux__)
    char *proc = NULL;
    if (asprintf(&proc, "/proc/self/fd/%ld", fd) < 0) return -1;
    if (!proc) return -1;

    size_t len = strlen(path);
    char *buf = static_cast<char *>(calloc(1, len + 2));
    if (!buf) {
        free(proc);
        return -1;
    }
    ssize_t ret = TEMP_FAILURE_RETRY(readlink(proc, buf, len + 1));
    free(proc);
    int cmp = (len != static_cast<size_t>(ret)) || strcmp(buf, path);
    free(buf);
    if (ret < 0) return -1;
    if (cmp != 0) return -1;
#endif

    // It is what we think it is
    return static_cast<int>(fd);
}
