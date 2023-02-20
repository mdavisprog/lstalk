#ifndef __UTILITY_H__
#define __UTILITY_H__

#if defined(_WIN32) || defined(_WIN64)
    #define WINDOWS 1
#endif

#if WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define PATH_MAX MAX_PATH
#else
    #include <unistd.h>
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !WINDOWS && __STDC_VERSION__ <= 199901L
static int strncpy_s(char* restrict dest, size_t destsz, const char* restrict src, size_t count) {
    (void)destsz;
    strncpy(dest, src, count);
    return 0;
}

static int strcat_s(char* restrict dest, size_t destsz, const char* restrict src) {
    (void)destsz;
    strcat(dest, src);
    return 0;
}
#endif

static void utility_get_directory(char* path, char* out, size_t out_size) {
    char* anchor = path;
    char* ptr = anchor;
    while (*ptr != 0) {
        char ch = *ptr;
        if (ch == '\\' || ch == '/') {
            anchor = ptr;
        }
        ptr++;
    }

    size_t length = anchor - path;
    if (length == 0) {
        length = strlen(path);
    }

    length = length > out_size ? out_size : length;
    strncpy_s(out, out_size, path, length);
    out[length] = 0;
}

static void utility_absolute_path(char* relative_path, char* out, size_t out_size) {
    if (out == NULL) {
        return;
    }

    if (relative_path == NULL) {
        out[0] = 0;
    }

#if WINDOWS
    GetFullPathNameA(relative_path, (DWORD)out_size, out, NULL);
#else
    (void)out_size;
    realpath(relative_path, out);
#endif
}

#if DEFINE_SLEEP
static void utility_sleep(unsigned int ms) {
#if WINDOWS
    Sleep(ms);
#else
    usleep(1000L * ms);
#endif
}
#endif

#endif
