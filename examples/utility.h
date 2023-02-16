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
    strncpy(out, path, length);
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
    #error("utility_absolute_path not implemented for Windows!")
#else
    realpath(relative_path, out);
#endif
}

static void utility_sleep(unsigned int ms) {
#if WINDOWS
    Sleep(ms);
#else
    usleep(1000L * ms);
#endif
}

#endif
