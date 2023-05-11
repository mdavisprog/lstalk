#include "../lstalk.h"
#include "utility.h"
#include <stdio.h>
#include <stdlib.h>

#if WINDOWS
    #define CLANGD "clangd.exe"
#else
    #define CLANGD "clangd"
#endif

static size_t malloc_count = 0;
static size_t malloc_size_total = 0;
static size_t free_count = 0;

static void* custom_malloc(size_t size) {
    malloc_count++;
    malloc_size_total += size;
    return malloc(size);
}

static void* custom_calloc(size_t num, size_t size) {
    malloc_count++;
    malloc_size_total += size * num;
    return calloc(num, size);
}

static void* custom_realloc(void* ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

static void custom_free(void* ptr) {
    free_count++;
    free(ptr);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    LSTalk_MemoryAllocator allocator;
    allocator.malloc = custom_malloc;
    allocator.calloc = custom_calloc;
    allocator.realloc = custom_realloc;
    allocator.free = custom_free;

    struct LSTalk_Context* context = lstalk_init_with_allocator(allocator);
    if (context == NULL) {
        return -1;
    }

    int major = 0;
    int minor = 0;
    int revision = 0;
    lstalk_version(&major, &minor, &revision);
    printf("LSTalk version %d.%d.%d\n", major, minor, revision);
    printf("Custom memory allocator example\n");

    LSTalk_ConnectParams params;
    params.root_uri = NULL;
    params.trace = LSTALK_TRACE_VERBOSE;
    params.seek_path_env = 1;
    LSTalk_ServerID server = lstalk_connect(context, CLANGD, &params);
    if (server != LSTALK_INVALID_SERVER_ID) {
        printf("Connecting to server...");
        while (lstalk_get_connection_status(context, server) != LSTALK_CONNECTION_STATUS_CONNECTED) {
            lstalk_process_responses(context);
        }
        printf("Success!\n");
        lstalk_close(context, server);
    }

    lstalk_shutdown(context);

    printf("Total malloc calls: %zu\n", malloc_count);
    printf("Total allocated bytes: %zu\n", malloc_size_total);
    printf("Total free calls: %zu\n", free_count);

    return 0;
}
