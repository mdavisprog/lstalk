/*

MIT License

Copyright (c) 2026 Mitchell Davis <mdavisprog@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#ifndef __LSTALK_H__
#define __LSTALK_H__

#include <stddef.h>

#if LSTALK_LIB
    #if LSTALK_STATIC
        #define LSTALK_API
    #else
        #ifndef LSTALK_API
            #if defined(_WIN32) || defined(_WIN64)
                #if LSTALK_EXPORT
                    #define LSTALK_API __declspec(dllexport)
                #else
                    #define LSTALK_API __declspec(dllimport)
                #endif
            #else
                #define LSTALK_API
            #endif
        #endif
    #endif
#else
    #define LSTALK_API
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct LSTalk_Allocator {
    void* (*malloc)(size_t);
    void* (*calloc)(size_t, size_t);
    void* (*realloc)(void*, size_t);
    void (*free)(void*);
} LSTalk_Allocator;

typedef enum { false, true } bool;

typedef struct LSTalk_Context {
    LSTalk_Allocator allocator;
} LSTalk_Context;

/**
 * Initializes a LSTalk_Context object given the memory allocator. The context is to
 * be used with all of the API functions.
 *
 * @return A heap-allocated LSTalk_Context object. Must be freed with lstalk_shutdown.
 */
LSTALK_API LSTalk_Context* lstalk_init(LSTalk_Allocator allocator);

/**
 * Cleans up a LSTalk_Context object. This will close any existing connections to servers
 * and send shutdown/exit requests to them. The context object memory is then freed.
 *
 * @param context - The context object to shutdown.
 */
LSTALK_API void lstalk_shutdown(LSTalk_Context* context);

/**
 * Retrieves the current version number for the LSTalk library.
 *
 * @param major - A pointer to store the major version number.
 * @param minor - A pointer to store the minor version number.
 * @param revision - A pointer to store the revision number.
 */
LSTALK_API void lstalk_version(int* major, int* minor, int* revision);

#if defined(__cplusplus)
}
#endif

#endif // __LSTALK_H__
