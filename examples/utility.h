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

static void utility_sleep(unsigned int ms) {
#if WINDOWS
    Sleep(ms);
#else
    usleep(1000L * ms);
#endif
}

#endif
