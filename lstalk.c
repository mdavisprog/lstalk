/*

MIT License

Copyright (c) 2023 Mitchell Davis <mdavisprog@gmail.com>

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

#include "lstalk.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//
// Version information
//

#define LSTALK_MAJOR 0
#define LSTALK_MINOR 2
#define LSTALK_REVISION 0

//
// Platform definitions
//

#if _WIN32 || _WIN64
    #define LSTALK_WINDOWS 1
#elif __APPLE__
    #define LSTALK_APPLE 1
#elif __linux__
    #define LSTALK_LINUX 1
#else
    #error "Current platform is not supported."
#endif

#if LSTALK_APPLE || LSTALK_LINUX
    #define LSTALK_POSIX 1
#endif

//
// Platform includes
//

#if LSTALK_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#elif LSTALK_POSIX
    #include <errno.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

//
// C Standard compliant functions.
//
// MSVC provides secure functions in the C99 standard that other
// compilers do not. These functions will be defined here for
// operability with these platforms.

#if !LSTALK_WINDOWS && __STDC_VERSION <= 199901L
static int strcpy_s(char* restrict dest, size_t destsz, const char* restrict src) {
    (void)destsz;
    char* result = strcpy(dest, src);
    if (result != dest) {
        return EINVAL;
    }
    return EXIT_SUCCESS;
}

static int strncpy_s(char* restrict dest, size_t destsz, const char* src, size_t count) {
    (void)destsz;

#if LSTALK_LINUX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
    char* result = strncpy(dest, src, count);
#if LSTALK_LINUX
#pragma GCC diagnostic pop
#endif
    if (result != dest) {
        return EINVAL;
    }
    return EXIT_SUCCESS;
}

#if LSTALK_TESTS
// This function is currently only used in testing. Add to main library when needed.
static int strncat_s(char* restrict dest, size_t destsz, const char* restrict src, size_t count) {
    (void)destsz;

    char* result = strncat(dest, src, count);
    if (result != dest) {
        return EINVAL;
    }
    return EXIT_SUCCESS;
}
#endif

static int fopen_s(FILE* restrict* restrict streamptr, const char* restrict filename, const char* restrict mode) {
    *streamptr = fopen(filename, mode);
    if (*streamptr == NULL) {
        return EINVAL;
    }
    return EXIT_SUCCESS;
}

#define sprintf_s(dest, bufsz, format, ...) (void)bufsz; sprintf(dest, format, __VA_ARGS__)
#define sscanf_s(buffer, format, ...) sscanf(buffer, format, __VA_ARGS__)
#endif

#if _MSC_VER
    #define MAYBE_UNUSED
#elif __GNUC__
    #define MAYBE_UNUSED __attribute__((unused))
#endif

//
// Dynamic Array
//
// Very simple dynamic array structure to aid in managing storage and
// retrieval of data.

typedef struct Vector {
    char* data;
    size_t element_size;
    size_t length;
    size_t capacity;
} Vector;

static Vector vector_create(size_t element_size, LSTalk_MemoryAllocator* allocator) {
    Vector result;
    result.element_size = element_size;
    result.length = 0;
    result.capacity = 1;
    result.data = allocator->malloc(element_size * result.capacity);
    return result;
}

static void vector_destroy(Vector* vector, LSTalk_MemoryAllocator* allocator) {
    if (vector == NULL) {
        return;
    }

    if (vector->data != NULL) {
        allocator->free(vector->data);
    }

    vector->data = NULL;
    vector->element_size = 0;
    vector->length = 0;
    vector->capacity = 0;
}

static void vector_resize(Vector* vector, size_t capacity, LSTalk_MemoryAllocator* allocator) {
    if (vector == NULL || vector->element_size == 0) {
        return;
    }

    vector->capacity = capacity;
    vector->data = allocator->realloc(vector->data, vector->element_size * vector->capacity);
}

static void vector_push(Vector* vector, void* element, LSTalk_MemoryAllocator* allocator) {
    if (vector == NULL || element == NULL || vector->element_size == 0) {
        return;
    }

    if (vector->length == vector->capacity) {
        vector_resize(vector, vector->capacity * 2, allocator);
    }

    size_t offset = vector->length * vector->element_size;
    memcpy(vector->data + offset, element, vector->element_size);
    vector->length++;
}

static void vector_append(Vector* vector, void* elements, size_t count, LSTalk_MemoryAllocator* allocator) {
    if (vector == NULL || elements == NULL || vector->element_size == 0) {
        return;
    }

    size_t remaining = vector->capacity - vector->length;

    if (count > remaining) {
        vector_resize(vector, vector->capacity + (count - remaining) * 2, allocator);
    }

    size_t offset = vector->length * vector->element_size;
    memcpy(vector->data + offset, elements, count * vector->element_size);
    vector->length += count;
}

static int vector_remove(Vector* vector, size_t index) {
    if (vector == NULL || vector->element_size == 0 || index >= vector->length) {
        return 0;
    }

    // If removed the final index, then do nothing.
    if (index == vector->length - 1) {
        vector->length--;
        return 1;
    }

    char* start = vector->data + index * vector->element_size;
    char* end = start + vector->element_size;
    size_t count = vector->length - (index + 1);
    size_t size = count * vector->element_size;
    memmove(start, end, size);
    vector->length--;
    return 1;
}

static char* vector_get(Vector* vector, size_t index) {
    if (vector == NULL || vector->element_size == 0 || index >= vector->length) {
        return NULL;
    }

    return &vector->data[vector->element_size * index];
}

//
// String Functions
//
// Section that contains functions to help manage strings.

static char* string_alloc_copy(const char* source, LSTalk_MemoryAllocator* allocator) {
    size_t length = strlen(source);
    char* result = (char*)allocator->malloc(length + 1);
    strcpy_s(result, length + 1, source);
    return result;
}

static void string_free_array(char** array, size_t count, LSTalk_MemoryAllocator* allocator) {
    if (array == NULL) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (array[i] != NULL) {
            allocator->free(array[i]);
        }
    }

    allocator->free(array);
}

//
// File functions
//
// This section contains utility functions when operating on files.

static char* file_get_contents(const char* path, LSTalk_MemoryAllocator* allocator) {
    if (path == NULL || allocator == NULL || allocator->malloc == NULL) {
        return NULL;
    }

    FILE* file = NULL;
    fopen_s(&file, path, "rb");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    if (size == 0) {
        fclose(file);
        return NULL;
    }

    char* result = (char*)allocator->malloc(sizeof(char) * size + 1);
    size_t read = fread(result, sizeof(char), size, file);
    result[read] = '\0';
    fclose(file);

    return result;
}

MAYBE_UNUSED static int file_write_contents(const char* path, const char* contents) {
    if (path == NULL || contents == NULL) {
        return 0;
    }

    FILE* file = NULL;
    fopen_s(&file, path, "w");
    if (file == NULL) {
        return 0;
    }

    size_t length = strlen(contents);
    size_t written = fwrite(contents, sizeof(char), length, file);
    fclose(file);

    if (written != length) {
        return 0;
    }

    return 1;
}

static char* file_uri(const char* path, LSTalk_MemoryAllocator* allocator) {
    if (path == NULL) {
        return NULL;
    }

    char* scheme = "file:///";
    size_t scheme_length = strlen(scheme);
    size_t path_length = strlen(path);
    Vector result = vector_create(sizeof(char), allocator);
    vector_resize(&result, scheme_length + path_length + 1, allocator);
    vector_append(&result, scheme, scheme_length, allocator);
    vector_append(&result, (void*)path, path_length, allocator);
    result.data[scheme_length + path_length] = 0;
    return result.data;
}

static char* file_extension(const char* path, LSTalk_MemoryAllocator* allocator) {
    if (path == NULL) {
        return NULL;
    }

    char* result = NULL;
    const char* ptr = path;
    while (ptr != NULL) {
        const char* start = ptr;
        ptr = strchr(ptr + 1, '.');
        if (ptr == NULL) {
            result = string_alloc_copy(start + 1, allocator);
        }
    }

    return result;
}

#if LSTALK_WINDOWS
static int file_exists(wchar_t* path) {
    if (path == NULL) {
        return 0;
    }

    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return 0;
    }

    return !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static char* file_async_read(HANDLE handle, LSTalk_MemoryAllocator* allocator) {
    if (handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DWORD total_bytes_avail = 0;
    if (!PeekNamedPipe(handle, NULL, 0, NULL, &total_bytes_avail, NULL)) {
        printf("Failed to peek for number of bytes!\n");
        return NULL;
    }

    if (total_bytes_avail == 0) {
        return NULL;
    }

    char* read_buffer = (char*)allocator->malloc(sizeof(char) * total_bytes_avail + 1);
    DWORD read = 0;
    BOOL read_result = ReadFile(handle, read_buffer, total_bytes_avail, &read, NULL);
    if (!read_result || read == 0) {
        printf("Failed to read from process stdout.\n");
        allocator->free(read_buffer);
        return NULL;
    }
    read_buffer[read] = 0;
    return read_buffer;
}
#elif LSTALK_POSIX
static int file_exists(char* path) {
    if (path == NULL) {
        return 0;
    }

    struct stat info;
    if (stat(path, &info) != 0) {
        return 0;
    }

    if (!(info.st_mode & S_IFREG)) {
        return 0;
    }

    return 1;
}

#define READ_SIZE 4096

static char* file_async_read(int handle, LSTalk_MemoryAllocator* allocator) {
    if (handle < 0) {
        return NULL;
    }

    Vector array = vector_create(sizeof(char), allocator);
    char buffer[READ_SIZE];
    int bytes_read = read(handle, (void*)buffer, sizeof(buffer));
    if (bytes_read == -1) {
        vector_destroy(&array, allocator);
        return NULL;
    }

    while (bytes_read > 0) {
        vector_append(&array, (void*)buffer, (size_t)bytes_read, allocator);
        if (bytes_read < READ_SIZE) {
            break;
        }

        bytes_read = read(handle, (void*)buffer, sizeof(buffer));
    }

    char* result = (char*)allocator->malloc(sizeof(char) * array.length + 1);
    strncpy(result, array.data, array.length);
    result[array.length] = 0;
    vector_destroy(&array, allocator);
    return result;
}
#endif

#if LSTALK_TESTS
// These functions are currently only used in testing. Add to main library when needed.
static void file_get_directory(char* path, char* out, size_t out_size) {
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

static void file_to_absolute_path(char* relative_path, char* out, size_t out_size) {
    if (out == NULL) {
        return;
    }

    if (relative_path == NULL) {
        out[0] = 0;
    }

#if LSTALK_WINDOWS
    GetFullPathNameA(relative_path, (DWORD)out_size, out, NULL);
#else
    (void)out_size;
    if (realpath(relative_path, out) == NULL) {
        out = NULL;
    }
#endif
}

static char* file_async_read_stdin(LSTalk_MemoryAllocator* allocator) {
#if LSTALK_WINDOWS
    return file_async_read(GetStdHandle(STD_INPUT_HANDLE), allocator);
#elif LSTALK_POSIX
    return file_async_read(STDIN_FILENO, allocator);
#else
    #error "Not implemented for current platform!"
#endif
}
#endif

//
// Process Management
//
// This section will manage the creation/destruction of a process. All platform implementations should be
// provided here.

struct Process;

#if LSTALK_WINDOWS

//
// Process Management Windows
//

#define PATH_MAX 32767

typedef struct StdHandles {
    HANDLE child_stdin_read;
    HANDLE child_stdin_write;
    HANDLE child_stdout_read;
    HANDLE child_stdout_write;
} StdHandles;

static void process_close_handles(StdHandles* handles) {
    CloseHandle(handles->child_stdin_read);
    CloseHandle(handles->child_stdin_write);
    CloseHandle(handles->child_stdout_read);
    CloseHandle(handles->child_stdout_write);
}

typedef struct Process {
    StdHandles std_handles;
    PROCESS_INFORMATION info;
} Process;

static Process* process_create_windows(const char* path, int seek_path_env, LSTalk_MemoryAllocator* allocator) {
    StdHandles handles;
    handles.child_stdin_read = NULL;
    handles.child_stdin_write = NULL;
    handles.child_stdout_read = NULL;
    handles.child_stdout_write = NULL;

    SECURITY_ATTRIBUTES security_attr;
    ZeroMemory(&security_attr, sizeof(security_attr));
    security_attr.bInheritHandle = TRUE;
    security_attr.lpSecurityDescriptor = NULL;

    // CreateProcessW does not accept a path larger than 32767.
    wchar_t wpath[PATH_MAX];
    size_t retval = 0;
    mbstowcs_s(&retval, wpath, PATH_MAX, path, PATH_MAX);

    if (seek_path_env) {
        wchar_t path_var[PATH_MAX];
        GetEnvironmentVariableW(L"PATH", path_var, PATH_MAX);
        wchar_t* anchor = path_var;
        while (anchor != NULL) {
            wchar_t item[PATH_MAX];
            wchar_t* end = wcschr(anchor, L';');
            if (end != NULL) {
                size_t length = end - anchor;
                wcsncpy_s(item, PATH_MAX, anchor, length);
                item[length] = 0;
                anchor = end + 1;
            } else {
                wcscpy_s(item, PATH_MAX, anchor);
                anchor = end;
            }

            wchar_t full_path[PATH_MAX];
            full_path[0] = 0;
            wcscat_s(full_path, PATH_MAX, item);
            wcscat_s(full_path, PATH_MAX, L"\\");
            wcscat_s(full_path, PATH_MAX, wpath);

            if (file_exists(full_path)) {
                wcscpy_s(wpath, PATH_MAX, full_path);
                break;
            }
        }
    }

    if (!CreatePipe(&handles.child_stdout_read, &handles.child_stdout_write, &security_attr, 0)) {
        printf("Failed to create stdout pipe!\n");
        return NULL;
    }

    if (!SetHandleInformation(handles.child_stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        printf("Failed to set handle information for stdout read!\n");
        return NULL;
    }

    if (!CreatePipe(&handles.child_stdin_read, &handles.child_stdin_write, &security_attr, 0)) {
        printf("Failed to create stdin pipe!\n");
        return NULL;
    }

    if (!SetHandleInformation(handles.child_stdin_write, HANDLE_FLAG_INHERIT, 0)) {
        printf("Failed to set handle information for stdin write!\n");
        return NULL;
    }

    STARTUPINFOW startup_info;
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.hStdError = handles.child_stdout_write;
    startup_info.hStdOutput = handles.child_stdout_write;
    startup_info.hStdInput = handles.child_stdin_read;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION process_info;
    ZeroMemory(&process_info, sizeof(process_info));

    BOOL result = CreateProcessW(wpath, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &startup_info, &process_info);
    if (!result) {
        printf("Failed to create child process.\n");
        process_close_handles(&handles);
        return NULL;
    }

    Process* process = (Process*)allocator->malloc(sizeof(Process));
    process->std_handles = handles;
    process->info = process_info;
    return process;
}

static void process_close_windows(Process* process, LSTalk_MemoryAllocator* allocator) {
    if (process == NULL) {
        return;
    }

    TerminateProcess(process->info.hProcess, 0);
    process_close_handles(&process->std_handles);
    allocator->free(process);
}

static char* process_read_windows(Process* process, LSTalk_MemoryAllocator* allocator) {
    if (process == NULL) {
        return NULL;
    }

    return file_async_read(process->std_handles.child_stdout_read, allocator);
}

static void process_write_windows(Process* process, const char* request) {
    if (process == NULL) {
        return;
    }

    DWORD written = 0;
    if (!WriteFile(process->std_handles.child_stdin_write, (void*)request, (DWORD)strlen(request), &written, NULL)) {
        printf("Failed to write to process stdin.\n");
    }
}

static int process_get_current_id_windows() {
    return (int)GetCurrentProcessId();
}

#elif LSTALK_POSIX

//
// Process Management Posix
//

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef struct Pipes {
    int in[2];
    int out[2];
} Pipes;

static void process_close_pipes(Pipes* pipes) {
    if (pipes == NULL) {
        return;
    }

    close(pipes->in[PIPE_READ]);
    close(pipes->in[PIPE_WRITE]);
    close(pipes->out[PIPE_READ]);
    close(pipes->out[PIPE_WRITE]);
}

typedef struct Process {
    Pipes pipes;
    pid_t pid;
} Process;

static Process* process_create_posix(const char* path, int seek_path_env, LSTalk_MemoryAllocator* allocator) {
    char final_path[PATH_MAX];
    strcpy(final_path, path);

    if (seek_path_env) {
        char* path_var = getenv("PATH");
        char* anchor = path_var;
        while (anchor != NULL) {
            char item[PATH_MAX];
            char* end = strchr(anchor, ':');
            if (end != NULL) {
                size_t length = end - anchor;
                strncpy(item, anchor, length);
                item[length] = 0;
                anchor = end + 1;
            } else {
                strcpy(item, anchor);
                anchor = end;
            }

            char full_path[PATH_MAX] = "";
            strcat(full_path, item);
            strcat(full_path, "/");
            strcat(full_path, path);

            if (file_exists(full_path)) {
                strcpy(final_path, full_path);
                break;
            }
        }
    }

    if (!file_exists(final_path)) {
        return NULL;
    }

    Pipes pipes;

    if (pipe(pipes.in) < 0) {
        printf("Failed to create stdin pipes!\n");
        return NULL;
    }

    if (pipe(pipes.out) < 0) {
        printf("Failed to create stdout pipes!\n");
        process_close_pipes(&pipes);
        return NULL;
    }

    pid_t pid = fork();

    if (pid == 0) {
        if (dup2(pipes.in[PIPE_READ], STDIN_FILENO) < 0) {
            printf("Failed to duplicate stdin read pipe.\n");
            exit(-1);
        }

        if (dup2(pipes.out[PIPE_WRITE], STDOUT_FILENO) < 0) {
            printf("Failed to duplicate stdout write pipe.\n");
            exit(-1);
        }

        if (dup2(pipes.out[PIPE_WRITE], STDERR_FILENO) < 0) {
            printf("Failed to duplicate stderr write pipe.\n");
            exit(-1);
        }

        // Close pipes that are used by the parent process.
        process_close_pipes(&pipes);

        char* args[] = {final_path, NULL};
        int error = execv(final_path, args);
        if (error == -1) {
            printf("Failed to execv child process!\n");
            exit(-1);
        }
    } else if (pid < 0) {
        printf("Failed to create child process!\n");
        return NULL;
    }

    close(pipes.in[PIPE_READ]);
    close(pipes.out[PIPE_WRITE]);

    fcntl(pipes.out[PIPE_READ], F_SETFL, O_NONBLOCK);

    Process* process = (Process*)allocator->malloc(sizeof(Process));
    process->pipes = pipes;
    process->pid = pid;

    return process;
}

static void process_close_posix(Process* process, LSTalk_MemoryAllocator* allocator) {
    if (process == NULL) {
        return;
    }

    process_close_pipes(&process->pipes);
    kill(process->pid, SIGKILL);
    allocator->free(process);
}

static char* process_read_posix(Process* process, LSTalk_MemoryAllocator* allocator) {
    if (process == NULL) {
        return NULL;
    }

    return file_async_read(process->pipes.out[PIPE_READ], allocator);
}

static void process_write_posix(Process* process, const char* request) {
    if (process == NULL) {
        return;
    }

    ssize_t bytes_written = write(process->pipes.in[PIPE_WRITE], (void*)request, strlen(request));
    if (bytes_written < 0) {
        printf("Failed to write to child process.\n");
    }
}

static int process_get_current_id_posix() {
    return (int)getpid();
}

#endif

//
// Process Management functions
//

static Process* process_create(const char* path, int seek_path_env, LSTalk_MemoryAllocator* allocator) {
#if LSTALK_WINDOWS
    return process_create_windows(path, seek_path_env, allocator);
#elif LSTALK_POSIX
    return process_create_posix(path, seek_path_env, allocator);
#else
    #error "Current platform does not implement create_process"
#endif
}

static void process_close(Process* process, LSTalk_MemoryAllocator* allocator) {
#if LSTALK_WINDOWS
    process_close_windows(process, allocator);
#elif LSTALK_POSIX
    process_close_posix(process, allocator);
#else
    #error "Current platform does not implement close_process"
#endif
}

// Platform specific handling should allocate the string on the heap
// and the caller is responsible for freeing the result.
static char* process_read(Process* process, LSTalk_MemoryAllocator* allocator) {
#if LSTALK_WINDOWS
    return process_read_windows(process, allocator);
#elif LSTALK_POSIX
    return process_read_posix(process, allocator);
#else
    #error "Current platform does not implement read_response"
#endif
}

static void process_write(Process* process, const char* request) {
#if LSTALK_WINDOWS
    process_write_windows(process, request);
#elif LSTALK_POSIX
    process_write_posix(process, request);
#else
    #error "Current platform does not implement write_request"
#endif
}

static int process_get_current_id() {
#if LSTALK_WINDOWS
    return process_get_current_id_windows();
#elif LSTALK_POSIX
    return process_get_current_id_posix();
#else
    #error "Current platform does not implement get_current_process_id"
#endif
}

static void process_request(Process* process, const char* request, LSTalk_MemoryAllocator* allocator) {
    size_t length = strlen(request);

    // Temporary buffer length.
    // TODO: Is there a way to eliminate this heap allocation?
    size_t buffer_size = length + 40;
    char* buffer = (char*)allocator->malloc(buffer_size);
    sprintf_s(buffer, buffer_size, "Content-Length: %zu\r\n\r\n%s", length, request);
    process_write(process, buffer);
    allocator->free(buffer);
}

//
// JSON API
//
// This section contains all functionality for interacting with JSON objects
// and streams.

typedef enum {
    JSON_VALUE_NULL,
    JSON_VALUE_BOOLEAN,
    JSON_VALUE_INT,
    JSON_VALUE_FLOAT,
    JSON_VALUE_STRING,
    // Special type used to point to a const string that doesn't need to be freed.
    JSON_VALUE_STRING_CONST,
    JSON_VALUE_OBJECT,
    JSON_VALUE_ARRAY,
} JSON_VALUE_TYPE;

static char* json_escape_string(char* source, LSTalk_MemoryAllocator* allocator) {
    if (source == NULL) {
        return NULL;
    }

    Vector array = vector_create(sizeof(char), allocator);

    char* start = source;
    char* ptr = start;
    size_t length = 0;
    while (start != NULL) {
        char ch = *ptr;
        char escaped = 0;
        switch (ch) {
            case '"': escaped = '"'; break;
            case '\\': escaped = '\\'; break;
            case '/': escaped = '/'; break;
            case '\b': escaped = 'b'; break;
            case '\f': escaped = 'f'; break;
            case '\n': escaped = 'n'; break;
            case '\r': escaped = 'r'; break;
            case '\t': escaped = 't'; break;
            default: break;
        }
        if (ch != 0) {
            if (escaped != 0) {
                size_t count = ptr - start;
                length += count + 2;
                vector_append(&array, start, count, allocator);
                char escape = '\\';
                vector_push(&array, &escape, allocator);
                vector_push(&array, &escaped, allocator);
                start = ptr + 1;
            }
        // TODO: Handle unicode escape characters.
        } else if (ch == '\0') {
            size_t count = ptr - start;
            length += count;
            vector_append(&array, start, count, allocator);
            start = NULL;
        }
        ptr++;
    }

    char* result = NULL;
    if (array.length > 0) {
        result = (char*)allocator->malloc(sizeof(char) * length + 1);
        strncpy_s(result, length + 1, array.data, length);
        result[length] = '\0';
    }

    vector_destroy(&array, allocator);
    return result;
}

static char* json_unescape_string(char* source, LSTalk_MemoryAllocator* allocator) {
    if (source == NULL) {
        return NULL;
    }

    Vector array = vector_create(sizeof(char), allocator);

    char* start = source;
    char* ptr = start;
    size_t length = 0;
    while (start != NULL) {
        char ch = *ptr;
        if (ch == '\\') {
            char next = *(ptr + 1);
            char unescaped = 0;
            switch (next) {
                case '"': unescaped = '"'; break;
                case '\\': unescaped = '\\'; break;
                case '/': unescaped = '/'; break;
                case 'b': unescaped = '\b'; break;
                case 'f': unescaped = '\f'; break;
                case 'n': unescaped = '\n'; break;
                case 'r': unescaped = '\r'; break;
                case 't': unescaped = '\t'; break;
                default: break;
            }
            if (unescaped != 0) {
                size_t count = ptr - start;
                length += count + 1;
                vector_append(&array, start, count, allocator);
                vector_push(&array, &unescaped, allocator);
                // Advance to the unescaped character. The loop will move to the next character.
                ptr++;
                start = ptr + 1;
            }
            // TODO: Handle unicode escape characters.
        } else if (ch == '\0') {
            size_t count = ptr - start;
            length += count;
            vector_append(&array, start, count, allocator);
            start = NULL;
        }
        ptr++;
    }

    char* result = NULL;
    if (array.length > 0) {
        result = (char*)allocator->malloc(sizeof(char) * length + 1);
        strncpy_s(result, length + 1, array.data, length);
        result[length] = '\0';
    }

    vector_destroy(&array, allocator);
    return result;
}

struct JSONObject;
struct JSONArray;

typedef struct JSONValue {
    union {
        unsigned char bool_value;
        int int_value;
        float float_value;
        char* string_value;
        struct JSONObject* object_value;
        struct JSONArray* array_value;
    } value;
    JSON_VALUE_TYPE type;
} JSONValue;

typedef struct JSONPair {
    // Should either be a JSON_VALUE_STRING or JSON_VALUE_STRING_CONST
    JSONValue key;
    JSONValue value;
} JSONPair;

typedef struct JSONObject {
    Vector pairs;
} JSONObject;

typedef struct JSONArray {
    Vector values;
} JSONArray;

typedef struct JSONEncoder {
    Vector string;
} JSONEncoder;

static void json_to_string(JSONValue* value, Vector* vector, LSTalk_MemoryAllocator* allocator);
static void json_object_to_string(JSONObject* object, Vector* vector, LSTalk_MemoryAllocator* allocator) {
    if (object == NULL || vector == NULL || vector->element_size != 1) {
        return;
    }

    vector_append(vector, (void*)"{", 1, allocator);
    for (size_t i = 0; i < object->pairs.length; i++) {
        JSONPair* pair = (JSONPair*)vector_get(&object->pairs, i);
        json_to_string(&pair->key, vector, allocator);
        vector_append(vector, (void*)": ", 2, allocator);
        json_to_string(&pair->value, vector, allocator);

        if (i + 1 < object->pairs.length) {
            vector_append(vector, (void*)", ", 2, allocator);
        }
    }
    vector_append(vector, (void*)"}", 1, allocator);
}

static void json_array_to_string(JSONArray* array, Vector* vector, LSTalk_MemoryAllocator* allocator) {
    if (array == NULL || vector == NULL || vector->element_size != 1) {
        return;
    }

    vector_append(vector, (void*)"[", 1, allocator);
    for (size_t i = 0; i < array->values.length; i++) {
        JSONValue* value = (JSONValue*)vector_get(&array->values, i);
        json_to_string(value, vector, allocator);

        if (i + 1 < array->values.length) {
            vector_append(vector, (void*)", ", 2, allocator);
        }
    }
    vector_append(vector, (void*)"]", 1, allocator);
}

static void json_to_string(JSONValue* value, Vector* vector, LSTalk_MemoryAllocator* allocator) {
    // The vector object must be created with an element size of 1.

    if (value == NULL || vector == NULL || vector->element_size != 1) {
        return;
    }

    switch (value->type) {
        case JSON_VALUE_BOOLEAN: {
            if (value->value.bool_value) {
                vector_append(vector, (void*)"true", 4, allocator);
            } else {
                vector_append(vector, (void*)"false", 5, allocator);
            }
        } break;

        case JSON_VALUE_INT: {
            char buffer[40];
            sprintf_s(buffer, sizeof(buffer), "%d", value->value.int_value);
            vector_append(vector, (void*)buffer, strlen(buffer), allocator);
        } break;

        case JSON_VALUE_FLOAT: {
            char buffer[40];
            sprintf_s(buffer, sizeof(buffer), "%f", value->value.float_value);
            vector_append(vector, (void*)buffer, strlen(buffer), allocator);
        } break;

        case JSON_VALUE_STRING_CONST:
        case JSON_VALUE_STRING: {
            vector_append(vector, (void*)"\"", 1, allocator);
            vector_append(vector, (void*)value->value.string_value, strlen(value->value.string_value), allocator);
            vector_append(vector, (void*)"\"", 1, allocator);
        } break;

        case JSON_VALUE_OBJECT: {
            json_object_to_string(value->value.object_value, vector, allocator);
        } break;

        case JSON_VALUE_ARRAY: {
            json_array_to_string(value->value.array_value, vector, allocator);
        } break;

        case JSON_VALUE_NULL: {
            vector_append(vector, (void*)"null", 4, allocator);
        } break;

        default: break;
    }
}

static void json_destroy_value(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    if (value == NULL) {
        return;
    }

    switch (value->type) {
        case JSON_VALUE_STRING: {
            if (value->value.string_value != NULL) {
                allocator->free(value->value.string_value);
            }
        } break;

        case JSON_VALUE_OBJECT: {
            JSONObject* object = value->value.object_value;
            if (object != NULL) {
                for (size_t i = 0; i < object->pairs.length; i++) {
                    JSONPair* pair = (JSONPair*)vector_get(&object->pairs, i);
                    json_destroy_value(&pair->key, allocator);
                    json_destroy_value(&pair->value, allocator);
                }
                vector_destroy(&object->pairs, allocator);
                allocator->free(object);
            }
        } break;

        case JSON_VALUE_ARRAY: {
            JSONArray* array = value->value.array_value;
            if (array != NULL) {
                for (size_t i = 0; i < array->values.length; i++) {
                    JSONValue* item = (JSONValue*)vector_get(&array->values, i);
                    json_destroy_value(item, allocator);
                }
                vector_destroy(&array->values, allocator);
                allocator->free(array);
            }
        } break;

        default: break;
    }

    value->type = JSON_VALUE_NULL;
    value->value.int_value = 0;
}

static JSONValue json_make_null() {
    JSONValue result;
    result.type = JSON_VALUE_NULL;
    result.value.int_value = 0;
    return result;
}

static JSONValue json_make_boolean(lstalk_bool value) {
    JSONValue result;
    result.type = JSON_VALUE_BOOLEAN;
    result.value.bool_value = value;
    return result;
}

static JSONValue json_make_int(int value) {
    JSONValue result;
    result.type = JSON_VALUE_INT;
    result.value.int_value = value;
    return result;
}

static JSONValue json_make_float(float value) {
    JSONValue result;
    result.type = JSON_VALUE_FLOAT;
    result.value.float_value = value;
    return result;
}

static JSONValue json_make_string(char* value, LSTalk_MemoryAllocator* allocator) {
    if (value == NULL) {
        return json_make_null();
    }

    JSONValue result;
    result.type = JSON_VALUE_STRING;
    result.value.string_value = string_alloc_copy(value, allocator);
    return result;
}

static JSONValue json_make_owned_string(char* value) {
    JSONValue result;
    result.type = JSON_VALUE_STRING;
    result.value.string_value = value;
    return result;
}

static JSONValue json_make_string_const(char* value) {
    JSONValue result;
    result.type = JSON_VALUE_STRING_CONST;
    result.value.string_value = value;
    return result;
}

static JSONValue json_make_object(LSTalk_MemoryAllocator* allocator) {
    JSONValue result;
    result.type = JSON_VALUE_OBJECT;
    result.value.object_value = (JSONObject*)allocator->malloc(sizeof(JSONObject));
    result.value.object_value->pairs = vector_create(sizeof(JSONPair), allocator);
    return result;
}

static JSONValue json_make_array(LSTalk_MemoryAllocator* allocator) {
    JSONValue result;
    result.type = JSON_VALUE_ARRAY;
    result.value.array_value = (JSONArray*)allocator->malloc(sizeof(JSONArray));
    result.value.array_value->values = vector_create(sizeof(JSONValue), allocator);
    return result;
}

static char* json_move_string(JSONValue* value) {
    if (value == NULL || value->type != JSON_VALUE_STRING) {
        return NULL;
    }

    char* result = value->value.string_value;
    value->type = JSON_VALUE_STRING_CONST;

    return result;
}

static JSONValue* json_object_get_ptr(JSONValue* object, char* key) {
    JSONValue* result = NULL;

    if (object == NULL || object->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONObject* obj = object->value.object_value;
    for (size_t i = 0; i < obj->pairs.length; i++) {
        JSONPair* pair = (JSONPair*)vector_get(&obj->pairs, i);

        if (strcmp(pair->key.value.string_value, key) == 0) {
            result = &pair->value;
            break;
        }
    }

    return result;
}

static JSONValue json_object_get(JSONValue* object, char* key) {
    JSONValue* ptr = json_object_get_ptr(object, key);
    if (ptr == NULL) {
        return json_make_null();
    }

    return *ptr;
}

static void json_object_set(JSONValue* object, JSONValue key, JSONValue value, LSTalk_MemoryAllocator* allocator) {
    if (object == NULL || object->value.object_value == NULL || object->type != JSON_VALUE_OBJECT) {
        return;
    }

    if (key.type != JSON_VALUE_STRING && key.type != JSON_VALUE_STRING_CONST) {
        return;
    }

    int found = 0;
    JSONObject* obj = object->value.object_value;
    for (size_t i = 0; i < obj->pairs.length; i++) {
        JSONPair* pair = (JSONPair*)vector_get(&obj->pairs, i);

        if (pair->key.type != JSON_VALUE_STRING && pair->key.type != JSON_VALUE_STRING_CONST) {
            continue;
        }

        if (strcmp(pair->key.value.string_value, key.value.string_value) == 0) {
            json_destroy_value(&pair->value, allocator);
            pair->value = value;
            found = 1;
            break;
        }
    }

    if (!found) {
        JSONPair pair;
        pair.key = key;
        pair.value = value;
        vector_push(&object->value.object_value->pairs, (void*)&pair, allocator);
    }
}

static void json_object_const_key_set(JSONValue* object, char* key, JSONValue value, LSTalk_MemoryAllocator* allocator) {
    json_object_set(object, json_make_string_const(key), value, allocator);
}

static void json_array_push(JSONValue* array, JSONValue value, LSTalk_MemoryAllocator* allocator) {
    if (array == NULL || array->type != JSON_VALUE_ARRAY) {
        return;
    }

    JSONArray* arr = array->value.array_value;
    vector_push(&arr->values, (void*)&value, allocator);
}

static JSONValue* json_array_get_ptr(JSONValue* array, size_t index) {
    JSONValue* result = NULL;

    if (array == NULL || array->type != JSON_VALUE_ARRAY) {
        return result;
    }

    JSONArray* arr = array->value.array_value;

    if (index >= arr->values.length) {
        return result;
    }

    result = (JSONValue*)vector_get(&arr->values, index);

    return result;
}

static JSONValue json_array_get(JSONValue* array, size_t index) {
    JSONValue* result = json_array_get_ptr(array, index);
    if (result == NULL) {
        return json_make_null();
    }

    return *result;
}

static size_t json_array_length(JSONValue* array) {
    if (array == NULL || array->type != JSON_VALUE_ARRAY) {
        return 0;
    }

    return array->value.array_value->values.length;
}

static JSONValue json_make_string_array(char** array, int count, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);
    for (int i = 0; i < count; i++) {
        // TODO: Should this be json_make_owned_string to prevent an allocation?
        json_array_push(&result, json_make_string(array[i], allocator), allocator);
    }
    return result;
}

static JSONEncoder json_encode(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    JSONEncoder encoder;
    encoder.string = vector_create(sizeof(char), allocator);
    json_to_string(value, &encoder.string, allocator);
    vector_append(&encoder.string, (void*)"\0", 1, allocator);
    return encoder;
}

static void json_destroy_encoder(JSONEncoder* encoder, LSTalk_MemoryAllocator* allocator) {
    vector_destroy(&encoder->string, allocator);
}

MAYBE_UNUSED static void json_print(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    if (value == NULL) {
        return;
    }

    JSONEncoder encoder = json_encode(value, allocator);
    printf("%s\n", encoder.string.data);
    json_destroy_encoder(&encoder, allocator);
}

//
// JSON Parsing Functions
//
// This section contains functions that parses a JSON stream into a JSONValue.

typedef struct Lexer {
    char* buffer;
    char* delimiters;
    char* ptr;
    LSTalk_MemoryAllocator* allocator;
} Lexer;

typedef struct Token {
    char* ptr;
    size_t length;
} Token;

static int token_compare(Token* token, const char* value) {
    if (token == NULL || token->length == 0) {
        return 0;
    }

    return strncmp(token->ptr, value, token->length) == 0;
}

static char* token_make_string(Token* token, LSTalk_MemoryAllocator* allocator) {
    if (token == NULL) {
        return NULL;
    }

    // TODO: Make json_unescape_string take length to prevent an additional
    // memory allocation here.
    char* buffer = (char*)allocator->malloc(sizeof(char) * token->length + 1);
    strncpy_s(buffer, token->length + 1, token->ptr, token->length);
    buffer[token->length] = 0;

    char* result = json_unescape_string(buffer, allocator);
    allocator->free(buffer);
    return result;
}

static Token lexer_get_token(Lexer* lexer) {
    Token result;
    result.ptr = NULL;
    result.length = 0;

    size_t length = 0;
    char* ptr = lexer->ptr;
    while (*ptr != 0) {
        char ch = *ptr;
        ptr++;
        length = ptr - lexer->ptr;

        if (isspace(ch)) {
            // If there are valid characters to make a token, return the result here.
            if (length > 1) {
                result.ptr = lexer->ptr;
                result.length = ptr - lexer->ptr;
                lexer->ptr = ptr;
                return result;
            }

            // If we have reached a space character and there is no token, then update the current
            // lexer pointer to remove the space from consideration of a token.
            lexer->ptr = ptr;
        }

        // Check if this character is a delimiter.
        char* delimiter = lexer->delimiters;
        while (*delimiter != 0) {
            if (ch == *delimiter) {
                length = ptr - lexer->ptr;
                // If there is enough to create a separate token, then adjust the pointer to remove
                // the delimiter and apply that on the next token.
                if (length > 1) {
                    length--;
                    ptr--;
                }
                result.ptr = lexer->ptr;
                result.length = ptr - lexer->ptr;
                lexer->ptr = ptr;
                return result;
            }
            delimiter++;
        }
    }

    // The end of the lexing stream has been found. Return whatever token is left.
    if (length > 0) {
        result.ptr = lexer->ptr;
        result.length = ptr - lexer->ptr;
        lexer->ptr = ptr;
    }

    return result;
}

static Token lexer_parse_until(Lexer* lexer, char term) {
    char* ptr = lexer->ptr;
    while (*ptr != 0 && *ptr != term) {
        ptr++;
    }

    Token result;
    // Don't include the terminator in the string.
    result.ptr = lexer->ptr;
    result.length = (ptr - lexer->ptr);
    lexer->ptr = ptr;

    // Advance the pointer if we are not at the end.
    if (*lexer->ptr != 0) {
        lexer->ptr++;
    }

    return result;
}

static Token lexer_parse_string(Lexer* lexer) {
    char* ptr = lexer->ptr;
    int is_escaped = 0;
    while (*ptr != 0) {
        if (*ptr == '"') {
            if (!is_escaped) {
                break;
            }
        }

        is_escaped = 0;

        if (*ptr == '\\') {
            is_escaped = 1;
        }
        ptr++;
    }

    Token result;
    result.ptr = lexer->ptr;
    result.length = (ptr - lexer->ptr);
    lexer->ptr = ptr;

    // Advance the pointer if we are not at the end.
    if (*lexer->ptr != 0) {
        lexer->ptr++;
    }

    return result;
}

static JSONValue json_decode_number(Token* token) {
    JSONValue result = json_make_null();

    if (token == NULL || token->length == 0) {
        return result;
    }

    char buffer[UCHAR_MAX];
    memcpy(buffer, token->ptr, token->length + 1);
    buffer[token->length] = '\0';

    char* end = NULL;
    if (strchr(buffer, '.') == NULL) {
        result = json_make_int(strtol(buffer, &end, 10));
    } else {
        result = json_make_float(strtof(buffer, &end));
    }

    return result;
}

static JSONValue json_decode_value(Token* token, Lexer* lexer);
static JSONValue json_decode_object(Lexer* lexer) {
    JSONValue result = json_make_null();

    if (lexer == NULL) {
        return result;
    }

    Token token = lexer_get_token(lexer);
    if (token.length == 0) {
        return result;
    }

    result = json_make_object(lexer->allocator);

    // Could be an empty object.
    while (!token_compare(&token, "}")) {
        if (!token_compare(&token, "\"")) {
            json_destroy_value(&result, lexer->allocator);
            return result;
        }

        token = lexer_parse_until(lexer, '"');
        JSONValue key;
        key.type = JSON_VALUE_STRING;
        key.value.string_value = token_make_string(&token, lexer->allocator);

        token = lexer_get_token(lexer);
        if (!token_compare(&token, ":")) {
            json_destroy_value(&key, lexer->allocator);
            json_destroy_value(&result, lexer->allocator);
            return result;
        }

        token = lexer_get_token(lexer);
        JSONValue value = json_decode_value(&token, lexer);
        json_object_set(&result, key, value, lexer->allocator);

        token = lexer_get_token(lexer);
        if (token_compare(&token, "}")) {
            break;
        }

        if (!token_compare(&token, ",")) {
            json_destroy_value(&result, lexer->allocator);
            break;
        }

        token = lexer_get_token(lexer);
    }

    return result;
}

static JSONValue json_decode_array(Lexer* lexer) {
    JSONValue result = json_make_null();

    if (lexer == NULL) {
        return result;
    }

    result = json_make_array(lexer->allocator);

    Token token = lexer_get_token(lexer);
    while (!token_compare(&token, "]")) {
        JSONValue value = json_decode_value(&token, lexer);
        json_array_push(&result, value, lexer->allocator);

        token = lexer_get_token(lexer);
        if (token_compare(&token, "]")) {
            break;
        }

        if (!token_compare(&token, ",")) {
            json_destroy_value(&result, lexer->allocator);
            break;
        }

        token = lexer_get_token(lexer);
    }

    return result;
}

static JSONValue json_decode_value(Token* token, Lexer* lexer) {
    JSONValue result = json_make_null();

    if (token == NULL) {
        return result;
    }

    if (token->length > 0)
    {
        if (token_compare(token, "{")) {
            result = json_decode_object(lexer);
        } else if (token_compare(token, "[")) {
            result = json_decode_array(lexer);
        } else if (token_compare(token, "\"")) {
            Token literal = lexer_parse_string(lexer);
            // Need to create the string value manually due to allocating a copy of the
            // token.
            result.type = JSON_VALUE_STRING;
            result.value.string_value = token_make_string(&literal, lexer->allocator);
        } else if (token_compare(token, "true")) {
            result = json_make_boolean(1);
        } else if (token_compare(token, "false")) {
            result = json_make_boolean(0);
        } else if (token_compare(token, "null")) {
            result = json_make_null();
        } else {
            result = json_decode_number(token);
        }
    }

    return result;
}

static JSONValue json_decode(char* stream, LSTalk_MemoryAllocator* allocator) {
    Lexer lexer;
    lexer.buffer = stream;
    lexer.delimiters = "\":{}[],";
    lexer.ptr = stream;
    lexer.allocator = allocator;

    Token token = lexer_get_token(&lexer);
    return json_decode_value(&token, &lexer);
}

//
// RPC Functions
//
// This section will contain functions to create JSON-RPC objects that can be encoded and sent
// to the language server.

typedef struct Request {
    int id;
    JSONValue payload;
} Request;

static void rpc_message(JSONValue* object, LSTalk_MemoryAllocator* allocator) {
    if (object == NULL || object->type != JSON_VALUE_OBJECT) {
        return;
    }

    json_object_const_key_set(object, "jsonrpc", json_make_string_const("2.0"), allocator);
}

static JSONValue rpc_make_notification(char* method, JSONValue params, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_null();
    
    if (method == NULL) {
        return result;
    }

    result = json_make_object(allocator);
    rpc_message(&result, allocator);
    json_object_const_key_set(&result, "method", json_make_string_const(method), allocator);

    if (params.type == JSON_VALUE_OBJECT || params.type == JSON_VALUE_ARRAY) {
        json_object_const_key_set(&result, "params", params, allocator);
    }

    return result;
}

static Request rpc_make_notification_request(char* method, JSONValue params, LSTalk_MemoryAllocator* allocator) {
    Request result;
    result.id = 0;
    result.payload = rpc_make_notification(method, params, allocator);
    return result;
}

static Request rpc_make_request(int* id, char* method, JSONValue params, LSTalk_MemoryAllocator* allocator) {
    Request result;
    result.id = 0;
    result.payload = json_make_null();

    if (id == NULL) {
        return result;
    }

    result = rpc_make_notification_request(method, params, allocator);
    json_object_const_key_set(&result.payload, "id", json_make_int(*id), allocator);
    result.id = *id;
    (*id)++;
    return result;
}

static char* rpc_get_method(Request* request) {
    if (request == NULL) {
        return NULL;
    }

    JSONValue method = json_object_get(&request->payload, "method");
    if (method.type != JSON_VALUE_STRING_CONST) {
        return NULL;
    }

    return method.value.string_value;
}

static void rpc_send_request(Process* server, Request* request, int print_request, LSTalk_MemoryAllocator* allocator) {
    if (server == NULL || request == NULL) {
        return;
    }

    JSONEncoder encoder = json_encode(&request->payload, allocator);
    process_request(server, encoder.string.data, allocator);
    if (print_request) {
        printf("%s\n", encoder.string.data);
    }
    json_destroy_encoder(&encoder, allocator);
}

static void rpc_close_request(Request* request, LSTalk_MemoryAllocator* allocator) {
    if (request == NULL) {
        return;
    }

    json_destroy_value(&request->payload, allocator);
}

//
// LSTalk_Trace conversions
//

static char* trace_to_string(LSTalk_Trace trace) {
    switch (trace) {
        case LSTALK_TRACE_MESSAGES: return "messages";
        case LSTALK_TRACE_VERBOSE: return "verbose";
        case LSTALK_TRACE_OFF:
        default: break;
    }

    return "off";
}

static LSTalk_Trace trace_from_string(const char* trace) {
    if (strcmp(trace, "messages") == 0) {
        return LSTALK_TRACE_MESSAGES;
    } else if (strcmp(trace, "verbose") == 0) {
        return LSTALK_TRACE_VERBOSE;
    }

    return LSTALK_TRACE_OFF;
}

//
// Begin Client Capabilities
//

/**
 * The kind of resource operations supported by the client.
 */
typedef enum {
    /**
     * Supports creating new files and folders.
     */
    RESOURCEOPERATIONKIND_CREATE = 1 << 0,

    /**
     * Supports renaming existing files and folders.
     */
    RESOURCEOPERATIONKIND_RENAME = 1 << 1,

    /**
     * Supports deleting existing files and folders.
     */
    RESOURCEOPERATIONKIND_DELETE = 1 << 2,
} ResourceOperationKind;

static JSONValue resource_operation_kind_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & RESOURCEOPERATIONKIND_CREATE) { json_array_push(&result, json_make_string_const("create"), allocator); }
    if (value & RESOURCEOPERATIONKIND_RENAME) { json_array_push(&result, json_make_string_const("rename"), allocator); }
    if (value & RESOURCEOPERATIONKIND_DELETE) { json_array_push(&result, json_make_string_const("delete"), allocator); }

    return result;
}

/**
 * The failure handling strategy of a client if applying the workspace edit
 * fails.
 *
 * @since 3.13.0
 */
typedef enum {
    /**
     * Applying the workspace change is simply aborted if one of the changes
     * provided fails. All operations executed before the failing operation
     * stay executed.
     */
    FAILUREHANDLINGKIND_ABORT = 1 << 0,

    /**
     * All operations are executed transactional. That means they either all
     * succeed or no changes at all are applied to the workspace.
     */
    FAILUREHANDLINGKIND_TRANSACTIONAL = 1 << 1,

    /**
     * If the workspace edit contains only textual file changes they are
     * executed transactional. If resource changes (create, rename or delete
     * file) are part of the change the failure handling strategy is abort.
     */
    FAILUREHANDLINGKIND_TEXTONLYTRANSACTIONAL = 1 << 2,

    /**
     * The client tries to undo the operations already executed. But there is no
     * guarantee that this is succeeding.
     */
    FAILUREHANDLINGKIND_UNDO = 1 << 3,
} FailureHandlingKind;

static JSONValue failure_handling_kind_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & FAILUREHANDLINGKIND_ABORT) { json_array_push(&result, json_make_string_const("abort"), allocator); }
    if (value & FAILUREHANDLINGKIND_TRANSACTIONAL) { json_array_push(&result, json_make_string_const("transactional"), allocator); }
    if (value & FAILUREHANDLINGKIND_TEXTONLYTRANSACTIONAL) { json_array_push(&result, json_make_string_const("textOnlyTransactional"), allocator); }
    if (value & FAILUREHANDLINGKIND_UNDO) { json_array_push(&result, json_make_string_const("undo"), allocator); }

    return result;
}

typedef enum {
    SYMBOLKIND_File = 1,
    SYMBOLKIND_Module = 2,
    SYMBOLKIND_Namespace = 3,
    SYMBOLKIND_Package = 4,
    SYMBOLKIND_Class = 5,
    SYMBOLKIND_Method = 6,
    SYMBOLKIND_Property = 7,
    SYMBOLKIND_Field = 8,
    SYMBOLKIND_Constructor = 9,
    SYMBOLKIND_Enum = 10,
    SYMBOLKIND_Interface = 11,
    SYMBOLKIND_Function = 12,
    SYMBOLKIND_Variable = 13,
    SYMBOLKIND_Constant = 14,
    SYMBOLKIND_String = 15,
    SYMBOLKIND_Number = 16,
    SYMBOLKIND_Boolean = 17,
    SYMBOLKIND_Array = 18,
    SYMBOLKIND_Object = 19,
    SYMBOLKIND_Key = 20,
    SYMBOLKIND_Null = 21,
    SYMBOLKIND_EnumMember = 22,
    SYMBOLKIND_Struct = 23,
    SYMBOLKIND_Event = 24,
    SYMBOLKIND_Operator = 25,
    SYMBOLKIND_TypeParameter = 26,
} SymbolKind;

static JSONValue symbol_kind_make_array(long long value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & LSTALK_SYMBOLKIND_FILE) { json_array_push(&result, json_make_int(SYMBOLKIND_File), allocator); }
    if (value & LSTALK_SYMBOLKIND_MODULE) { json_array_push(&result, json_make_int(SYMBOLKIND_Module), allocator); }
    if (value & LSTALK_SYMBOLKIND_NAMESPACE) { json_array_push(&result, json_make_int(SYMBOLKIND_Namespace), allocator); }
    if (value & LSTALK_SYMBOLKIND_PACKAGE) { json_array_push(&result, json_make_int(SYMBOLKIND_Package), allocator); }
    if (value & LSTALK_SYMBOLKIND_CLASS) { json_array_push(&result, json_make_int(SYMBOLKIND_Class), allocator); }
    if (value & LSTALK_SYMBOLKIND_METHOD) { json_array_push(&result, json_make_int(SYMBOLKIND_Method), allocator); }
    if (value & LSTALK_SYMBOLKIND_PROPERTY) { json_array_push(&result, json_make_int(SYMBOLKIND_Property), allocator); }
    if (value & LSTALK_SYMBOLKIND_FIELD) { json_array_push(&result, json_make_int(SYMBOLKIND_Field), allocator); }
    if (value & LSTALK_SYMBOLKIND_CONSTRUCTOR) { json_array_push(&result, json_make_int(SYMBOLKIND_Constructor), allocator); }
    if (value & LSTALK_SYMBOLKIND_ENUM) { json_array_push(&result, json_make_int(SYMBOLKIND_Enum), allocator); }
    if (value & LSTALK_SYMBOLKIND_INTERFACE) { json_array_push(&result, json_make_int(SYMBOLKIND_Interface), allocator); }
    if (value & LSTALK_SYMBOLKIND_FUNCTION) { json_array_push(&result, json_make_int(SYMBOLKIND_Function), allocator); }
    if (value & LSTALK_SYMBOLKIND_VARIABLE) { json_array_push(&result, json_make_int(SYMBOLKIND_Variable), allocator); }
    if (value & LSTALK_SYMBOLKIND_CONSTANT) { json_array_push(&result, json_make_int(SYMBOLKIND_Constant), allocator); }
    if (value & LSTALK_SYMBOLKIND_STRING) { json_array_push(&result, json_make_int(SYMBOLKIND_String), allocator); }
    if (value & LSTALK_SYMBOLKIND_NUMBER) { json_array_push(&result, json_make_int(SYMBOLKIND_Number), allocator); }
    if (value & LSTALK_SYMBOLKIND_BOOLEAN) { json_array_push(&result, json_make_int(SYMBOLKIND_Boolean), allocator); }
    if (value & LSTALK_SYMBOLKIND_ARRAY) { json_array_push(&result, json_make_int(SYMBOLKIND_Array), allocator); }
    if (value & LSTALK_SYMBOLKIND_OBJECT) { json_array_push(&result, json_make_int(SYMBOLKIND_Object), allocator); }
    if (value & LSTALK_SYMBOLKIND_KEY) { json_array_push(&result, json_make_int(SYMBOLKIND_Key), allocator); }
    if (value & LSTALK_SYMBOLKIND_NULL) { json_array_push(&result, json_make_int(SYMBOLKIND_Null), allocator); }
    if (value & LSTALK_SYMBOLKIND_ENUMMEMBER) { json_array_push(&result, json_make_int(SYMBOLKIND_EnumMember), allocator); }
    if (value & LSTALK_SYMBOLKIND_STRUCT) { json_array_push(&result, json_make_int(SYMBOLKIND_Struct), allocator); }
    if (value & LSTALK_SYMBOLKIND_EVENT) { json_array_push(&result, json_make_int(SYMBOLKIND_Event), allocator); }
    if (value & LSTALK_SYMBOLKIND_OPERATOR) { json_array_push(&result, json_make_int(SYMBOLKIND_Operator), allocator); }
    if (value & LSTALK_SYMBOLKIND_TYPEPARAMETER) { json_array_push(&result, json_make_int(SYMBOLKIND_TypeParameter), allocator); }

    return result;
}

static LSTalk_SymbolKind symbol_kind_parse(JSONValue* value) {
    if (value == NULL || value->type != JSON_VALUE_INT) {
        return LSTALK_SYMBOLKIND_NONE;
    }

    int kind = value->value.int_value;
    switch (kind) {
        case SYMBOLKIND_File: return LSTALK_SYMBOLKIND_FILE;
        case SYMBOLKIND_Module: return LSTALK_SYMBOLKIND_MODULE;
        case SYMBOLKIND_Namespace: return LSTALK_SYMBOLKIND_NAMESPACE;
        case SYMBOLKIND_Package: return LSTALK_SYMBOLKIND_PACKAGE;
        case SYMBOLKIND_Class: return LSTALK_SYMBOLKIND_CLASS;
        case SYMBOLKIND_Method: return LSTALK_SYMBOLKIND_METHOD;
        case SYMBOLKIND_Property: return LSTALK_SYMBOLKIND_PROPERTY;
        case SYMBOLKIND_Field: return LSTALK_SYMBOLKIND_FIELD;
        case SYMBOLKIND_Constructor: return LSTALK_SYMBOLKIND_CONSTRUCTOR;
        case SYMBOLKIND_Enum: return LSTALK_SYMBOLKIND_ENUM;
        case SYMBOLKIND_Interface: return LSTALK_SYMBOLKIND_INTERFACE;
        case SYMBOLKIND_Function: return LSTALK_SYMBOLKIND_FUNCTION;
        case SYMBOLKIND_Variable: return LSTALK_SYMBOLKIND_VARIABLE;
        case SYMBOLKIND_Constant: return LSTALK_SYMBOLKIND_CONSTANT;
        case SYMBOLKIND_String: return LSTALK_SYMBOLKIND_STRING;
        case SYMBOLKIND_Number: return LSTALK_SYMBOLKIND_NUMBER;
        case SYMBOLKIND_Boolean: return LSTALK_SYMBOLKIND_BOOLEAN;
        case SYMBOLKIND_Array: return LSTALK_SYMBOLKIND_ARRAY;
        case SYMBOLKIND_Object: return LSTALK_SYMBOLKIND_OBJECT;
        case SYMBOLKIND_Key: return LSTALK_SYMBOLKIND_KEY;
        case SYMBOLKIND_Null: return LSTALK_SYMBOLKIND_NULL;
        case SYMBOLKIND_EnumMember: return LSTALK_SYMBOLKIND_ENUMMEMBER;
        case SYMBOLKIND_Struct: return LSTALK_SYMBOLKIND_STRUCT;
        case SYMBOLKIND_Event: return LSTALK_SYMBOLKIND_EVENT;
        case SYMBOLKIND_Operator: return LSTALK_SYMBOLKIND_OPERATOR;
        case SYMBOLKIND_TypeParameter: return LSTALK_SYMBOLKIND_TYPEPARAMETER;
        default: break;
    }

    return LSTALK_SYMBOLKIND_NONE;
}

typedef enum {
    SYMBOLTAG_Deprecated = 1,
} SymbolTag;

static JSONValue symbol_tags_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & LSTALK_SYMBOLTAG_DEPRECATED) { json_array_push(&result, json_make_int(SYMBOLTAG_Deprecated), allocator); }

    return result;
}

/**
 * Describes the content type that a client supports in various
 * result literals like `Hover`, `ParameterInfo` or `CompletionItem`.
 *
 * Please note that `MarkupKinds` must not start with a `$`. This kinds
 * are reserved for internal usage.
 */
typedef enum {
    /**
     * Plain text is supported as a content format
     */
    MARKUPKIND_PLAINTEXT = 1 << 0,

    /**
     * Markdown is supported as a content format
     */
    MARKUPKIND_MARKDOWN = 1 << 1,
} MarkupKind;

MAYBE_UNUSED static MarkupKind markup_kind_parse(JSONValue* value) {
    if (value == NULL || value->type != JSON_VALUE_STRING) {
        return MARKUPKIND_PLAINTEXT;
    }

    if (strcmp(value->value.string_value, "markdown") == 0) {
        return MARKUPKIND_MARKDOWN;
    }

    return MARKUPKIND_PLAINTEXT;
}

static JSONValue markup_kind_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & MARKUPKIND_PLAINTEXT) { json_array_push(&result, json_make_string_const("plaintext"), allocator); }
    if (value & MARKUPKIND_MARKDOWN) { json_array_push(&result, json_make_string_const("markdown"), allocator); }

    return result;
}

/**
 * Completion item tags are extra annotations that tweak the rendering of a
 * completion item.
 *
 * @since 3.15.0
 */
typedef enum {
    /**
     * Render a completion as obsolete, usually using a strike-out.
     */
    COMPLETIONITEMTAGMASK_DEPRECATED = 1 << 0,
} CompletionItemTagMask;

typedef enum {
    COMPLETIONITEMTAG_DEPRECATED = 1,
} CompletionItemTag;

static JSONValue completion_item_tag_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & COMPLETIONITEMTAGMASK_DEPRECATED) { json_array_push(&result, json_make_int(COMPLETIONITEMTAG_DEPRECATED), allocator); }

    return result;
}

/**
 * How whitespace and indentation is handled during completion
 * item insertion.
 *
 * @since 3.16.0
 */
typedef enum {
    /**
     * The insertion or replace strings is taken as it is. If the
     * value is multi line the lines below the cursor will be
     * inserted using the indentation defined in the string value.
     * The client will not apply any kind of adjustments to the
     * string.
     */
    INSERTTEXTMODEMASK_ASIS = 1 << 0,

    /**
     * The editor adjusts leading whitespace of new lines so that
     * they match the indentation up to the cursor of the line for
     * which the item is accepted.
     *
     * Consider a line like this: <2tabs><cursor><3tabs>foo. Accepting a
     * multi line completion item is indented using 2 tabs and all
     * following lines inserted will be indented using 2 tabs as well.
     */
    INSERTTEXTMODEMASK_ADJUSTINDENTATION = 1 << 1,
} InsertTextModeMask;

typedef enum {
    INSERTTEXTMODE_ASIS = 1,
    INSERTTEXTMODE_ADJUSTINDENTATION = 2,
} InsertTextMode;

static JSONValue insert_text_mode_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & INSERTTEXTMODEMASK_ASIS) { json_array_push(&result, json_make_int(INSERTTEXTMODE_ASIS), allocator); }
    if (value & INSERTTEXTMODEMASK_ADJUSTINDENTATION) { json_array_push(&result, json_make_int(INSERTTEXTMODE_ADJUSTINDENTATION), allocator); }

    return result;
}

/**
 * The kind of a completion entry.
 */
typedef enum {
    COMPLETIONITEMKINDMASK_TEXT = 1 << 0,
    COMPLETIONITEMKINDMASK_METHOD = 1 << 1,
    COMPLETIONITEMKINDMASK_FUNCTION = 1 << 2,
    COMPLETIONITEMKINDMASK_CONSTRUCTOR = 1 << 3,
    COMPLETIONITEMKINDMASK_FIELD = 1 << 4,
    COMPLETIONITEMKINDMASK_VARIABLE = 1 << 5,
    COMPLETIONITEMKINDMASK_CLASS = 1 << 6,
    COMPLETIONITEMKINDMASK_INTERFACE = 1 << 7,
    COMPLETIONITEMKINDMASK_MODULE = 1 << 8,
    COMPLETIONITEMKINDMASK_PROPERTY = 1 << 9,
    COMPLETIONITEMKINDMASK_UNIT = 1 << 10,
    COMPLETIONITEMKINDMASK_VALUE = 1 << 11,
    COMPLETIONITEMKINDMASK_ENUM = 1 << 12,
    COMPLETIONITEMKINDMASK_KEYWORD = 1 << 13,
    COMPLETIONITEMKINDMASK_SNIPPET = 1 << 14,
    COMPLETIONITEMKINDMASK_COLOR = 1 << 15,
    COMPLETIONITEMKINDMASK_FILE = 1 << 16,
    COMPLETIONITEMKINDMASK_REFERENCE = 1 << 17,
    COMPLETIONITEMKINDMASK_FOLDER = 1 << 18,
    COMPLETIONITEMKINDMASK_ENUMMEMBER = 1 << 19,
    COMPLETIONITEMKINDMASK_CONSTANT = 1 << 20,
    COMPLETIONITEMKINDMASK_STRUCT = 1 << 21,
    COMPLETIONITEMKINDMASK_EVENT = 1 << 22,
    COMPLETIONITEMKINDMASK_OPERATOR = 1 << 23,
    COMPLETIONITEMKINDMASK_TYPEPARAMETER = 1 << 24,
} CompletionItemKindMask;

typedef enum {
    COMPLETIONITEMKIND_TEXT = 1,
    COMPLETIONITEMKIND_METHOD = 2,
    COMPLETIONITEMKIND_FUNCTION = 3,
    COMPLETIONITEMKIND_CONSTRUCTOR = 4,
    COMPLETIONITEMKIND_FIELD = 5,
    COMPLETIONITEMKIND_VARIABLE = 6,
    COMPLETIONITEMKIND_CLASS = 7,
    COMPLETIONITEMKIND_INTERFACE = 8,
    COMPLETIONITEMKIND_MODULE = 9,
    COMPLETIONITEMKIND_PROPERTY = 10,
    COMPLETIONITEMKIND_UNIT = 11,
    COMPLETIONITEMKIND_VALUE = 12,
    COMPLETIONITEMKIND_ENUM = 13,
    COMPLETIONITEMKIND_KEYWORD = 14,
    COMPLETIONITEMKIND_SNIPPET = 15,
    COMPLETIONITEMKIND_COLOR = 16,
    COMPLETIONITEMKIND_FILE = 17,
    COMPLETIONITEMKIND_REFERENCE = 18,
    COMPLETIONITEMKIND_FOLDER = 19,
    COMPLETIONITEMKIND_ENUMMEMBER = 20,
    COMPLETIONITEMKIND_CONSTANT = 21,
    COMPLETIONITEMKIND_STRUCT = 22,
    COMPLETIONITEMKIND_EVENT = 23,
    COMPLETIONITEMKIND_OPERATOR = 24,
    COMPLETIONITEMKIND_TYPEPARAMETER = 25,
} CompletionItemKind;

static JSONValue completion_item_kind_make_array(long long value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & COMPLETIONITEMKINDMASK_TEXT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_TEXT), allocator); }
    if (value & COMPLETIONITEMKINDMASK_METHOD) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_METHOD), allocator); }
    if (value & COMPLETIONITEMKINDMASK_FUNCTION) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_FUNCTION), allocator); }
    if (value & COMPLETIONITEMKINDMASK_CONSTRUCTOR) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_CONSTRUCTOR), allocator); }
    if (value & COMPLETIONITEMKINDMASK_FIELD) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_FIELD), allocator); }
    if (value & COMPLETIONITEMKINDMASK_VARIABLE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_VARIABLE), allocator); }
    if (value & COMPLETIONITEMKINDMASK_CLASS) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_CLASS), allocator); }
    if (value & COMPLETIONITEMKINDMASK_INTERFACE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_INTERFACE), allocator); }
    if (value & COMPLETIONITEMKINDMASK_MODULE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_MODULE), allocator); }
    if (value & COMPLETIONITEMKINDMASK_PROPERTY) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_PROPERTY), allocator); }
    if (value & COMPLETIONITEMKINDMASK_UNIT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_UNIT), allocator); }
    if (value & COMPLETIONITEMKINDMASK_VALUE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_VALUE), allocator); }
    if (value & COMPLETIONITEMKINDMASK_ENUM) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_ENUM), allocator); }
    if (value & COMPLETIONITEMKINDMASK_KEYWORD) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_KEYWORD), allocator); }
    if (value & COMPLETIONITEMKINDMASK_SNIPPET) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_SNIPPET), allocator); }
    if (value & COMPLETIONITEMKINDMASK_COLOR) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_COLOR), allocator); }
    if (value & COMPLETIONITEMKINDMASK_FILE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_FILE), allocator); }
    if (value & COMPLETIONITEMKINDMASK_REFERENCE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_REFERENCE), allocator); }
    if (value & COMPLETIONITEMKINDMASK_FOLDER) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_FOLDER), allocator); }
    if (value & COMPLETIONITEMKINDMASK_ENUMMEMBER) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_ENUMMEMBER), allocator); }
    if (value & COMPLETIONITEMKINDMASK_CONSTANT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_CONSTANT), allocator); }
    if (value & COMPLETIONITEMKINDMASK_STRUCT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_STRUCT), allocator); }
    if (value & COMPLETIONITEMKINDMASK_EVENT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_EVENT), allocator); }
    if (value & COMPLETIONITEMKINDMASK_OPERATOR) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_OPERATOR), allocator); }
    if (value & COMPLETIONITEMKINDMASK_TYPEPARAMETER) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_TYPEPARAMETER), allocator); }

    return result;
}

/**
 * A set of predefined code action kinds.
 */
typedef enum {
    /**
     * Empty kind.
     */
    CODEACTIONKIND_EMPTY = 1 << 0,

    /**
     * Base kind for quickfix actions: 'quickfix'.
     */
    CODEACTIONKIND_QUICKFIX = 1 << 1,

    /**
     * Base kind for refactoring actions: 'refactor'.
     */
    CODEACTIONKIND_REFACTOR = 1 << 2,

    /**
     * Base kind for refactoring extraction actions: 'refactor.extract'.
     *
     * Example extract actions:
     *
     * - Extract method
     * - Extract function
     * - Extract variable
     * - Extract interface from class
     * - ...
     */
    CODEACTIONKIND_REFACTOREXTRACT = 1 << 3,

    /**
     * Base kind for refactoring inline actions: 'refactor.inline'.
     *
     * Example inline actions:
     *
     * - Inline function
     * - Inline variable
     * - Inline constant
     * - ...
     */
    CODEACTIONKIND_REFACTORINLINE = 1 << 4,

    /**
     * Base kind for refactoring rewrite actions: 'refactor.rewrite'.
     *
     * Example rewrite actions:
     *
     * - Convert JavaScript function to class
     * - Add or remove parameter
     * - Encapsulate field
     * - Make method static
     * - Move method to base class
     * - ...
     */
    CODEACTIONKIND_REFACTORREWRITE = 1 << 5,

    /**
     * Base kind for source actions: `source`.
     *
     * Source code actions apply to the entire file.
     */
    CODEACTIONKIND_SOURCE = 1 << 6,

    /**
     * Base kind for an organize imports source action:
     * `source.organizeImports`.
     */
    CODEACTIONKIND_SOURCEORGANIZEIMPORTS = 1 << 7,

    /**
     * Base kind for a 'fix all' source action: `source.fixAll`.
     *
     * 'Fix all' actions automatically fix errors that have a clear fix that
     * do not require user input. They should not suppress errors or perform
     * unsafe fixes such as generating new types or classes.
     *
     * @since 3.17.0
     */
    CODEACTIONKIND_SOURCEFIXALL = 1 << 8,
} CodeActionKind;

static JSONValue code_action_kind_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & CODEACTIONKIND_EMPTY) { json_array_push(&result, json_make_string_const(""), allocator); }
    if (value & CODEACTIONKIND_QUICKFIX) { json_array_push(&result, json_make_string_const("quickfix"), allocator); }
    if (value & CODEACTIONKIND_REFACTOR) { json_array_push(&result, json_make_string_const("refactor"), allocator); }
    if (value & CODEACTIONKIND_REFACTOREXTRACT) { json_array_push(&result, json_make_string_const("refactor.extract"), allocator); }
    if (value & CODEACTIONKIND_REFACTORINLINE) { json_array_push(&result, json_make_string_const("refactor.inline"), allocator); }
    if (value & CODEACTIONKIND_REFACTORREWRITE) { json_array_push(&result, json_make_string_const("refactor.rewrite"), allocator); }
    if (value & CODEACTIONKIND_SOURCE) { json_array_push(&result, json_make_string_const("source"), allocator); }
    if (value & CODEACTIONKIND_SOURCEORGANIZEIMPORTS) { json_array_push(&result, json_make_string_const("source.organizeImports"), allocator); }
    if (value & CODEACTIONKIND_SOURCEFIXALL) { json_array_push(&result, json_make_string_const("source.fixAll"), allocator); }

    return result;
}

static int code_action_kind_parse(JSONValue* value) {
    if (value == NULL || value->type != JSON_VALUE_ARRAY) {
        return 0;
    }

    int result = 0;
    for (size_t i = 0; i < value->value.array_value->values.length; i++) {
        JSONValue item = json_array_get(value, i);

        if (item.type == JSON_VALUE_STRING) {
            if (strcmp(item.value.string_value, "") == 0) {
                result |= CODEACTIONKIND_EMPTY;
            } else if (strcmp(item.value.string_value, "quickfix") == 0) {
                result |= CODEACTIONKIND_QUICKFIX;
            } else if (strcmp(item.value.string_value, "refactor") == 0) {
                result |= CODEACTIONKIND_REFACTOR;
            } else if (strcmp(item.value.string_value, "refactor.extract") == 0) {
                result |= CODEACTIONKIND_REFACTOREXTRACT;
            } else if (strcmp(item.value.string_value, "refactor.inline") == 0) {
                result |= CODEACTIONKIND_REFACTORINLINE;
            } else if (strcmp(item.value.string_value, "refactor.rewrite") == 0) {
                result |= CODEACTIONKIND_REFACTORREWRITE;
            } else if (strcmp(item.value.string_value, "source") == 0) {
                result |= CODEACTIONKIND_SOURCE;
            } else if (strcmp(item.value.string_value, "source.organizeImports") == 0) {
                result |= CODEACTIONKIND_SOURCEORGANIZEIMPORTS;
            } else if (strcmp(item.value.string_value, "source.fixAll") == 0) {
                result |= CODEACTIONKIND_SOURCEFIXALL;
            }
        }
    }

    return result;
}

/**
 * The diagnostic tags.
 *
 * @since 3.15.0
 */
typedef enum {
    /**
     * Unused or unnecessary code.
     *
     * Clients are allowed to render diagnostics with this tag faded out
     * instead of having an error squiggle.
     */
    DIAGNOSTICTAGMASK_UNNECESSARY = 1 << 0,

    /**
     * Deprecated or obsolete code.
     *
     * Clients are allowed to rendered diagnostics with this tag strike through.
     */
    DIAGNOSTICTAGMASK_DEPRECATED = 1 << 1,
} DiagnosticTagMask;

typedef enum {
    DIAGNOSTICTAG_UNNECESSARY = 1,
    DIAGNOSTICTAG_DEPRECATED = 2,
} DiagnosticTag;

static JSONValue diagnostic_tags_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & DIAGNOSTICTAGMASK_UNNECESSARY) { json_array_push(&result, json_make_int(DIAGNOSTICTAG_UNNECESSARY), allocator); }
    if (value & DIAGNOSTICTAGMASK_DEPRECATED) { json_array_push(&result, json_make_int(DIAGNOSTICTAG_DEPRECATED), allocator); }

    return result;
}

static int diagnostic_tags_parse(JSONValue* value) {
    int result = 0;
    
    if (value == NULL || value->type != JSON_VALUE_ARRAY) {
        return result;
    }

    for (size_t i = 0; i < json_array_length(value); i++) {
        JSONValue item = json_array_get(value, i);
        if (item.type == JSON_VALUE_INT) {
            switch (item.value.int_value) {
                case DIAGNOSTICTAG_UNNECESSARY: result |= DIAGNOSTICTAGMASK_UNNECESSARY; break;
                case DIAGNOSTICTAG_DEPRECATED: result |= DIAGNOSTICTAGMASK_DEPRECATED; break;
                default: break;
            }
        }
    }

    return result;
}

/**
 * A set of predefined range kinds.
 */
typedef enum {
    /**
     * Folding range for a comment
     */
    FOLDINGRANGEKIND_COMMENT = 1 << 0,

    /**
     * Folding range for a imports or includes
     */
    FOLDINGRANGEKIND_IMPORTS = 1 << 1,

    /**
     * Folding range for a region (e.g. `#region`)
     */
    FOLDINGRANGEKIND_REGION = 1 << 2,
} FoldingRangeKind;

static JSONValue folding_range_kind_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & FOLDINGRANGEKIND_COMMENT) { json_array_push(&result, json_make_string_const("comment"), allocator); }
    if (value & FOLDINGRANGEKIND_IMPORTS) { json_array_push(&result, json_make_string_const("imports"), allocator); }
    if (value & FOLDINGRANGEKIND_REGION) { json_array_push(&result, json_make_string_const("region"), allocator); }

    return result;
}

typedef enum {
    TOKENFORMAT_RELATIVE = 1 << 0,
} TokenFormat;

static JSONValue token_format_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & TOKENFORMAT_RELATIVE) { json_array_push(&result, json_make_string_const("relative"), allocator); }

    return result;
}

/**
 * A set of predefined position encoding kinds.
 *
 * @since 3.17.0
 */
typedef enum {
    /**
     * Character offsets count UTF-8 code units (e.g bytes).
     */
    POSITIONENCODINGKIND_UTF8 = 1 << 0,

    /**
     * Character offsets count UTF-16 code units.
     *
     * This is the default and must always be supported
     * by servers
     */
    POSITIONENCODINGKIND_UTF16 = 1 << 1,

    /**
     * Character offsets count UTF-32 code units.
     *
     * Implementation note: these are the same as Unicode code points,
     * so this `PositionEncodingKind` may also be used for an
     * encoding-agnostic representation of character offsets.
     */
    POSITIONENCODINGKIND_UTF32 = 1 << 2,
} PositionEncodingKind;

static JSONValue position_encoding_kind_make_array(int value, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_array(allocator);

    if (value & POSITIONENCODINGKIND_UTF8) { json_array_push(&result, json_make_string_const("utf-8"), allocator); }
    if (value & POSITIONENCODINGKIND_UTF16) { json_array_push(&result, json_make_string_const("utf-16"), allocator); }
    if (value & POSITIONENCODINGKIND_UTF32) { json_array_push(&result, json_make_string_const("utf-32"), allocator); }

    return result;
}

static PositionEncodingKind position_encoding_kind_parse(char* value) {
    if (value == NULL) {
        return POSITIONENCODINGKIND_UTF16;
    }

    if (strcmp(value, "utf-8") == 0) { return POSITIONENCODINGKIND_UTF8; }
    if (strcmp(value, "utf-16") == 0) { return POSITIONENCODINGKIND_UTF16; }
    if (strcmp(value, "utf-32") == 0) { return POSITIONENCODINGKIND_UTF32; }

    return POSITIONENCODINGKIND_UTF16;
}

/**
 * Capabilities specific to `WorkspaceEdit`s
 */
typedef struct WorkspaceEditClientCapabilities {
    /**
     * The client supports versioned document changes in `WorkspaceEdit`s
     */
    lstalk_bool document_changes;

    /**
     * The resource operations the client supports. Clients should at least
     * support 'create', 'rename' and 'delete' files and folders.
     *
     * @since 3.13.0
     */
    int resource_operations;

    /**
     * The failure handling strategy of a client if applying the workspace edit
     * fails.
     *
     * @since 3.13.0
     */
    int failure_handling;

    /**
     * Whether the client normalizes line endings to the client specific
     * setting.
     * If set to `true` the client will normalize line ending characters
     * in a workspace edit to the client specific new line character(s).
     *
     * @since 3.16.0
     */
    lstalk_bool normalizes_line_endings;

    /**
     * Whether the client in general supports change annotations on text edits,
     * create file, rename file and delete file changes.
     *
     * @since 3.16.0
     * 
     * changeAnnotationSupport?: {}
     * 
     * Whether the client groups edits with equal labels into tree nodes,
     * for instance all edits labelled with "Changes in Strings" would
     * be a tree node.
     */
    lstalk_bool groups_on_label;
} WorkspaceEditClientCapabilities;

static JSONValue workspace_edit_client_capabilities_make(WorkspaceEditClientCapabilities* workspace_edit, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    json_object_const_key_set(&result, "documentChanges", json_make_boolean(workspace_edit->document_changes), allocator);
    json_object_const_key_set(&result, "resourceOperations", resource_operation_kind_make_array(workspace_edit->resource_operations, allocator), allocator);
    json_object_const_key_set(&result, "failureHandling", failure_handling_kind_make_array(workspace_edit->failure_handling, allocator), allocator);
    json_object_const_key_set(&result, "normalizesLineEndings", json_make_boolean(workspace_edit->normalizes_line_endings), allocator);
    JSONValue change_annotation_support = json_make_object(allocator);
    json_object_const_key_set(&change_annotation_support, "groupsOnLabel", json_make_boolean(workspace_edit->groups_on_label), allocator);
    json_object_const_key_set(&result, "changeAnnotationSupport", change_annotation_support, allocator);
    return result;
}

typedef struct DynamicRegistration {
    lstalk_bool value;
} DynamicRegistration;

static void dynamic_registration_set(JSONValue* root, DynamicRegistration* dynamic_registration, LSTalk_MemoryAllocator* allocator) {
    if (root == NULL || root->type != JSON_VALUE_OBJECT || dynamic_registration == NULL) {
        return;
    }

    json_object_const_key_set(root, "dynamicRegistration", json_make_boolean(dynamic_registration->value), allocator);
}

static JSONValue dynamic_registration_make(DynamicRegistration* dynamic_registration, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    json_object_const_key_set(&result, "dynamicRegistration", json_make_boolean(dynamic_registration->value), allocator);
    return result;
}

/**
 * Capabilities specific to the `workspace/didChangeWatchedFiles`
 * notification.
 */
typedef struct DidChangeWatchedFilesClientCapabilities {
    /**
     * Did change watched files notification supports dynamic registration.
     * Please note that the current protocol doesn't support static
     * configuration for file changes from the server side.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Whether the client has support for relative patterns
     * or not.
     *
     * @since 3.17.0
     */
    lstalk_bool relative_pattern_support;
} DidChangeWatchedFilesClientCapabilities;

typedef struct WorkspaceSymbolClientCapabilities {
    /**
     * Symbol request supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Specific capabilities for the `SymbolKind` in the `workspace/symbol`
     * request.
     *
     * symbolKind:
     * 
     * The symbol kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     *
     * If this property is not present the client only supports
     * the symbol kinds from `File` to `Array` as defined in
     * the initial version of the protocol.
     */
    long long symbol_kind_value_set;

    /**
     * The client supports tags on `SymbolInformation` and `WorkspaceSymbol`.
     * Clients supporting tags have to handle unknown tags gracefully.
     *
     * @since 3.16.0
     *
     * tagSupport:
     * 
     * The tags supported by the client.
     */
    int tag_support_value_set;

    /**
     * The client support partial workspace symbols. The client will send the
     * request `workspaceSymbol/resolve` to the server to resolve additional
     * properties.
     *
     * @since 3.17.0 - proposedState
     *
     * resolveSupport:
     * 
     * The properties that a client can resolve lazily. Usually
     * `location.range`
     */
    char** resolve_support_properties;
    int resolve_support_count;
} WorkspaceSymbolClientCapabilities;

static JSONValue workspace_symbol_client_capabilities_make(WorkspaceSymbolClientCapabilities* symbol, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);

    dynamic_registration_set(&result, &symbol->dynamic_registration, allocator);
    JSONValue symbol_kind = json_make_object(allocator);
    json_object_const_key_set(&symbol_kind, "valueSet", symbol_kind_make_array(symbol->symbol_kind_value_set, allocator), allocator);
    json_object_const_key_set(&result, "symbolKind", symbol_kind, allocator);
    JSONValue tag_support = json_make_object(allocator);
    json_object_const_key_set(&tag_support, "valueSet", symbol_tags_make_array(symbol->tag_support_value_set, allocator), allocator);
    json_object_const_key_set(&result, "tagSupport", tag_support, allocator);
    JSONValue resolve_support = json_make_object(allocator);
    json_object_const_key_set(&resolve_support, "properties", json_make_string_array(symbol->resolve_support_properties, symbol->resolve_support_count, allocator), allocator);
    json_object_const_key_set(&result, "resolveSupport", resolve_support, allocator);

    return result;
}

typedef struct RefreshSupport {
    /**
     * Note that this event is global and will force the client to refresh all
     * values. It should be used with absolute care and is useful for situation
     * where a server for example detect a project wide change that requires
     * such a calculation.
     */
    lstalk_bool value;
} RefreshSupport;

static JSONValue refresh_support_make(RefreshSupport* refresh_support, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    json_object_const_key_set(&result, "refreshSupport", json_make_boolean(refresh_support->value), allocator);
    return result;
}

/**
 * The client has support for file requests/notifications.
 *
 * @since 3.16.0
 */
typedef struct FileOperations {
    /**
     * Whether the client supports dynamic registration for file
     * requests/notifications.
     */
    DynamicRegistration dynamic_registration;

    /**
     * The client has support for sending didCreateFiles notifications.
     */
    lstalk_bool did_create;

    /**
     * The client has support for sending willCreateFiles requests.
     */
    lstalk_bool will_create;

    /**
     * The client has support for sending didRenameFiles notifications.
     */
    lstalk_bool did_rename;

    /**
     * The client has support for sending willRenameFiles requests.
     */
    lstalk_bool will_rename;

    /**
     * The client has support for sending didDeleteFiles notifications.
     */
    lstalk_bool did_delete;

    /**
     * The client has support for sending willDeleteFiles requests.
     */
    lstalk_bool will_delete;
} FileOperations;

static JSONValue file_operations_make(FileOperations* file_ops, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &file_ops->dynamic_registration, allocator);
    json_object_const_key_set(&result, "didCreate", json_make_boolean(file_ops->did_create), allocator);
    json_object_const_key_set(&result, "willCreate", json_make_boolean(file_ops->will_create), allocator);
    json_object_const_key_set(&result, "didRename", json_make_boolean(file_ops->did_rename), allocator);
    json_object_const_key_set(&result, "willRename", json_make_boolean(file_ops->will_rename), allocator);
    json_object_const_key_set(&result, "didDelete", json_make_boolean(file_ops->did_delete), allocator);
    json_object_const_key_set(&result, "willDelete", json_make_boolean(file_ops->will_delete), allocator);
    return result;
}

/**
 * Workspace specific client capabilities.
 */
typedef struct Workspace {
    /**
     * The client supports applying batch edits
     * to the workspace by supporting the request
     * 'workspace/applyEdit'
     */
    lstalk_bool apply_edit;

    /**
     * Capabilities specific to `WorkspaceEdit`s
     */
    WorkspaceEditClientCapabilities workspace_edit;

    /**
     * Capabilities specific to the `workspace/didChangeConfiguration`
     * notification.
     */
    DynamicRegistration did_change_configuration;

    /**
     * Capabilities specific to the `workspace/didChangeWatchedFiles`
     * notification.
     */
    DidChangeWatchedFilesClientCapabilities did_change_watched_files;

    /**
     * Capabilities specific to the `workspace/symbol` request.
     */
    WorkspaceSymbolClientCapabilities symbol;

    /**
     * Capabilities specific to the `workspace/executeCommand` request.
     */
    DynamicRegistration execute_command;

    /**
     * The client has support for workspace folders.
     *
     * @since 3.6.0
     */
    lstalk_bool workspace_folders;

    /**
     * The client supports `workspace/configuration` requests.
     *
     * @since 3.6.0
     */
    lstalk_bool configuration;

    /**
     * Capabilities specific to the semantic token requests scoped to the
     * workspace.
     *
     * @since 3.16.0
     */
    RefreshSupport semantic_tokens;

    /**
     * Capabilities specific to the code lens requests scoped to the
     * workspace.
     *
     * @since 3.16.0
     */
    RefreshSupport code_lens;

    /**
     * The client has support for file requests/notifications.
     *
     * @since 3.16.0
     */
    FileOperations file_operations;

    /**
     * Client workspace capabilities specific to inline values.
     *
     * @since 3.17.0
     */
    RefreshSupport inline_value;

    /**
     * Client workspace capabilities specific to inlay hints.
     *
     * @since 3.17.0
     */
    RefreshSupport inlay_hint;

    /**
     * Client workspace capabilities specific to diagnostics.
     *
     * @since 3.17.0.
     */
    RefreshSupport diagnostics;
} Workspace;

static JSONValue workspace_make(Workspace* workspace, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);

    JSONValue did_change_watched_files = json_make_object(allocator);
    dynamic_registration_set(&did_change_watched_files, &workspace->did_change_watched_files.dynamic_registration, allocator);
    json_object_const_key_set(&did_change_watched_files, "relativePatternSupport", json_make_boolean(workspace->did_change_watched_files.relative_pattern_support), allocator);

    json_object_const_key_set(&result, "applyEdit", json_make_boolean(workspace->apply_edit), allocator);
    json_object_const_key_set(&result, "workspaceEdit", workspace_edit_client_capabilities_make(&workspace->workspace_edit, allocator), allocator);
    json_object_const_key_set(&result, "didChangeConfiguration", dynamic_registration_make(&workspace->did_change_configuration, allocator), allocator);
    json_object_const_key_set(&result, "didChangeWatchedFiles", did_change_watched_files, allocator);
    json_object_const_key_set(&result, "symbol", workspace_symbol_client_capabilities_make(&workspace->symbol, allocator), allocator);
    json_object_const_key_set(&result, "executeCommand", dynamic_registration_make(&workspace->execute_command, allocator), allocator);
    json_object_const_key_set(&result, "workspaceFolders", json_make_boolean(workspace->workspace_folders), allocator);
    json_object_const_key_set(&result, "configuration", json_make_boolean(workspace->configuration), allocator);
    json_object_const_key_set(&result, "semanticTokens", refresh_support_make(&workspace->semantic_tokens, allocator), allocator);
    json_object_const_key_set(&result, "codeLens", refresh_support_make(&workspace->code_lens, allocator), allocator);
    json_object_const_key_set(&result, "fileOperations", file_operations_make(&workspace->file_operations, allocator), allocator);
    json_object_const_key_set(&result, "inlineValue", refresh_support_make(&workspace->inline_value, allocator), allocator);
    json_object_const_key_set(&result, "inlayHint", refresh_support_make(&workspace->inlay_hint, allocator), allocator);
    json_object_const_key_set(&result, "diagnostics", refresh_support_make(&workspace->diagnostics, allocator), allocator);

    return result;
}

typedef struct TextDocumentSyncClientCapabilities {
    /**
     * Whether text document synchronization supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * The client supports sending will save notifications.
     */
    lstalk_bool will_save;

    /**
     * The client supports sending a will save request and
     * waits for a response providing text edits which will
     * be applied to the document before it is saved.
     */
    lstalk_bool will_save_wait_until;

    /**
     * The client supports did save notifications.
     */
    lstalk_bool did_save;
} TextDocumentSyncClientCapabilities;

static JSONValue text_document_sync_client_capabilities_make(TextDocumentSyncClientCapabilities* sync, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &sync->dynamic_registration, allocator);
    json_object_const_key_set(&result, "willSave", json_make_boolean(sync->will_save), allocator);
    json_object_const_key_set(&result, "willSaveWaitUntil", json_make_boolean(sync->will_save_wait_until), allocator);
    json_object_const_key_set(&result, "didSave", json_make_boolean(sync->did_save), allocator);
    return result;
}

/**
 * The client supports the following `CompletionItem` specific
 * capabilities.
 */
typedef struct CompletionItem {
    /**
     * Client supports snippets as insert text.
     *
     * A snippet can define tab stops and placeholders with `$1`, `$2`
     * and `${3:foo}`. `$0` defines the final tab stop, it defaults to
     * the end of the snippet. Placeholders with equal identifiers are
     * linked, that is typing in one will update others too.
     */
    lstalk_bool snippet_support;

    /**
     * Client supports commit characters on a completion item.
     */
    lstalk_bool commit_characters_support;

    /**
     * Client supports the follow content formats for the documentation
     * property. The order describes the preferred format of the client.
     */
    int documentation_format;

    /**
     * Client supports the deprecated property on a completion item.
     */
    lstalk_bool deprecated_support;

    /**
     * Client supports the preselect property on a completion item.
     */
    lstalk_bool preselect_support;

    /**
     * Client supports the tag property on a completion item. Clients
     * supporting tags have to handle unknown tags gracefully. Clients
     * especially need to preserve unknown tags when sending a completion
     * item back to the server in a resolve call.
     *
     * @since 3.15.0
     *
     * tagSupport:
     *
     * The tags supported by the client.
     */
    int tag_support_value_set;

    /**
     * Client supports insert replace edit to control different behavior if
     * a completion item is inserted in the text or should replace text.
     *
     * @since 3.16.0
     */
    lstalk_bool insert_replace_support;

    /**
     * Indicates which properties a client can resolve lazily on a
     * completion item. Before version 3.16.0 only the predefined properties
     * `documentation` and `detail` could be resolved lazily.
     *
     * @since 3.16.0
     *
     * resolveSupport
     * 
     * The properties that a client can resolve lazily.
     */
    char** resolve_support_properties;
    int resolve_support_count;

    /**
     * The client supports the `insertTextMode` property on
     * a completion item to override the whitespace handling mode
     * as defined by the client (see `insertTextMode`).
     *
     * @since 3.16.0
     *
     * insertTextModeSupport
     * 
     */
    int insert_text_mode_support_value_set;

    /**
     * The client has support for completion item label
     * details (see also `CompletionItemLabelDetails`).
     *
     * @since 3.17.0
     */
    lstalk_bool label_details_support;
} CompletionItem;

static JSONValue completion_item_make(CompletionItem* completion_item, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);

    json_object_const_key_set(&result, "snippetSupport", json_make_boolean(completion_item->snippet_support), allocator);
    json_object_const_key_set(&result, "commitCharactersSupport", json_make_boolean(completion_item->commit_characters_support), allocator);
    json_object_const_key_set(&result, "documentationFormat", markup_kind_make_array(completion_item->documentation_format, allocator), allocator);
    json_object_const_key_set(&result, "deprecatedSupport", json_make_boolean(completion_item->deprecated_support), allocator);
    json_object_const_key_set(&result, "preselectSupport", json_make_boolean(completion_item->preselect_support), allocator);
    JSONValue item_tag_support = json_make_object(allocator);
    json_object_const_key_set(&item_tag_support, "valueSet", completion_item_tag_make_array(completion_item->tag_support_value_set, allocator), allocator);
    json_object_const_key_set(&result, "tagSupport", item_tag_support, allocator);
    json_object_const_key_set(&result, "insertReplaceSupport", json_make_boolean(completion_item->insert_replace_support), allocator);
    JSONValue item_resolve_properties = json_make_object(allocator);
    json_object_const_key_set(&item_resolve_properties, "properties",
        json_make_string_array(completion_item->resolve_support_properties, completion_item->resolve_support_count, allocator), allocator);
    json_object_const_key_set(&result, "resolveSupport", item_resolve_properties, allocator);
    JSONValue insert_text_mode = json_make_object(allocator);
    json_object_const_key_set(&insert_text_mode, "valueSet", insert_text_mode_make_array(completion_item->insert_text_mode_support_value_set, allocator), allocator);
    json_object_const_key_set(&result, "insertTextModeSupport", insert_text_mode, allocator);
    json_object_const_key_set(&result, "labelDetailsSupport", json_make_boolean(completion_item->label_details_support), allocator);

    return result;
}

/**
 * Capabilities specific to the `textDocument/completion` request.
 */
typedef struct CompletionClientCapabilities {
    /**
     * Whether completion supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * The client supports the following `CompletionItem` specific
     * capabilities.
     */
    CompletionItem completion_item;

    /**
     * completionItemKind
     * 
     * The completion item kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     *
     * If this property is not present the client only supports
     * the completion items kinds from `Text` to `Reference` as defined in
     * the initial version of the protocol.
     */
    long long completion_item_kind_value_set;

    /**
     * The client supports to send additional context information for a
     * `textDocument/completion` request.
     */
    lstalk_bool context_support;

    /**
     * The client's default when the completion item doesn't provide a
     * `insertTextMode` property.
     *
     * @since 3.17.0
     */
    int insert_text_mode;

    /**
     * The client supports the following `CompletionList` specific
     * capabilities.
     *
     * @since 3.17.0
     *
     * completionList
     * 
     * The client supports the following itemDefaults on
     * a completion list.
     *
     * The value lists the supported property names of the
     * `CompletionList.itemDefaults` object. If omitted
     * no properties are supported.
     *
     * @since 3.17.0
     */
    char** completion_list_item_defaults;
    int completion_list_item_defaults_count;
} CompletionClientCapabilities;

static JSONValue completion_client_capabilities_make(CompletionClientCapabilities* completion, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);

    dynamic_registration_set(&result, &completion->dynamic_registration, allocator);
    json_object_const_key_set(&result, "completionItem", completion_item_make(&completion->completion_item, allocator), allocator);
    JSONValue item_kind = json_make_object(allocator);
    json_object_const_key_set(&item_kind, "valueSet", completion_item_kind_make_array(completion->completion_item_kind_value_set, allocator), allocator);
    json_object_const_key_set(&result, "completionItemKind", item_kind, allocator);
    json_object_const_key_set(&result, "contextSupport", json_make_boolean(completion->context_support), allocator);
    json_object_const_key_set(&result, "insertTextMode", json_make_int(completion->insert_text_mode), allocator);
    JSONValue item_defaults = json_make_object(allocator);
    json_object_const_key_set(&item_defaults, "itemDefaults",
        json_make_string_array(completion->completion_list_item_defaults, completion->completion_list_item_defaults_count, allocator), allocator);
    json_object_const_key_set(&result, "completionList", item_defaults, allocator);

    return result;
}

/**
 * Capabilities specific to the `textDocument/hover` request.
 */
typedef struct HoverClientCapabilities {
    /**
     * Whether hover supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Client supports the follow content formats if the content
     * property refers to a `literal of type MarkupContent`.
     * The order describes the preferred format of the client.
     */
    int content_format;
} HoverClientCapabilities;

/**
 * The client supports the following `SignatureInformation`
 * specific properties.
 */
typedef struct SignatureInformation {
    /**
     * Client supports the follow content formats for the documentation
     * property. The order describes the preferred format of the client.
     */
    int documentation_format;

    /**
     * Client capabilities specific to parameter information.
     *
     * parameterInformation:
     * 
     * The client supports processing label offsets instead of a
     * simple label string.
     *
     * @since 3.14.0
     */
    lstalk_bool label_offset_support;

    /**
     * The client supports the `activeParameter` property on
     * `SignatureInformation` literal.
     *
     * @since 3.16.0
     */
    lstalk_bool active_parameter_support;
} SignatureInformation;

/**
 * Capabilities specific to the `textDocument/signatureHelp` request.
 */
typedef struct SignatureHelpClientCapabilities {
    /**
     * Whether signature help supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * The client supports the following `SignatureInformation`
     * specific properties.
     */
    SignatureInformation signature_information;

    /**
     * The client supports to send additional context information for a
     * `textDocument/signatureHelp` request. A client that opts into
     * contextSupport will also support the `retriggerCharacters` on
     * `SignatureHelpOptions`.
     *
     * @since 3.15.0
     */
    lstalk_bool context_support;
} SignatureHelpClientCapabilities;

static JSONValue signature_help_client_capabilities_make(SignatureHelpClientCapabilities* signature_help, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &signature_help->dynamic_registration, allocator);
    JSONValue info = json_make_object(allocator);
    json_object_const_key_set(&info, "documentationFormat", markup_kind_make_array(signature_help->signature_information.documentation_format, allocator), allocator);
    JSONValue parameter_info = json_make_object(allocator);
    json_object_const_key_set(&parameter_info, "labelOffsetSupport", json_make_boolean(signature_help->signature_information.label_offset_support), allocator);
    json_object_const_key_set(&info, "parameterInformation", parameter_info, allocator);
    json_object_const_key_set(&info, "activeParameterSupport", json_make_boolean(signature_help->signature_information.active_parameter_support), allocator);
    json_object_const_key_set(&result, "signatureInformation", info, allocator);
    json_object_const_key_set(&result, "contextSupport", json_make_boolean(signature_help->context_support), allocator);
    return result;
}

typedef struct DynamicRegistrationLink {
    DynamicRegistration dynamic_registration;
    lstalk_bool link_support;
} DynamicRegistrationLink;

static JSONValue dynamic_registration_link_make(DynamicRegistrationLink* dynamic_registration_link, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &dynamic_registration_link->dynamic_registration, allocator);
    json_object_const_key_set(&result, "linkSupport", json_make_boolean(dynamic_registration_link->link_support), allocator);
    return result;
}

/**
 * Capabilities specific to the `textDocument/documentSymbol` request.
 */
typedef struct DocumentSymbolClientCapabilities {
    /**
     * Whether document symbol supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Specific capabilities for the `SymbolKind` in the
     * `textDocument/documentSymbol` request.
     *
     * symbolKind:
     * 
     * The symbol kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     *
     * If this property is not present the client only supports
     * the symbol kinds from `File` to `Array` as defined in
     * the initial version of the protocol.
     */
    int symbol_kind_value_set;

    /**
     * The client supports hierarchical document symbols.
     */
    lstalk_bool hierarchical_document_symbol_support;

    /**
     * The client supports tags on `SymbolInformation`. Tags are supported on
     * `DocumentSymbol` if `hierarchicalDocumentSymbolSupport` is set to true.
     * Clients supporting tags have to handle unknown tags gracefully.
     *
     * @since 3.16.0
     *
     * tagSupport:
     * 
     * The tags supported by the client.
     */
    int tag_support_value_set;

    /**
     * The client supports an additional label presented in the UI when
     * registering a document symbol provider.
     *
     * @since 3.16.0
     */
    lstalk_bool label_support;
} DocumentSymbolClientCapabilities;

static JSONValue document_symbol_client_capabilities_make(DocumentSymbolClientCapabilities* symbol, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &symbol->dynamic_registration, allocator);
    JSONValue symbol_kind = json_make_object(allocator);
    json_object_const_key_set(&symbol_kind, "valueSet", symbol_kind_make_array(symbol->symbol_kind_value_set, allocator), allocator);
    json_object_const_key_set(&result, "symbolKind", symbol_kind, allocator);
    json_object_const_key_set(&result, "hierarchicalDocumentSymbolSupport", json_make_boolean(symbol->hierarchical_document_symbol_support), allocator);
    JSONValue tag_support = json_make_object(allocator);
    json_object_const_key_set(&tag_support, "valueSet", symbol_tags_make_array(symbol->tag_support_value_set, allocator), allocator);
    json_object_const_key_set(&result, "tagSupport", tag_support, allocator);
    json_object_const_key_set(&result, "labelSupport", json_make_boolean(symbol->label_support), allocator);
    return result;
}

/**
 * Capabilities specific to the `textDocument/codeAction` request.
 */
typedef struct CodeActionClientCapabilities {
    /**
     * Whether code action supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * The client supports code action literals as a valid
     * response of the `textDocument/codeAction` request.
     *
     * @since 3.8.0
     *
     * codeActionLiteralSupport:
     * 
     * The code action kind is supported with the following value
     * set.
     *
     * codeActionKind:
     * 
     * The code action kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     */
    int code_action_value_set;

    /**
     * Whether code action supports the `isPreferred` property.
     *
     * @since 3.15.0
     */
    lstalk_bool is_preferred_support;

    /**
     * Whether code action supports the `disabled` property.
     *
     * @since 3.16.0
     */
    lstalk_bool disabled_support;

    /**
     * Whether code action supports the `data` property which is
     * preserved between a `textDocument/codeAction` and a
     * `codeAction/resolve` request.
     *
     * @since 3.16.0
     */
    lstalk_bool data_support;

    /**
     * Whether the client supports resolving additional code action
     * properties via a separate `codeAction/resolve` request.
     *
     * @since 3.16.0
     *
     * resolveSupport:
     * 
     * The properties that a client can resolve lazily.
     */
    char** resolve_support_properties;
    int resolve_support_count;

    /**
     * Whether the client honors the change annotations in
     * text edits and resource operations returned via the
     * `CodeAction#edit` property by for example presenting
     * the workspace edit in the user interface and asking
     * for confirmation.
     *
     * @since 3.16.0
     */
    lstalk_bool honors_change_annotations;
} CodeActionClientCapabilities;

static JSONValue code_action_client_capabilities_make(CodeActionClientCapabilities* code_action, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &code_action->dynamic_registration, allocator);
    JSONValue kind = json_make_object(allocator);
    json_object_const_key_set(&kind, "valueSet", code_action_kind_make_array(code_action->code_action_value_set, allocator), allocator);
    JSONValue literal_support = json_make_object(allocator);
    json_object_const_key_set(&literal_support, "codeActionKind", kind, allocator);
    json_object_const_key_set(&result, "codeActionLiteralSupport", literal_support, allocator);
    json_object_const_key_set(&result, "isPreferredSupport", json_make_boolean(code_action->is_preferred_support), allocator);
    json_object_const_key_set(&result, "disabledSupport", json_make_boolean(code_action->disabled_support), allocator);
    json_object_const_key_set(&result, "dataSupport", json_make_boolean(code_action->data_support), allocator);
    JSONValue resolve_support = json_make_object(allocator);
    json_object_const_key_set(&resolve_support, "properties",
        json_make_string_array(code_action->resolve_support_properties, code_action->resolve_support_count, allocator), allocator);
    json_object_const_key_set(&result, "resolveSupport", resolve_support, allocator);
    json_object_const_key_set(&result, "honorsChangeAnnotations", json_make_boolean(code_action->honors_change_annotations), allocator);
    return result;
}

/**
 * Capabilities specific to the `textDocument/documentLink` request.
 */
typedef struct DocumentLinkClientCapabilities {
    /**
     * Whether document link supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Whether the client supports the `tooltip` property on `DocumentLink`.
     *
     * @since 3.15.0
     */
    lstalk_bool tooltip_support;
} DocumentLinkClientCapabilities;

typedef enum {
    /**
     * The client's default behavior is to select the identifier
     * according to the language's syntax rule.
     */
    PREPARESUPPORTDEFAULTBEHAVIOR_IDENTIFIER = 1,
} PrepareSupportDefaultBehavior;

/**
 * Capabilities specific to the `textDocument/rename` request.
 */
typedef struct RenameClientCapabilities {
    /**
     * Whether rename supports dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Client supports testing for validity of rename operations
     * before execution.
     *
     * @since version 3.12.0
     */
    lstalk_bool prepare_support;

    /**
     * Client supports the default behavior result
     * (`{ defaultBehavior: boolean }`).
     *
     * The value indicates the default behavior used by the
     * client.
     *
     * @since version 3.16.0
     */
    PrepareSupportDefaultBehavior prepare_support_default_behavior;

    /**
     * Whether the client honors the change annotations in
     * text edits and resource operations returned via the
     * rename request's workspace edit by for example presenting
     * the workspace edit in the user interface and asking
     * for confirmation.
     *
     * @since 3.16.0
     */
    lstalk_bool honors_change_annotations;
} RenameClientCapabilities;

static JSONValue rename_client_capabilities_make(RenameClientCapabilities* rename, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &rename->dynamic_registration, allocator);
    json_object_const_key_set(&result, "prepareSupport", json_make_boolean(rename->prepare_support), allocator);
    json_object_const_key_set(&result, "prepareSupportDefaultBehavior", json_make_int(rename->prepare_support_default_behavior), allocator);
    json_object_const_key_set(&result, "honorsChangeAnnotations", json_make_boolean(rename->honors_change_annotations), allocator);
    return result;
}

/**
 * Capabilities specific to the `textDocument/publishDiagnostics`
 * notification.
 */
typedef struct PublishDiagnosticsClientCapabilities {
    /**
     * Whether the clients accepts diagnostics with related information.
     */
    lstalk_bool related_information;

    /**
     * Client supports the tag property to provide meta data about a diagnostic.
     * Clients supporting tags have to handle unknown tags gracefully.
     *
     * @since 3.15.0
     *
     * tagSupport:
     * 
     * The tags supported by the client.
     */
    int value_set;

    /**
     * Whether the client interprets the version property of the
     * `textDocument/publishDiagnostics` notification's parameter.
     *
     * @since 3.15.0
     */
    lstalk_bool version_support;

    /**
     * Client supports a codeDescription property
     *
     * @since 3.16.0
     */
    lstalk_bool code_description_support;

    /**
     * Whether code action supports the `data` property which is
     * preserved between a `textDocument/publishDiagnostics` and
     * `textDocument/codeAction` request.
     *
     * @since 3.16.0
     */
    lstalk_bool data_support;
} PublishDiagnosticsClientCapabilities;

static JSONValue publish_diagnostics_client_capabilities_make(PublishDiagnosticsClientCapabilities* publish, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    json_object_const_key_set(&result, "relatedInformation", json_make_boolean(publish->related_information), allocator);
    JSONValue tag_support = json_make_object(allocator);
    json_object_const_key_set(&tag_support, "valueSet", diagnostic_tags_make_array(publish->value_set, allocator), allocator);
    json_object_const_key_set(&result, "tagSupport", tag_support, allocator);
    json_object_const_key_set(&result, "versionSupport", json_make_boolean(publish->version_support), allocator);
    json_object_const_key_set(&result, "codeDescriptionSupport", json_make_boolean(publish->code_description_support), allocator);
    json_object_const_key_set(&result, "dataSupport", json_make_boolean(publish->data_support), allocator);
    return result;
}

/**
 * Capabilities specific to the `textDocument/foldingRange` request.
 *
 * @since 3.10.0
 */
typedef struct FoldingRangeClientCapabilities {
    /**
     * Whether implementation supports dynamic registration for folding range
     * providers. If this is set to `true` the client supports the new
     * `FoldingRangeRegistrationOptions` return value for the corresponding
     * server capability as well.
     */
    DynamicRegistration dynamic_registration;

    /**
     * The maximum number of folding ranges that the client prefers to receive
     * per document. The value serves as a hint, servers are free to follow the
     * limit.
     */
    unsigned int range_limit;

    /**
     * If set, the client signals that it only supports folding complete lines.
     * If set, client will ignore specified `startCharacter` and `endCharacter`
     * properties in a FoldingRange.
     */
    lstalk_bool line_folding_only;

    /**
     * Specific options for the folding range kind.
     *
     * @since 3.17.0
     *
     * foldingRangeKind:
     * 
     * The folding range kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     */
    int value_set;

    /**
     * Specific options for the folding range.
     * @since 3.17.0
     *
     * foldingRange:
     * 
    * If set, the client signals that it supports setting collapsedText on
    * folding ranges to display custom labels instead of the default text.
    *
    * @since 3.17.0
    */
    lstalk_bool collapsed_text;
} FoldingRangeClientCapabilities;

static JSONValue folding_range_client_capabilities_make(FoldingRangeClientCapabilities* folding_range, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &folding_range->dynamic_registration, allocator);
    json_object_const_key_set(&result, "rangeLimit", json_make_int(folding_range->range_limit), allocator);
    json_object_const_key_set(&result, "lineFoldingOnly", json_make_boolean(folding_range->line_folding_only), allocator);
    JSONValue kind = json_make_object(allocator);
    json_object_const_key_set(&kind, "valueSet", folding_range_kind_make_array(folding_range->value_set, allocator), allocator);
    json_object_const_key_set(&result, "foldingRangeKind", kind, allocator);
    JSONValue range = json_make_object(allocator);
    json_object_const_key_set(&range, "collapsedText", json_make_boolean(folding_range->collapsed_text), allocator);
    json_object_const_key_set(&result, "foldingRange", range, allocator);
    return result;
}

/**
 * Capabilities specific to the various semantic token requests.
 *
 * @since 3.16.0
 */
typedef struct SemanticTokensClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new `(TextDocumentRegistrationOptions &
     * StaticRegistrationOptions)` return value for the corresponding server
     * capability as well.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Which requests the client supports and might send to the server
     * depending on the server's capability. Please note that clients might not
     * show semantic tokens or degrade some of the user experience if a range
     * or full request is advertised by the client but not provided by the
     * server. If for example the client capability `requests.full` and
     * `request.range` are both set to true but the server only provides a
     * range provider the client might not render a minimap correctly or might
     * even decide to not show any semantic tokens at all.
     *
     * requests:
     *
     * The client will send the `textDocument/semanticTokens/range` request
     * if the server provides a corresponding handler.
     *
     * range?: boolean | {}
     */
    lstalk_bool range;

    /**
     * requests:
     * 
     * The client will send the `textDocument/semanticTokens/full` request
     * if the server provides a corresponding handler.
     *
     * full?: boolean | {}
     * 
     * The client will send the `textDocument/semanticTokens/full/delta`
     * request if the server provides a corresponding handler.
     */
    lstalk_bool delta;

    /**
     * The token types that the client supports.
     */
    char** token_types;
    int token_types_count;

    /**
     * The token modifiers that the client supports.
     */
    char** token_modifiers;
    int token_modifiers_count;

    /**
     * The formats the clients supports.
     */
    int formats;

    /**
     * Whether the client supports tokens that can overlap each other.
     */
    lstalk_bool overlapping_token_support;

    /**
     * Whether the client supports tokens that can span multiple lines.
     */
    lstalk_bool multiline_token_support;

    /**
     * Whether the client allows the server to actively cancel a
     * semantic token request, e.g. supports returning
     * ErrorCodes.ServerCancelled. If a server does the client
     * needs to retrigger the request.
     *
     * @since 3.17.0
     */
    lstalk_bool server_cancel_support;

    /**
     * Whether the client uses semantic tokens to augment existing
     * syntax tokens. If set to `true` client side created syntax
     * tokens and semantic tokens are both used for colorization. If
     * set to `false` the client only uses the returned semantic tokens
     * for colorization.
     *
     * If the value is `undefined` then the client behavior is not
     * specified.
     *
     * @since 3.17.0
     */
    lstalk_bool augments_syntax_tokens;
} SemanticTokensClientCapabilities;

static JSONValue semantic_tokens_client_capabilities_make(SemanticTokensClientCapabilities* semantic_tokens, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    dynamic_registration_set(&result, &semantic_tokens->dynamic_registration, allocator);
    JSONValue requests_full = json_make_object(allocator);
    json_object_const_key_set(&requests_full, "delta", json_make_boolean(semantic_tokens->delta), allocator);
    JSONValue requests = json_make_object(allocator);
    json_object_const_key_set(&requests, "range", json_make_boolean(semantic_tokens->range), allocator);
    json_object_const_key_set(&requests, "full", requests_full, allocator);
    json_object_const_key_set(&result, "requests", requests, allocator);
    json_object_const_key_set(&result, "tokenTypes",
        json_make_string_array(semantic_tokens->token_types, semantic_tokens->token_types_count, allocator), allocator);
    json_object_const_key_set(&result, "tokenModifiers",
        json_make_string_array(semantic_tokens->token_modifiers, semantic_tokens->token_modifiers_count, allocator), allocator);
    json_object_const_key_set(&result, "formats", token_format_make_array(semantic_tokens->formats, allocator), allocator);
    json_object_const_key_set(&result, "overlappingTokenSupport", json_make_boolean(semantic_tokens->overlapping_token_support), allocator);
    json_object_const_key_set(&result, "multilineTokenSupport", json_make_boolean(semantic_tokens->multiline_token_support), allocator);
    json_object_const_key_set(&result, "serverCancelSupport", json_make_boolean(semantic_tokens->server_cancel_support), allocator);
    json_object_const_key_set(&result, "augmentsSyntaxTokens", json_make_boolean(semantic_tokens->augments_syntax_tokens), allocator);
    return result;
}

/**
 * Inlay hint client capabilities.
 *
 * @since 3.17.0
 */
typedef struct InlayHintClientCapabilities {
    /**
     * Whether inlay hints support dynamic registration.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Indicates which properties a client can resolve lazily on a inlay
     * hint.
     *
     * resolveSupport:
     * 
     * The properties that a client can resolve lazily.
     */
    char** properties;
    int properties_count;
} InlayHintClientCapabilities;

/**
 * Client capabilities specific to diagnostic pull requests.
 *
 * @since 3.17.0
 */
typedef struct DiagnosticClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new
     * `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
     * return value for the corresponding server capability as well.
     */
    DynamicRegistration dynamic_registration;

    /**
     * Whether the clients supports related documents for document diagnostic
     * pulls.
     */
    lstalk_bool related_document_support;
} DiagnosticClientCapabilities;

/**
 * Text document specific client capabilities.
 */
typedef struct TextDocumentClientCapabilities {
    TextDocumentSyncClientCapabilities synchronization;

    /**
     * Capabilities specific to the `textDocument/completion` request.
     */
    CompletionClientCapabilities completion;

    /**
     * Capabilities specific to the `textDocument/hover` request.
     */
    HoverClientCapabilities hover;

    /**
     * Capabilities specific to the `textDocument/signatureHelp` request.
     */
    SignatureHelpClientCapabilities signature_help;

    /**
     * Capabilities specific to the `textDocument/declaration` request.
     *
     * @since 3.14.0
     */
    DynamicRegistrationLink declaration;

    /**
     * Capabilities specific to the `textDocument/definition` request.
     */
    DynamicRegistrationLink definition;

    /**
     * Capabilities specific to the `textDocument/typeDefinition` request.
     *
     * @since 3.6.0
     */
    DynamicRegistrationLink type_definition;

    /**
     * Capabilities specific to the `textDocument/implementation` request.
     *
     * @since 3.6.0
     */
    DynamicRegistrationLink implementation;

    /**
     * Capabilities specific to the `textDocument/references` request.
     */
    DynamicRegistration references;

    /**
     * Capabilities specific to the `textDocument/documentHighlight` request.
     */
    DynamicRegistration document_highlight;

    /**
     * Capabilities specific to the `textDocument/documentSymbol` request.
     */
    DocumentSymbolClientCapabilities document_symbol;

    /**
     * Capabilities specific to the `textDocument/codeAction` request.
     */
    CodeActionClientCapabilities code_action;

    /**
     * Capabilities specific to the `textDocument/codeLens` request.
     */
    DynamicRegistration code_lens;

    /**
     * Capabilities specific to the `textDocument/documentLink` request.
     */
    DocumentLinkClientCapabilities document_link;

    /**
     * Capabilities specific to the `textDocument/documentColor` and the
     * `textDocument/colorPresentation` request.
     *
     * @since 3.6.0
     */
    DynamicRegistration color_provider;

    /**
     * Capabilities specific to the `textDocument/formatting` request.
     */
    DynamicRegistration formatting;

    /**
     * Capabilities specific to the `textDocument/rangeFormatting` request.
     */
    DynamicRegistration range_formatting;

    /**
     * Capabilities specific to the `textDocument/onTypeFormatting` request.
     */
    DynamicRegistration on_type_formatting;

    /**
     * Capabilities specific to the `textDocument/rename` request.
     */
    RenameClientCapabilities rename;

    /**
     * Capabilities specific to the `textDocument/publishDiagnostics`
     * notification.
     */
    PublishDiagnosticsClientCapabilities publish_diagnostics;

    /**
     * Capabilities specific to the `textDocument/foldingRange` request.
     *
     * @since 3.10.0
     */
    FoldingRangeClientCapabilities folding_range;

    /**
     * Capabilities specific to the `textDocument/selectionRange` request.
     *
     * @since 3.15.0
     */
    DynamicRegistration selection_range;

    /**
     * Capabilities specific to the `textDocument/linkedEditingRange` request.
     *
     * @since 3.16.0
     */
    DynamicRegistration linked_editing_range;

    /**
     * Capabilities specific to the various call hierarchy requests.
     *
     * @since 3.16.0
     */
    DynamicRegistration call_hierarchy;

    /**
     * Capabilities specific to the various semantic token requests.
     *
     * @since 3.16.0
     */
    SemanticTokensClientCapabilities semantic_tokens;

    /**
     * Capabilities specific to the `textDocument/moniker` request.
     *
     * @since 3.16.0
     */
    DynamicRegistration moniker;

    /**
     * Capabilities specific to the various type hierarchy requests.
     *
     * @since 3.17.0
     */
    DynamicRegistration type_hierarchy;

    /**
     * Capabilities specific to the `textDocument/inlineValue` request.
     *
     * @since 3.17.0
     */
    DynamicRegistration inline_value;

    /**
     * Capabilities specific to the `textDocument/inlayHint` request.
     *
     * @since 3.17.0
     */
    InlayHintClientCapabilities inlay_hint;

    /**
     * Capabilities specific to the diagnostic pull model.
     *
     * @since 3.17.0
     */
    DiagnosticClientCapabilities diagnostic;
} TextDocumentClientCapabilities;

static JSONValue text_document_client_capabilities_make(TextDocumentClientCapabilities* text_document, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);

    JSONValue hover = json_make_object(allocator);
    dynamic_registration_set(&hover, &text_document->hover.dynamic_registration, allocator);
    json_object_const_key_set(&hover, "contentFormat", markup_kind_make_array(text_document->hover.content_format, allocator), allocator);

    JSONValue document_link = json_make_object(allocator);
    dynamic_registration_set(&document_link, &text_document->document_link.dynamic_registration, allocator);
    json_object_const_key_set(&document_link, "tooltipSupport", json_make_boolean(text_document->document_link.tooltip_support), allocator);

    JSONValue inlay_hint = json_make_object(allocator);
    dynamic_registration_set(&inlay_hint, &text_document->inlay_hint.dynamic_registration, allocator);
    JSONValue inlay_hint_resolve_support = json_make_object(allocator);
    json_object_const_key_set(&inlay_hint_resolve_support, "properties",
        json_make_string_array(text_document->inlay_hint.properties, text_document->inlay_hint.properties_count, allocator), allocator);
    json_object_const_key_set(&inlay_hint, "resolveSupport", inlay_hint_resolve_support, allocator);

    JSONValue diagnostic = json_make_object(allocator);
    dynamic_registration_set(&diagnostic, &text_document->diagnostic.dynamic_registration, allocator);
    json_object_const_key_set(&diagnostic, "relatedDocumentSupport", json_make_boolean(text_document->diagnostic.related_document_support), allocator);

    json_object_const_key_set(&result, "synchronization", text_document_sync_client_capabilities_make(&text_document->synchronization, allocator), allocator);
    json_object_const_key_set(&result, "completion", completion_client_capabilities_make(&text_document->completion, allocator), allocator);
    json_object_const_key_set(&result, "hover", hover, allocator);
    json_object_const_key_set(&result, "signatureHelp", signature_help_client_capabilities_make(&text_document->signature_help, allocator), allocator);
    json_object_const_key_set(&result, "declaration", dynamic_registration_link_make(&text_document->declaration, allocator), allocator);
    json_object_const_key_set(&result, "definition", dynamic_registration_link_make(&text_document->definition, allocator), allocator);
    json_object_const_key_set(&result, "typeDefinition", dynamic_registration_link_make(&text_document->type_definition, allocator), allocator);
    json_object_const_key_set(&result, "implementation", dynamic_registration_link_make(&text_document->implementation, allocator), allocator);
    json_object_const_key_set(&result, "references", dynamic_registration_make(&text_document->references, allocator), allocator);
    json_object_const_key_set(&result, "documentHighlight", dynamic_registration_make(&text_document->document_highlight, allocator), allocator);
    json_object_const_key_set(&result, "documentSymbol", document_symbol_client_capabilities_make(&text_document->document_symbol, allocator), allocator);
    json_object_const_key_set(&result, "codeAction", code_action_client_capabilities_make(&text_document->code_action, allocator), allocator);
    json_object_const_key_set(&result, "codeLens", dynamic_registration_make(&text_document->code_lens, allocator), allocator);
    json_object_const_key_set(&result, "documentLink", document_link, allocator);
    json_object_const_key_set(&result, "colorProvider", dynamic_registration_make(&text_document->color_provider, allocator), allocator);
    json_object_const_key_set(&result, "formatting", dynamic_registration_make(&text_document->formatting, allocator), allocator);
    json_object_const_key_set(&result, "rangeFormatting", dynamic_registration_make(&text_document->range_formatting, allocator), allocator);
    json_object_const_key_set(&result, "onTypeFormatting", dynamic_registration_make(&text_document->on_type_formatting, allocator), allocator);
    json_object_const_key_set(&result, "rename", rename_client_capabilities_make(&text_document->rename, allocator), allocator);
    json_object_const_key_set(&result, "publishDiagnostics", publish_diagnostics_client_capabilities_make(&text_document->publish_diagnostics, allocator), allocator);
    json_object_const_key_set(&result, "foldingRange", folding_range_client_capabilities_make(&text_document->folding_range, allocator), allocator);
    json_object_const_key_set(&result, "selectionRange", dynamic_registration_make(&text_document->selection_range, allocator), allocator);
    json_object_const_key_set(&result, "linkedEditingRange", dynamic_registration_make(&text_document->linked_editing_range, allocator), allocator);
    json_object_const_key_set(&result, "callHierarchy", dynamic_registration_make(&text_document->call_hierarchy, allocator), allocator);
    json_object_const_key_set(&result, "semanticTokens", semantic_tokens_client_capabilities_make(&text_document->semantic_tokens, allocator), allocator);
    json_object_const_key_set(&result, "moniker", dynamic_registration_make(&text_document->moniker, allocator), allocator);
    json_object_const_key_set(&result, "typeHierarchy", dynamic_registration_make(&text_document->type_hierarchy, allocator), allocator);
    json_object_const_key_set(&result, "inlineValue", dynamic_registration_make(&text_document->inline_value, allocator), allocator);
    json_object_const_key_set(&result, "inlayHint", inlay_hint, allocator);
    json_object_const_key_set(&result, "diagnostic", diagnostic, allocator);

    return result;
}

/**
 * Notebook specific client capabilities.
 *
 * @since 3.17.0
 */
typedef struct NotebookDocumentSyncClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is
     * set to `true` the client supports the new
     * `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
     * return value for the corresponding server capability as well.
     */
    DynamicRegistration dynamic_registration;

    /**
     * The client supports sending execution summary data per cell.
     */
    lstalk_bool execution_summary_support;
} NotebookDocumentSyncClientCapabilities;

/**
 * Capabilities specific to the notebook document support.
 *
 * @since 3.17.0
 */
typedef struct NotebookDocumentClientCapabilities {
    /**
     * Capabilities specific to notebook document synchronization
     *
     * @since 3.17.0
     */
    NotebookDocumentSyncClientCapabilities synchronization;
} NotebookDocumentClientCapabilities;

/**
 * Show message request client capabilities
 */
typedef struct ShowMessageRequestClientCapabilities {
    /**
     * Capabilities specific to the `MessageActionItem` type.
     *
     * messageActionItem:
     * 
     * Whether the client supports additional attributes which
     * are preserved and sent back to the server in the
     * request's response.
     */
    lstalk_bool message_action_item_additional_properties_support;
} ShowMessageRequestClientCapabilities;

/**
 * Client capabilities for the show document request.
 *
 * @since 3.16.0
 */
typedef struct ShowDocumentClientCapabilities {
    /**
     * The client has support for the show document
     * request.
     */
    lstalk_bool support;
} ShowDocumentClientCapabilities;

/**
 * Window specific client capabilities.
 */
typedef struct WindowClientCapabilities {
    /**
     * It indicates whether the client supports server initiated
     * progress using the `window/workDoneProgress/create` request.
     *
     * The capability also controls Whether client supports handling
     * of progress notifications. If set servers are allowed to report a
     * `workDoneProgress` property in the request specific server
     * capabilities.
     *
     * @since 3.15.0
     */
    lstalk_bool work_done_progress;

    /**
     * Capabilities specific to the showMessage request
     *
     * @since 3.16.0
     */
    ShowMessageRequestClientCapabilities show_message;

    /**
     * Client capabilities for the show document request.
     *
     * @since 3.16.0
     */
    ShowDocumentClientCapabilities show_document;
} WindowClientCapabilities;

static JSONValue window_client_capabilities_make(WindowClientCapabilities* window, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    JSONValue show_message_message_action_item = json_make_object(allocator);
    json_object_const_key_set(&show_message_message_action_item, "additionalPropertiesSupport", json_make_boolean(window->show_message.message_action_item_additional_properties_support), allocator);
    JSONValue show_message = json_make_object(allocator);
    json_object_const_key_set(&show_message, "messageActionItem", show_message_message_action_item, allocator);
    JSONValue show_document = json_make_object(allocator);
    json_object_const_key_set(&show_document, "support", json_make_boolean(window->show_document.support), allocator);
    json_object_const_key_set(&result, "workDoneProgress", json_make_boolean(window->work_done_progress), allocator);
    json_object_const_key_set(&result, "showMessage", show_message, allocator);
    json_object_const_key_set(&result, "showDocument", show_document, allocator);
    return result;
}

/**
 * Client capabilities specific to regular expressions.
 */
typedef struct RegularExpressionsClientCapabilities {
    /**
     * The engine's name.
     */
    char* engine;

    /**
     * The engine's version.
     */
    char* version;
} RegularExpressionsClientCapabilities;

/**
 * Client capabilities specific to the used markdown parser.
 *
 * @since 3.16.0
 */
typedef struct MarkdownClientCapabilities {
    /**
     * The name of the parser.
     */
    char* parser;

    /**
     * The version of the parser.
     */
    char* version;

    /**
     * A list of HTML tags that the client allows / supports in
     * Markdown.
     *
     * @since 3.17.0
     */
    char** allowed_tags;
    int allowed_tags_count;
} MarkdownClientCapabilities;

/**
 * General client capabilities.
 *
 * @since 3.16.0
 */
typedef struct GeneralClientCapabilities {
    /**
     * Client capability that signals how the client
     * handles stale requests (e.g. a request
     * for which the client will not process the response
     * anymore since the information is outdated).
     *
     * @since 3.17.0
     *
     * staleRequestSupport:
     * 
     * The client will actively cancel the request.
     */
    lstalk_bool cancel;

    /**
     * The list of requests for which the client
     * will retry the request if it receives a
     * response with error code `ContentModified``
     * 
     * staleRequestSupport
     */
    char** retry_on_content_modified;
    int retry_on_content_modified_count;

    /**
     * Client capabilities specific to regular expressions.
     *
     * @since 3.16.0
     */
    RegularExpressionsClientCapabilities regular_expressions;

    /**
     * Client capabilities specific to the client's markdown parser.
     *
     * @since 3.16.0
     */
    MarkdownClientCapabilities markdown;

    /**
     * The position encodings supported by the client. Client and server
     * have to agree on the same position encoding to ensure that offsets
     * (e.g. character position in a line) are interpreted the same on both
     * side.
     *
     * To keep the protocol backwards compatible the following applies: if
     * the value 'utf-16' is missing from the array of position encodings
     * servers can assume that the client supports UTF-16. UTF-16 is
     * therefore a mandatory encoding.
     *
     * If omitted it defaults to ['utf-16'].
     *
     * Implementation considerations: since the conversion from one encoding
     * into another requires the content of the file / line the conversion
     * is best done where the file is read which is usually on the server
     * side.
     *
     * @since 3.17.0
     */
    int position_encodings;
} GeneralClientCapabilities;

static JSONValue general_client_capabilities_make(GeneralClientCapabilities* general, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    JSONValue stale_request_support = json_make_object(allocator);
    json_object_const_key_set(&stale_request_support, "cancel", json_make_boolean(general->cancel), allocator);
    json_object_const_key_set(&stale_request_support, "retryOnContentModified",
        json_make_string_array(general->retry_on_content_modified, general->retry_on_content_modified_count, allocator), allocator);
    JSONValue regular_expressions = json_make_object(allocator);
    json_object_const_key_set(&regular_expressions, "engine", json_make_string(general->regular_expressions.engine, allocator), allocator);
    json_object_const_key_set(&regular_expressions, "version", json_make_string(general->regular_expressions.version, allocator), allocator);
    JSONValue markdown = json_make_object(allocator);
    json_object_const_key_set(&markdown, "parser", json_make_string(general->markdown.parser, allocator), allocator);
    json_object_const_key_set(&markdown, "version", json_make_string(general->markdown.version, allocator), allocator);
    json_object_const_key_set(&markdown, "allowedTags",
        json_make_string_array(general->markdown.allowed_tags, general->markdown.allowed_tags_count, allocator), allocator);
    json_object_const_key_set(&result, "staleRequestSupport", stale_request_support, allocator);
    json_object_const_key_set(&result, "regularExpressions", regular_expressions, allocator);
    json_object_const_key_set(&result, "markdown", markdown, allocator);
    json_object_const_key_set(&result, "positionEncodings", position_encoding_kind_make_array(general->position_encodings, allocator), allocator);
    return result;
}

/**
 * The capabilities provided by the client (editor or tool)
 */
typedef struct ClientCapabilities {
    /**
     * Workspace specific client capabilities.
     */
    Workspace workspace;

    /**
     * Text document specific client capabilities.
     */
    TextDocumentClientCapabilities text_document;

    /**
     * Capabilities specific to the notebook document support.
     *
     * @since 3.17.0
     */
    NotebookDocumentClientCapabilities notebook_document;

    /**
     * Window specific client capabilities.
     */
    WindowClientCapabilities window;

    /**
     * General client capabilities.
     *
     * @since 3.16.0
     */
    GeneralClientCapabilities general;
} ClientCapabilities;

static JSONValue client_capabilities_make(ClientCapabilities* capabilities, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);
    JSONValue notebook_sync = json_make_object(allocator);
    dynamic_registration_set(&notebook_sync, &capabilities->notebook_document.synchronization.dynamic_registration, allocator);
    json_object_const_key_set(&notebook_sync, "executionSummarySupport", json_make_boolean(capabilities->notebook_document.synchronization.execution_summary_support), allocator);
    JSONValue notebook_document = json_make_object(allocator);
    json_object_const_key_set(&notebook_document, "synchronization", notebook_sync, allocator);
    json_object_const_key_set(&result, "workspace", workspace_make(&capabilities->workspace, allocator), allocator);
    json_object_const_key_set(&result, "textDocument", text_document_client_capabilities_make(&capabilities->text_document, allocator), allocator);
    json_object_const_key_set(&result, "notebookDocument", notebook_document, allocator);
    json_object_const_key_set(&result, "window", window_client_capabilities_make(&capabilities->window, allocator), allocator);
    json_object_const_key_set(&result, "general", general_client_capabilities_make(&capabilities->general, allocator), allocator);
    return result;
}

//
// End Client Capabilities
//

//
// Begin Server Capabilities
//

static char** parse_string_array(JSONValue* value, char* key, int* count, LSTalk_MemoryAllocator* allocator) {
    if (value == NULL || value->type != JSON_VALUE_OBJECT || count == NULL) {
        return NULL;
    }

    JSONValue* array = json_object_get_ptr(value, key);
    if (array == NULL || array->type != JSON_VALUE_ARRAY) {
        return NULL;
    }

    size_t length = json_array_length(array);
    char** result = (char**)allocator->calloc(length, sizeof(char*));
    for (size_t i = 0; i < length; i++) {
        JSONValue* item = json_array_get_ptr(array, i);
        if (item != NULL && item->type == JSON_VALUE_STRING) {
            result[i] = json_move_string(item);
        }
    }

    *count = (int)length;
    return result;
}

typedef struct WorkDoneProgressOptions {
    lstalk_bool value;
} WorkDoneProgressOptions;

#define WORK_DONE_PROGRESS_NAME "workDoneProgress"

static WorkDoneProgressOptions work_done_progress_parse(JSONValue* value) {
    WorkDoneProgressOptions result;
    result.value = 0;

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue work_done_progress = json_object_get(value, WORK_DONE_PROGRESS_NAME);
    if (work_done_progress.type != JSON_VALUE_BOOLEAN) {
        return result;
    }

    result.value = work_done_progress.value.bool_value;
    return result;
}

#if LSTALK_TESTS
// These functions are currently only used in testing. Add to main library when needed.
static JSONValue work_done_progress_json_make(WorkDoneProgressOptions* options, LSTalk_MemoryAllocator* allocator) {
    if (options == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_object(allocator);
    json_object_const_key_set(&result, WORK_DONE_PROGRESS_NAME, json_make_boolean(options->value), allocator);
    return result;
}

static void work_done_progress_json_set(WorkDoneProgressOptions* options, JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    if (options == NULL || value == NULL || value->type != JSON_VALUE_OBJECT) {
        return;
    }

    json_object_const_key_set(value, WORK_DONE_PROGRESS_NAME, json_make_boolean(options->value), allocator);
}
#endif

/**
 * Static registration options to be returned in the initialize request.
 */
typedef struct StaticRegistrationOptions {
    /**
     * The id used to register the request. The id can be used to deregister
     * the request again. See also Registration#id.
     */
    char* id;
} StaticRegistrationOptions;

static StaticRegistrationOptions static_registration_options_parse(JSONValue* value) {
    StaticRegistrationOptions result;
    result.id = NULL;

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue id = json_object_get(value, "id");
    if (id.type != JSON_VALUE_STRING) {
        return result;
    }

    result.id = json_move_string(&id);
    return result;
}

#if LSTALK_TESTS
// This function is currently only used in testing. Add to main library when needed.
static void static_registration_options_json_set(StaticRegistrationOptions* options, JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    if (options == NULL || value == NULL) {
        return;
    }

    json_object_const_key_set(value, "id", json_make_string(options->id, allocator), allocator);
}
#endif

static void static_registration_options_free(StaticRegistrationOptions* static_registration, LSTalk_MemoryAllocator* allocator) {
    if (static_registration == NULL || static_registration->id == NULL) {
        return;
    }

    allocator->free(static_registration->id);
}

/**
 * Defines how the host (editor) should sync document changes to the language
 * server.
 */
typedef enum {
    /**
     * Documents should not be synced at all.
     */
    TEXTDOCUMENTSYNCKIND_NONE = 0,

    /**
     * Documents are synced by always sending the full content
     * of the document.
     */
    TEXTDOCUMENTSYNCKIND_FULL = 1,

    /**
     * Documents are synced by sending the full content on open.
     * After that only incremental updates to the document are
     * sent.
     */
    TEXTDOCUMENTSYNCKIND_INCREMENTAL = 2,
} TextDocumentSyncKind;

/**
 * Defines how text documents are synced.
 */
typedef struct TextDocumentSyncOptions {
    /**
     * Open and close notifications are sent to the server. If omitted open
     * close notifications should not be sent.
     */
    lstalk_bool open_close;

    /**
     * Change notifications are sent to the server. See
     * TextDocumentSyncKind.None, TextDocumentSyncKind.Full and
     * TextDocumentSyncKind.Incremental. If omitted it defaults to
     * TextDocumentSyncKind.None.
     */
    TextDocumentSyncKind change;
} TextDocumentSyncOptions;

/**
 * A notebook document filter denotes a notebook document by
 * different properties.
 *
 * @since 3.17.0
 */
typedef struct NotebookDocumentFilter {
    /** The type of the enclosing notebook. */
    char* notebook_type;

    /** A Uri [scheme](#Uri.scheme), like `file` or `untitled`. */
    char* scheme;

    /** A glob pattern. */
    char* pattern;
} NotebookDocumentFilter;

/**
 * The notebooks to be synced
 */
typedef struct NotebookSelector {
    /**
     * The notebook to be synced. If a string
     * value is provided it matches against the
     * notebook type. '*' matches every notebook.
     */
    NotebookDocumentFilter notebook;

    /**
     * The cells of the matching notebook to be synced.
     */
    char** cells;
    int cells_count;
} NotebookSelector;

/**
 * Options specific to a notebook plus its cells
 * to be synced to the server.
 *
 * If a selector provides a notebook document
 * filter but no cell selector all cells of a
 * matching notebook document will be synced.
 *
 * If a selector provides no notebook document
 * filter but only a cell selector all notebook
 * documents that contain at least one matching
 * cell will be synced.
 *
 * @since 3.17.0
 */
typedef struct NotebookDocumentSyncOptions {
    StaticRegistrationOptions static_registration;

    /**
     * The notebooks to be synced
     */
    NotebookSelector* notebook_selector;
    int notebook_selector_count;

    /**
     * Whether save notification should be forwarded to
     * the server. Will only be honored if mode === `notebook`.
     */
    lstalk_bool save;
} NotebookDocumentSyncOptions;

/**
 * Completion options.
 */
typedef struct CompletionOptions {
    WorkDoneProgressOptions work_done_progress;

    /**
     * The additional characters, beyond the defaults provided by the client (typically
     * [a-zA-Z]), that should automatically trigger a completion request. For example
     * `.` in JavaScript represents the beginning of an object property or method and is
     * thus a good candidate for triggering a completion request.
     *
     * Most tools trigger a completion request automatically without explicitly
     * requesting it using a keyboard shortcut (e.g. Ctrl+Space). Typically they
     * do so when the user starts to type an identifier. For example if the user
     * types `c` in a JavaScript file code complete will automatically pop up
     * present `console` besides others as a completion item. Characters that
     * make up identifiers don't need to be listed here.
     */
    char** trigger_characters;
    int trigger_characters_count;

    /**
     * The list of all possible characters that commit a completion. This field
     * can be used if clients don't support individual commit characters per
     * completion item. See client capability
     * `completion.completionItem.commitCharactersSupport`.
     *
     * If a server provides both `allCommitCharacters` and commit characters on
     * an individual completion item the ones on the completion item win.
     *
     * @since 3.2.0
     */
    char** all_commit_characters;
    int all_commit_characters_count;

    /**
     * The server provides support to resolve additional
     * information for a completion item.
     */
    lstalk_bool resolve_provider;

    /**
     * The server supports the following `CompletionItem` specific
     * capabilities.
     *
     * @since 3.17.0
     *
     * completionItem:
     * 
     * The server has support for completion item label
     * details (see also `CompletionItemLabelDetails`) when receiving
     * a completion item in a resolve call.
     *
     * @since 3.17.0
     */
    lstalk_bool completion_item_label_details_support;
} CompletionOptions;

typedef struct HoverOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;
} HoverOptions;

/**
 * The server provides signature help support.
 */
typedef struct SignatureHelpOptions {
    WorkDoneProgressOptions work_done_progress;

    /**
     * The characters that trigger signature help
     * automatically.
     */
    char** trigger_characters;
    int trigger_characters_count;

    /**
     * List of characters that re-trigger signature help.
     *
     * These trigger characters are only active when signature help is already
     * showing. All trigger characters are also counted as re-trigger
     * characters.
     *
     * @since 3.15.0
     */
    char** retrigger_characters;
    int retrigger_characters_count;
} SignatureHelpOptions;

/**
 * A document filter denotes a document through properties like language, scheme or pattern.
 */
typedef struct DocumentFilter {
    /**
     * A language id, like `typescript`.
     */
    char* language;

    /**
     * A Uri [scheme](#Uri.scheme), like `file` or `untitled`.
     */
    char* scheme;

    /**
     * A glob pattern, like `*.{ts,js}`.
     *
     * Glob patterns can have the following syntax:
     * - `*` to match one or more characters in a path segment
     * - `?` to match on one character in a path segment
     * - `**` to match any number of path segments, including none
     * - `{}` to group sub patterns into an OR expression. (e.g. `**​ / *.{ts,js}`
     *   matches all TypeScript and JavaScript files)
     * - `[]` to declare a range of characters to match in a path segment
     *   (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
     * - `[!...]` to negate a range of characters to match in a path segment
     *   (e.g., `example.[!0-9]` to match on `example.a`, `example.b`, but
     *   not `example.0`)
     */
    char* pattern;
} DocumentFilter;

/**
 * General text document registration options.
 */
typedef struct TextDocumentRegistrationOptions {
    /**
     * A document selector to identify the scope of the registration. If set to
     * null the document selector provided on the client side will be used.
     */
    DocumentFilter* document_selector;
    int document_selector_count;
} TextDocumentRegistrationOptions;

static TextDocumentRegistrationOptions text_document_registration_options_parse(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    TextDocumentRegistrationOptions result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue* document_selector = json_object_get_ptr(value, "documentSelector");
    if (document_selector != NULL && document_selector->type != JSON_VALUE_ARRAY) {
        return result;
    }

    size_t length = json_array_length(document_selector);
    result.document_selector_count = (int)length;
    result.document_selector = (DocumentFilter*)allocator->calloc(length, sizeof(DocumentFilter));
    for (size_t i = 0; i < length; i++) {
        JSONValue* item = json_array_get_ptr(document_selector, i);
        DocumentFilter* filter = &result.document_selector[i];

        JSONValue* language = json_object_get_ptr(item, "language");
        if (language != NULL && language->type == JSON_VALUE_STRING) {
            filter->language = json_move_string(language);
        }

        JSONValue* scheme = json_object_get_ptr(item, "scheme");
        if (scheme != NULL && scheme->type == JSON_VALUE_STRING) {
            filter->scheme = json_move_string(scheme);
        }

        JSONValue* pattern = json_object_get_ptr(item, "pattern");
        if (pattern != NULL && pattern->type == JSON_VALUE_STRING) {
            filter->pattern = json_move_string(pattern);
        }
    }

    return result;
}

#if LSTALK_TESTS
// This function is currently only used in testing. Add to main library when needed.
static void text_document_registration_options_json_set(TextDocumentRegistrationOptions* options, JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    if (options == NULL || value == NULL || value->type != JSON_VALUE_OBJECT) {
        return;
    }

    if (options->document_selector_count == 0) {
        json_object_const_key_set(value, "documentFilter", json_make_null(), allocator);
        return;
    }

    JSONValue array = json_make_array(allocator);
    for (int i = 0; i < options->document_selector_count; i++) {
        DocumentFilter* filter = &options->document_selector[i];
        JSONValue item = json_make_object(allocator);
        if (filter->language != NULL) { 
            json_object_const_key_set(&item, "language", json_make_string(filter->language, allocator), allocator);
        }
        if (filter->scheme != NULL) {
            json_object_const_key_set(&item, "scheme", json_make_string(filter->scheme, allocator), allocator);
        }
        if (filter->pattern != NULL) {
            json_object_const_key_set(&item, "pattern", json_make_string(filter->pattern, allocator), allocator);
        }
        json_array_push(&array, item, allocator);
    }
    json_object_const_key_set(value, "documentFilter", array, allocator);
}
#endif

static void text_document_registration_options_free(TextDocumentRegistrationOptions* text_document_registration, LSTalk_MemoryAllocator* allocator) {
    if (text_document_registration == NULL) {
        return;
    }

    if (text_document_registration->document_selector != NULL) {
        for (int i = 0; i < text_document_registration->document_selector_count; i++) {
            DocumentFilter* filter = &text_document_registration->document_selector[i];

            if (filter->language != NULL) {
                allocator->free(filter->language);
            }

            if (filter->scheme != NULL) {
                allocator->free(filter->scheme);
            }

            if (filter->pattern != NULL) {
                allocator->free(filter->pattern);
            }
        }

        allocator->free(text_document_registration->document_selector);
    }
}

/**
 * The server provides go to declaration support.
 *
 * @since 3.14.0
 */
typedef struct DeclarationRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} DeclarationRegistrationOptions;

/**
 * The server provides goto definition support.
 */
typedef struct DefinitionOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;
} DefinitionOptions;

/**
 * The server provides goto type definition support.
 *
 * @since 3.6.0
 */
typedef struct TypeDefinitionRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} TypeDefinitionRegistrationOptions;

/**
 * The server provides goto implementation support.
 *
 * @since 3.6.0
 */
typedef struct ImplementationRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} ImplementationRegistrationOptions;

/**
 * The server provides find references support.
 */
typedef struct ReferenceOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;
} ReferenceOptions;

/**
 * The server provides document highlight support.
 */
typedef struct DocumentHighlightOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;
} DocumentHighlightOptions;

/**
 * The server provides document symbol support.
 */
typedef struct DocumentSymbolOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;

    /**
     * A human-readable string that is shown when multiple outlines trees
     * are shown for the same document.
     *
     * @since 3.16.0
     */
    char* label;
} DocumentSymbolOptions;

/**
 * The server provides code actions. The `CodeActionOptions` return type is
 * only valid if the client signals code action literal support via the
 * property `textDocument.codeAction.codeActionLiteralSupport`.
 */
typedef struct CodeActionOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;

    /**
     * CodeActionKinds that this server may return.
     *
     * The list of kinds may be generic, such as `CodeActionKind.Refactor`,
     * or the server may list out every specific kind they provide.
     */
    int code_action_kinds;

    /**
     * The server provides support to resolve additional
     * information for a code action.
     *
     * @since 3.16.0
     */
    lstalk_bool resolve_provider;
} CodeActionOptions;

/**
 * The server provides code lens.
 */
typedef struct CodeLensOptions {
    WorkDoneProgressOptions work_done_progress;

    /**
     * Code lens has a resolve provider as well.
     */
    lstalk_bool resolve_provider;
} CodeLensOptions;

/**
 * The server provides document link support.
 */
typedef struct DocumentLinkOptions {
    WorkDoneProgressOptions work_done_progress;

    /**
     * Document links have a resolve provider as well.
     */
    lstalk_bool resolve_provider;
} DocumentLinkOptions;

/**
 * The server provides color provider support.
 *
 * @since 3.6.0
 */
typedef struct DocumentColorRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} DocumentColorRegistrationOptions;

/**
 * The server provides document formatting.
 */
typedef struct DocumentFormattingOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;
} DocumentFormattingOptions;

/**
 * The server provides document range formatting.
 */
typedef struct DocumentRangeFormattingOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;
} DocumentRangeFormattingOptions;

/**
 * The server provides document formatting on typing.
 */
typedef struct DocumentOnTypeFormattingOptions {
    /**
     * A character on which formatting should be triggered, like `{`.
     */
    char* first_trigger_character;

    /**
     * More trigger characters.
     */
    char** more_trigger_character;
    int more_trigger_character_count;
} DocumentOnTypeFormattingOptions;

/**
 * The server provides rename support. RenameOptions may only be
 * specified if the client states that it supports
 * `prepareSupport` in its initial `initialize` request.
 */
typedef struct RenameOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;

    /**
     * Renames should be checked and tested before being executed.
     */
    lstalk_bool prepare_provider;
} RenameOptions;

/**
 * The server provides folding provider support.
 *
 * @since 3.10.0
 */
typedef struct FoldingRangeRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} FoldingRangeRegistrationOptions;

/**
 * The server provides execute command support.
 */
typedef struct ExecuteCommandOptions {
    WorkDoneProgressOptions work_done_progress;

    /**
     * The commands to be executed on the server
     */
    char** commands;
    int commands_count;
} ExecuteCommandOptions;

/**
 * The server provides selection range support.
 *
 * @since 3.15.0
 */
typedef struct SelectionRangeRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} SelectionRangeRegistrationOptions;

/**
 * The server provides linked editing range support.
 *
 * @since 3.16.0
 */
typedef struct LinkedEditingRangeRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} LinkedEditingRangeRegistrationOptions;

/**
 * The server provides call hierarchy support.
 *
 * @since 3.16.0
 */
typedef struct CallHierarchyRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} CallHierarchyRegistrationOptions;

typedef struct SemanticTokensLegend {
    /**
     * The token types a server uses.
     */
    char** token_types;
    int token_types_count;

    /**
     * The token modifiers a server uses.
     */
    char** token_modifiers;
    int token_modifiers_count;
} SemanticTokensLegend;

typedef struct SemanticTokensOptions {
    WorkDoneProgressOptions work_done_progress;

    /**
     * The legend used by the server
     */
    SemanticTokensLegend legend;

    /**
     * Server supports providing semantic tokens for a specific range
     * of a document.
     */
    lstalk_bool range;

    /**
     * Server supports providing semantic tokens for a full document.
     *
     * full:
     * 
     * The server supports deltas for full documents.
     */
    lstalk_bool full_delta;
} SemanticTokensOptions;

/**
 * The server provides semantic tokens support.
 *
 * @since 3.16.0
 */
typedef struct SemanticTokensRegistrationOptions {
    SemanticTokensOptions semantic_tokens;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
} SemanticTokensRegistrationOptions;

/**
 * Whether server provides moniker support.
 *
 * @since 3.16.0
 */
typedef struct MonikerRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    lstalk_bool is_supported;
} MonikerRegistrationOptions;

/**
 * The server provides type hierarchy support.
 *
 * @since 3.17.0
 */
typedef struct TypeHierarchyRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} TypeHierarchyRegistrationOptions;

/**
 * The server provides inline values.
 *
 * @since 3.17.0
 */
typedef struct InlineValueRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;
} InlineValueRegistrationOptions;

/**
 * Inlay hint options used during static or dynamic registration.
 *
 * @since 3.17.0
 */
typedef struct InlayHintRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;
    lstalk_bool is_supported;

    /**
     * The server provides support to resolve additional
     * information for an inlay hint item.
     */
    lstalk_bool resolve_provider;
} InlayHintRegistrationOptions;

/**
 * Diagnostic registration options.
 *
 * @since 3.17.0
 */
typedef struct DiagnosticRegistrationOptions {
    WorkDoneProgressOptions work_done_progress;
    TextDocumentRegistrationOptions text_document_registration;
    StaticRegistrationOptions static_registration;

    /**
     * An optional identifier under which the diagnostics are
     * managed by the client.
     */
    char* identifier;

    /**
     * Whether the language has inter file dependencies meaning that
     * editing code in one file can result in a different diagnostic
     * set in another file. Inter file dependencies are common for
     * most programming languages and typically uncommon for linters.
     */
    lstalk_bool inter_file_dependencies;

    /**
     * The server provides support for workspace diagnostics as well.
     */
    lstalk_bool workspace_diagnostics;
} DiagnosticRegistrationOptions;

/**
 * The server provides workspace symbol support.
 */
typedef struct WorkspaceSymbolOptions {
    WorkDoneProgressOptions work_done_progress;
    lstalk_bool is_supported;

    /**
     * The server provides support to resolve additional
     * information for a workspace symbol.
     *
     * @since 3.17.0
     */
    lstalk_bool resolve_provider;
} WorkspaceSymbolOptions;

/**
 * The server supports workspace folder.
 *
 * @since 3.6.0
 */
typedef struct WorkspaceFoldersServerCapabilities {
    /**
     * The server has support for workspace folders
     */
    lstalk_bool supported;

    /**
     * Whether the server wants to receive workspace folder
     * change notifications.
     *
     * If a string is provided, the string is treated as an ID
     * under which the notification is registered on the client
     * side. The ID can be used to unregister for these events
     * using the `client/unregisterCapability` request.
     */
    char* change_notifications;
    lstalk_bool change_notifications_boolean;
} WorkspaceFoldersServerCapabilities;

/**
 * A pattern kind describing if a glob pattern matches a file a folder or
 * both.
 *
 * @since 3.16.0
 */
typedef enum {
    /**
     * The pattern matches a file only.
     */
    FILEOPERATIONPATTERNKIND_FILE = 1 << 0,

    /**
     * The pattern matches a folder only.
     */
    FILEOPERATIONPATTERNKIND_FOLDER = 1 << 1,
} FileOperationPatternKind;

/**
 * Matching options for the file operation pattern.
 *
 * @since 3.16.0
 */
typedef struct FileOperationPatternOptions {
    /**
     * The pattern should be matched ignoring casing.
     */
    lstalk_bool ignore_case;
} FileOperationPatternOptions;

/**
 * The options to register for file operations.
 *
 * @since 3.16.0
 */
typedef struct FileOperationPattern {
    /**
     * The glob pattern to match. Glob patterns can have the following syntax:
     * - `*` to match one or more characters in a path segment
     * - `?` to match on one character in a path segment
     * - `**` to match any number of path segments, including none
     * - `{}` to group sub patterns into an OR expression. (e.g. `**​ / *.{ts,js}`
     *   matches all TypeScript and JavaScript files)
     * - `[]` to declare a range of characters to match in a path segment
     *   (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
     * - `[!...]` to negate a range of characters to match in a path segment
     *   (e.g., `example.[!0-9]` to match on `example.a`, `example.b`, but
     *   not `example.0`)
     */
    char* glob;

    /**
     * Whether to match files or folders with this pattern.
     *
     * Matches both if undefined.
     */
    int matches;

    /**
     * Additional options used during matching.
     */
    FileOperationPatternOptions options;
} FileOperationPattern;

/**
 * A filter to describe in which file operation requests or notifications
 * the server is interested in.
 *
 * @since 3.16.0
 */
typedef struct FileOperationFilter {
    /**
     * A Uri like `file` or `untitled`.
     */
    char* scheme;

    /**
     * The actual file operation pattern.
     */
    FileOperationPattern pattern;
} FileOperationFilter;

/**
 * The options to register for file operations.
 *
 * @since 3.16.0
 */
typedef struct FileOperationRegistrationOptions {
    /**
     * The actual filters.
     */
    FileOperationFilter* filters;
    int filters_count;
} FileOperationRegistrationOptions;

static FileOperationRegistrationOptions file_operation_registration_options_parse(JSONValue* value, char* key, LSTalk_MemoryAllocator* allocator) {
    FileOperationRegistrationOptions result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue* operation = json_object_get_ptr(value, key);
    if (operation != NULL && operation->type == JSON_VALUE_OBJECT) {
        JSONValue* filters = json_object_get_ptr(operation, "filters");
        if (filters != NULL && filters->type == JSON_VALUE_ARRAY) {
            size_t length = json_array_length(filters);
            result.filters_count = (int)length;
            result.filters = (FileOperationFilter*)allocator->calloc(length, sizeof(FileOperationFilter));
            for (size_t i = 0; i < length; i++) {
                JSONValue* item = json_array_get_ptr(filters, i);
                FileOperationFilter* filter = &result.filters[i];

                JSONValue* scheme = json_object_get_ptr(item, "scheme");
                if (scheme != NULL && scheme->type == JSON_VALUE_STRING) {
                    filter->scheme = json_move_string(scheme);
                }

                JSONValue* pattern = json_object_get_ptr(item, "pattern");
                if (pattern != NULL && pattern->type == JSON_VALUE_OBJECT) {
                    JSONValue* glob = json_object_get_ptr(pattern, "glob");
                    if (glob != NULL && glob->type == JSON_VALUE_STRING) {
                        filter->pattern.glob = json_move_string(glob);
                    }

                    JSONValue matches = json_object_get(pattern, "matches");
                    if (matches.type == JSON_VALUE_ARRAY) {
                        for (size_t j = 0; j < json_array_length(&matches); j++) {
                            JSONValue match_item = json_array_get(&matches, j);
                            if (match_item.type == JSON_VALUE_STRING) {
                                if (strcmp(match_item.value.string_value, "file") == 0) {
                                    filter->pattern.matches |= FILEOPERATIONPATTERNKIND_FILE;
                                } else if (strcmp(match_item.value.string_value, "folder") == 0) {
                                    filter->pattern.matches |= FILEOPERATIONPATTERNKIND_FOLDER;
                                }
                            }
                        }
                    } else {
                        filter->pattern.matches = (FILEOPERATIONPATTERNKIND_FILE | FILEOPERATIONPATTERNKIND_FOLDER);
                    }

                    JSONValue options = json_object_get(pattern, "options");
                    if (options.type == JSON_VALUE_OBJECT) {
                        JSONValue ignore_case = json_object_get(&options, "ignoreCase");
                        if (ignore_case.type == JSON_VALUE_BOOLEAN) {
                            filter->pattern.options.ignore_case = ignore_case.value.bool_value;
                        }
                    }
                }
            }
        }
    }

    return result;
}

#if LSTALK_TESTS
// This function is currently only used in testing. Add to main library when needed.
static JSONValue file_operation_registration_json(FileOperationRegistrationOptions* options, LSTalk_MemoryAllocator* allocator) {
    if (options == NULL) {
        return json_make_null();
    }

    JSONValue value = json_make_object(allocator);

    if (options->filters_count > 0) {
        JSONValue filters = json_make_array(allocator);
        for (int filter_index = 0; filter_index < options->filters_count; filter_index++) {
            FileOperationFilter* item = &options->filters[filter_index];
            JSONValue filter = json_make_object(allocator);
            if (item->scheme != NULL) {
                json_object_const_key_set(&filter, "scheme", json_make_string(item->scheme, allocator), allocator);
            }

            JSONValue pattern = json_make_object(allocator);
            json_object_const_key_set(&pattern, "glob", json_make_string(item->pattern.glob, allocator), allocator);
            if (item->pattern.matches == FILEOPERATIONPATTERNKIND_FILE) {
                json_object_const_key_set(&pattern, "matches", json_make_string_const("file"), allocator);
            } else if (item->pattern.matches == FILEOPERATIONPATTERNKIND_FOLDER) {
                json_object_const_key_set(&pattern, "matches", json_make_string_const("folder"), allocator);
            }
            JSONValue pattern_options = json_make_object(allocator);
            json_object_const_key_set(&pattern_options, "ignoreCase", json_make_boolean(item->pattern.options.ignore_case), allocator);
            json_object_const_key_set(&pattern, "options", pattern_options, allocator);
            json_object_const_key_set(&filter, "pattern", pattern, allocator);
            json_array_push(&filters, filter, allocator);
        }
        json_object_const_key_set(&value, "filters", filters, allocator);
    }

    return value;
}
#endif

static void file_operation_registration_options_free(FileOperationRegistrationOptions* file_operation_registration, LSTalk_MemoryAllocator* allocator) {
    if (file_operation_registration == NULL) {
        return;
    }

    if (file_operation_registration->filters != NULL) {
        for (int i = 0; i < file_operation_registration->filters_count; i++) {
            FileOperationFilter* filter = &file_operation_registration->filters[i];
            if (filter->scheme != NULL) {
                allocator->free(filter->scheme);
            }

            if (filter->pattern.glob != NULL) {
                allocator->free(filter->pattern.glob);
            }
        }

        allocator->free(file_operation_registration->filters);
    }
}

typedef struct FileOperationsServer {
    /**
     * The server is interested in receiving didCreateFiles
     * notifications.
     */
    FileOperationRegistrationOptions did_create;

    /**
     * The server is interested in receiving willCreateFiles requests.
     */
    FileOperationRegistrationOptions will_create;

    /**
     * The server is interested in receiving didRenameFiles
     * notifications.
     */
    FileOperationRegistrationOptions did_rename;

    /**
     * The server is interested in receiving willRenameFiles requests.
     */
    FileOperationRegistrationOptions will_rename;

    /**
     * The server is interested in receiving didDeleteFiles file
     * notifications.
     */
    FileOperationRegistrationOptions did_delete;

    /**
     * The server is interested in receiving willDeleteFiles file
     * requests.
     */
    FileOperationRegistrationOptions will_delete;
} FileOperationsServer;

/**
 * Workspace specific server capabilities
 */
typedef struct WorkspaceServer {
    /**
     * The server supports workspace folder.
     *
     * @since 3.6.0
     */
    WorkspaceFoldersServerCapabilities workspace_folders;

    /**
     * The server is interested in file notifications/requests.
     *
     * @since 3.16.0
     */
    FileOperationsServer file_operations;
} WorkspaceServer;

/**
 * The capabilities the language server provides.
 */
typedef struct ServerCapabilities {
    /**
     * The position encoding the server picked from the encodings offered
     * by the client via the client capability `general.positionEncodings`.
     *
     * If the client didn't provide any position encodings the only valid
     * value that a server can return is 'utf-16'.
     *
     * If omitted it defaults to 'utf-16'.
     *
     * @since 3.17.0
     */
    int position_encoding;

    /**
     * Defines how text documents are synced. Is either a detailed structure
     * defining each notification or for backwards compatibility the
     * TextDocumentSyncKind number. If omitted it defaults to
     * `TextDocumentSyncKind.None`.
     */
    TextDocumentSyncOptions text_document_sync;

    /**
     * Defines how notebook documents are synced.
     *
     * @since 3.17.0
     */
    NotebookDocumentSyncOptions notebook_document_sync;

    /**
     * The server provides completion support.
     */
    CompletionOptions completion_provider;

    /**
     * The server provides hover support.
     */
    HoverOptions hover_provider;

    /**
     * The server provides signature help support.
     */
    SignatureHelpOptions signature_help_provider;

    /**
     * The server provides go to declaration support.
     *
     * @since 3.14.0
     */
    DeclarationRegistrationOptions declaration_provider;

    /**
     * The server provides goto definition support.
     */
    DefinitionOptions definition_provider;

    /**
     * The server provides goto type definition support.
     *
     * @since 3.6.0
     */
    TypeDefinitionRegistrationOptions type_definition_provider;

    /**
     * The server provides goto implementation support.
     *
     * @since 3.6.0
     */
    ImplementationRegistrationOptions implementation_provider;

    /**
     * The server provides find references support.
     */
    ReferenceOptions references_provider;

    /**
     * The server provides document highlight support.
     */
    DocumentHighlightOptions document_highlight_provider;

    /**
     * The server provides document symbol support.
     */
    DocumentSymbolOptions document_symbol_provider;

    /**
     * The server provides code actions. The `CodeActionOptions` return type is
     * only valid if the client signals code action literal support via the
     * property `textDocument.codeAction.codeActionLiteralSupport`.
     */
    CodeActionOptions code_action_provider;

    /**
     * The server provides code lens.
     */
    CodeLensOptions code_lens_provider;

    /**
     * The server provides document link support.
     */
    DocumentLinkOptions document_link_provider;

    /**
     * The server provides color provider support.
     *
     * @since 3.6.0
     */
    DocumentColorRegistrationOptions color_provider;

    /**
     * The server provides document formatting.
     */
    DocumentFormattingOptions document_formatting_provider;

    /**
     * The server provides document range formatting.
     */
    DocumentRangeFormattingOptions document_range_rormatting_provider;

    /**
     * The server provides document formatting on typing.
     */
    DocumentOnTypeFormattingOptions document_on_type_formatting_provider;

    /**
     * The server provides rename support. RenameOptions may only be
     * specified if the client states that it supports
     * `prepareSupport` in its initial `initialize` request.
     */
    RenameOptions rename_provider;

    /**
     * The server provides folding provider support.
     *
     * @since 3.10.0
     */
    FoldingRangeRegistrationOptions folding_range_provider;

    /**
     * The server provides execute command support.
     */
    ExecuteCommandOptions execute_command_provider;

    /**
     * The server provides selection range support.
     *
     * @since 3.15.0
     */
    SelectionRangeRegistrationOptions selection_range_provider;

    /**
     * The server provides linked editing range support.
     *
     * @since 3.16.0
     */
    LinkedEditingRangeRegistrationOptions linked_editing_range_provider;

    /**
     * The server provides call hierarchy support.
     *
     * @since 3.16.0
     */
    CallHierarchyRegistrationOptions call_hierarchy_provider;

    /**
     * The server provides semantic tokens support.
     *
     * @since 3.16.0
     */
    SemanticTokensRegistrationOptions semantic_tokens_provider;

    /**
     * Whether server provides moniker support.
     *
     * @since 3.16.0
     */
    MonikerRegistrationOptions moniker_provider;

    /**
     * The server provides type hierarchy support.
     *
     * @since 3.17.0
     */
    TypeHierarchyRegistrationOptions type_hierarchy_provider;

    /**
     * The server provides inline values.
     *
     * @since 3.17.0
     */
    InlineValueRegistrationOptions inline_value_provider;

    /**
     * The server provides inlay hints.
     *
     * @since 3.17.0
     */
    InlayHintRegistrationOptions inlay_hint_provider;

    /**
     * The server has support for pull model diagnostics.
     *
     * @since 3.17.0
     */
    DiagnosticRegistrationOptions diagnostic_provider;

    /**
     * The server provides workspace symbol support.
     */
    WorkspaceSymbolOptions workspace_symbol_provider;

    /**
     * Workspace specific server capabilities
     */
    WorkspaceServer workspace;
} ServerCapabilities;

static ServerCapabilities server_capabilities_parse(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    ServerCapabilities result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }
    
    JSONValue position_encoding = json_object_get(value, "positionEncoding");
    if (position_encoding.type == JSON_VALUE_STRING) {
        result.position_encoding = position_encoding_kind_parse(position_encoding.value.string_value);
    } else {
        result.position_encoding = POSITIONENCODINGKIND_UTF16;
    }

    JSONValue text_document_sync = json_object_get(value, "textDocumentSync");
    if (text_document_sync.type == JSON_VALUE_INT) {
        result.text_document_sync.change = text_document_sync.value.int_value;
    } else if (text_document_sync.type == JSON_VALUE_OBJECT) {
        JSONValue open_close = json_object_get(&text_document_sync, "openClose");
        if (open_close.type == JSON_VALUE_BOOLEAN) {
            result.text_document_sync.open_close = open_close.value.bool_value;
        }

        JSONValue change = json_object_get(&text_document_sync, "change");
        if (change.type == JSON_VALUE_INT) {
            result.text_document_sync.change = change.value.int_value;
        }
    }

    JSONValue* notebook_document_sync = json_object_get_ptr(value, "notebookDocumentSync");
    if (notebook_document_sync != NULL && notebook_document_sync->type == JSON_VALUE_OBJECT) {
        result.notebook_document_sync.static_registration = static_registration_options_parse(notebook_document_sync);

        JSONValue* notebook_selector = json_object_get_ptr(notebook_document_sync, "notebookSelector");
        if (notebook_selector != NULL && notebook_selector->type == JSON_VALUE_ARRAY) {
            size_t length = json_array_length(notebook_selector);
            result.notebook_document_sync.notebook_selector_count = (int)length;
            if (result.notebook_document_sync.notebook_selector_count > 0) {
                NotebookSelector* selectors = (NotebookSelector*)allocator->calloc(length, sizeof(NotebookSelector));
                for (size_t i = 0; i < length; i++) {
                    JSONValue* item = json_array_get_ptr(notebook_selector, i);
                    if (item == NULL || item->type != JSON_VALUE_OBJECT) {
                        continue;
                    }
                    JSONValue* notebook = json_object_get_ptr(item, "notebook");
                    if (notebook != NULL) {
                        if (notebook->type == JSON_VALUE_STRING) {
                            selectors[i].notebook.notebook_type = json_move_string(notebook);
                        } else if (notebook->type == JSON_VALUE_OBJECT) {
                            JSONValue* notebook_type = json_object_get_ptr(notebook, "notebookType");
                            if (notebook_type != NULL && notebook_type->type == JSON_VALUE_STRING) {
                                selectors[i].notebook.notebook_type = json_move_string(notebook_type);
                            }
                            JSONValue* scheme = json_object_get_ptr(notebook, "scheme");
                            if (scheme != NULL && scheme->type == JSON_VALUE_STRING) {
                                selectors[i].notebook.scheme = json_move_string(scheme);
                            }
                            JSONValue* pattern = json_object_get_ptr(notebook, "pattern");
                            if (pattern != NULL && pattern->type == JSON_VALUE_STRING) {
                                selectors[i].notebook.pattern = json_move_string(pattern);
                            }
                        }
                    }

                    JSONValue* cells = json_object_get_ptr(item, "cells");
                    if (cells != NULL && cells->type == JSON_VALUE_ARRAY) {
                        size_t count = json_array_length(cells);
                        selectors[i].cells_count = (int)count;
                        selectors[i].cells = (char**)allocator->malloc(count * sizeof(char*));
                        for (size_t cell_idx = 0; cell_idx < count; cell_idx++) {
                            JSONValue* cell = json_array_get_ptr(cells, cell_idx);
                            JSONValue* language = json_object_get_ptr(cell, "language");
                            selectors[i].cells[cell_idx] = json_move_string(language);
                        }
                    }
                }
                result.notebook_document_sync.notebook_selector = selectors;
            }
        }

        JSONValue save = json_object_get(notebook_document_sync, "save");
        if (save.type == JSON_VALUE_BOOLEAN) {
            result.notebook_document_sync.save = save.value.bool_value;
        }
    }

    JSONValue* completion_provider = json_object_get_ptr(value, "completionProvider");
    if (completion_provider != NULL && completion_provider->type == JSON_VALUE_OBJECT) {
        result.completion_provider.work_done_progress = work_done_progress_parse(completion_provider);

        result.completion_provider.trigger_characters = 
            parse_string_array(completion_provider, "triggerCharacters", &result.completion_provider.trigger_characters_count, allocator);

        result.completion_provider.all_commit_characters =
            parse_string_array(completion_provider, "allCommitCharacters", &result.completion_provider.all_commit_characters_count, allocator);

        JSONValue resolve_provider = json_object_get(completion_provider, "resolveProvider");
        if (resolve_provider.type == JSON_VALUE_BOOLEAN) {
            result.completion_provider.resolve_provider = resolve_provider.value.bool_value;
        }

        JSONValue completion_item = json_object_get(completion_provider, "completionItem");
        if (completion_item.type == JSON_VALUE_OBJECT) {
            JSONValue label_data_support = json_object_get(&completion_item, "labelDataSupport");
            if (label_data_support.type == JSON_VALUE_BOOLEAN) {
                result.completion_provider.completion_item_label_details_support = label_data_support.value.bool_value;
            }
        }
    }

    JSONValue hover_provider = json_object_get(value, "hoverProvider");
    if (hover_provider.type == JSON_VALUE_BOOLEAN) {
        result.hover_provider.is_supported = hover_provider.value.bool_value;
    } else if (hover_provider.type == JSON_VALUE_OBJECT) {
        result.hover_provider.work_done_progress = work_done_progress_parse(&hover_provider);
    }

    JSONValue* signature_help_provider = json_object_get_ptr(value, "signatureHelpProvider");
    if (signature_help_provider != NULL && signature_help_provider->type == JSON_VALUE_OBJECT) {
        result.signature_help_provider.work_done_progress = work_done_progress_parse(signature_help_provider);
        result.signature_help_provider.trigger_characters =
            parse_string_array(signature_help_provider, "triggerCharacters", &result.signature_help_provider.trigger_characters_count, allocator);
        result.signature_help_provider.retrigger_characters =
            parse_string_array(signature_help_provider, "retriggerCharacters", &result.signature_help_provider.retrigger_characters_count, allocator);
    }

    JSONValue* declaration_provider = json_object_get_ptr(value, "declarationProvider");
    if (declaration_provider != NULL) {
        if (declaration_provider->type == JSON_VALUE_BOOLEAN) {
            result.declaration_provider.is_supported = declaration_provider->value.bool_value;
        } else if (declaration_provider->type == JSON_VALUE_OBJECT) {
            result.declaration_provider.is_supported = 1;
            result.declaration_provider.work_done_progress = work_done_progress_parse(declaration_provider);
            result.declaration_provider.static_registration = static_registration_options_parse(declaration_provider);
            result.declaration_provider.text_document_registration = text_document_registration_options_parse(declaration_provider, allocator);
        }
    }

    JSONValue definition_provider = json_object_get(value, "definitionProvider");
    if (definition_provider.type == JSON_VALUE_BOOLEAN) {
        result.definition_provider.is_supported = definition_provider.value.bool_value;
    } else if (definition_provider.type == JSON_VALUE_OBJECT) {
        result.definition_provider.is_supported = 1;
        result.definition_provider.work_done_progress = work_done_progress_parse(&definition_provider);
    }

    JSONValue* type_definition_provider = json_object_get_ptr(value, "typeDefinitionProvider");
    if (type_definition_provider != NULL) {
        if (type_definition_provider->type == JSON_VALUE_BOOLEAN) {
            result.type_definition_provider.is_supported = type_definition_provider->value.bool_value;
        } else if (type_definition_provider->type == JSON_VALUE_OBJECT) {
            result.type_definition_provider.is_supported = 1;
            result.type_definition_provider.work_done_progress = work_done_progress_parse(type_definition_provider);
            result.type_definition_provider.text_document_registration = text_document_registration_options_parse(type_definition_provider, allocator);
            result.type_definition_provider.static_registration = static_registration_options_parse(type_definition_provider);
        }
    }

    JSONValue* implementation_provider = json_object_get_ptr(value, "implementationProvider");
    if (implementation_provider != NULL) {
        if (implementation_provider->type == JSON_VALUE_BOOLEAN) {
            result.implementation_provider.is_supported = implementation_provider->value.bool_value;
        } else if (implementation_provider->type == JSON_VALUE_OBJECT) {
            result.implementation_provider.is_supported = 1;
            result.implementation_provider.work_done_progress = work_done_progress_parse(implementation_provider);
            result.implementation_provider.text_document_registration = text_document_registration_options_parse(implementation_provider, allocator);
            result.implementation_provider.static_registration = static_registration_options_parse(implementation_provider);
        }
    }

    JSONValue references_provider = json_object_get(value, "referencesProvider");
    if (references_provider.type == JSON_VALUE_BOOLEAN) {
        result.references_provider.is_supported = references_provider.value.bool_value;
    } else if (references_provider.type == JSON_VALUE_OBJECT) {
        result.references_provider.is_supported = 1;
        result.references_provider.work_done_progress = work_done_progress_parse(&references_provider);
    }

    JSONValue document_highlight_provider = json_object_get(value, "documentHighlightProvider");
    if (document_highlight_provider.type == JSON_VALUE_BOOLEAN) {
        result.document_highlight_provider.is_supported = document_highlight_provider.value.bool_value;
    } else if (document_highlight_provider.type == JSON_VALUE_OBJECT) {
        result.document_highlight_provider.is_supported = 1;
        result.document_highlight_provider.work_done_progress = work_done_progress_parse(&document_highlight_provider);
    }

    JSONValue* document_symbol_provider = json_object_get_ptr(value, "documentSymbolProvider");
    if (document_symbol_provider != NULL) {
        if (document_symbol_provider->type == JSON_VALUE_BOOLEAN) {
            result.document_symbol_provider.is_supported = document_symbol_provider->value.bool_value;
        } else if (document_symbol_provider->type == JSON_VALUE_OBJECT) {
            result.document_symbol_provider.is_supported = 1;
            result.document_symbol_provider.work_done_progress = work_done_progress_parse(document_symbol_provider);
            
            JSONValue* label = json_object_get_ptr(document_symbol_provider, "label");
            if (label != NULL && label->type == JSON_VALUE_STRING) {
                result.document_symbol_provider.label = json_move_string(label);
            }
        }
    }

    JSONValue code_action_provider = json_object_get(value, "codeActionProvider");
    if (code_action_provider.type == JSON_VALUE_BOOLEAN) {
        result.code_action_provider.is_supported = code_action_provider.value.bool_value;
    } else if (code_action_provider.type == JSON_VALUE_OBJECT) {
        result.code_action_provider.is_supported = 1;
        result.code_action_provider.work_done_progress = work_done_progress_parse(&code_action_provider);

        JSONValue code_action_kinds = json_object_get(&code_action_provider, "codeActionKinds");
        result.code_action_provider.code_action_kinds = code_action_kind_parse(&code_action_kinds);

        JSONValue resolve_provider = json_object_get(&code_action_provider, "resolveProvider");
        if (resolve_provider.type == JSON_VALUE_BOOLEAN) {
            result.code_action_provider.resolve_provider = resolve_provider.value.bool_value;
        }
    }

    JSONValue code_lens_provider = json_object_get(value, "codeLensProvider");
    if (code_lens_provider.type == JSON_VALUE_OBJECT) {
        result.code_lens_provider.work_done_progress = work_done_progress_parse(&code_lens_provider);

        JSONValue resolve_provider = json_object_get(&code_lens_provider, "resolveProvider");
        if (resolve_provider.type == JSON_VALUE_BOOLEAN) {
            result.code_lens_provider.resolve_provider = resolve_provider.value.bool_value;
        }
    }

    JSONValue document_link_provider = json_object_get(value, "documentLinkProvider");
    if (document_link_provider.type == JSON_VALUE_OBJECT) {
        result.document_link_provider.work_done_progress = work_done_progress_parse(&document_link_provider);

        JSONValue resolve_provider = json_object_get(&document_link_provider, "resolveProvider");
        if (resolve_provider.type == JSON_VALUE_BOOLEAN) {
            result.document_link_provider.resolve_provider = resolve_provider.value.bool_value;
        }
    }

    JSONValue* color_provider = json_object_get_ptr(value, "colorProvider");
    if (color_provider != NULL) {
        if (color_provider->type == JSON_VALUE_BOOLEAN) {
            result.color_provider.is_supported = color_provider->value.bool_value;
        } else if (color_provider->type == JSON_VALUE_OBJECT) {
            result.color_provider.is_supported = 1;
            result.color_provider.work_done_progress = work_done_progress_parse(color_provider);
            result.color_provider.text_document_registration = text_document_registration_options_parse(color_provider, allocator);
            result.color_provider.static_registration = static_registration_options_parse(color_provider);
        }
    }

    JSONValue document_formatting_provider = json_object_get(value, "documentFormattingProvider");
    if (document_formatting_provider.type == JSON_VALUE_BOOLEAN) {
        result.document_formatting_provider.is_supported = document_formatting_provider.value.bool_value;
    } else if (document_formatting_provider.type == JSON_VALUE_OBJECT) {
        result.document_formatting_provider.is_supported = 1;
        result.document_formatting_provider.work_done_progress = work_done_progress_parse(&document_formatting_provider);
    }

    JSONValue document_range_formatting_provider = json_object_get(value, "documentRangeFormattingProvider");
    if (document_range_formatting_provider.type == JSON_VALUE_BOOLEAN) {
        result.document_range_rormatting_provider.is_supported = document_range_formatting_provider.value.bool_value;
    } else if (document_range_formatting_provider.type == JSON_VALUE_OBJECT) {
        result.document_range_rormatting_provider.is_supported = 1;
        result.document_range_rormatting_provider.work_done_progress = work_done_progress_parse(&document_range_formatting_provider);
    }

    JSONValue* document_on_type_formatting_provider = json_object_get_ptr(value, "documentOnTypeFormattingProvider");
    if (document_on_type_formatting_provider != NULL && document_on_type_formatting_provider->type == JSON_VALUE_OBJECT) {
        JSONValue* first_trigger_character = json_object_get_ptr(document_on_type_formatting_provider, "firstTriggerCharacter");
        if (first_trigger_character != NULL && first_trigger_character->type == JSON_VALUE_STRING) {
            result.document_on_type_formatting_provider.first_trigger_character = json_move_string(first_trigger_character);
        }

        result.document_on_type_formatting_provider.more_trigger_character =
            parse_string_array(document_on_type_formatting_provider, "moreTriggerCharacter", &result.document_on_type_formatting_provider.more_trigger_character_count, allocator);
    }

    JSONValue rename_provider = json_object_get(value, "renameProvider");
    if (rename_provider.type == JSON_VALUE_BOOLEAN) {
        result.rename_provider.is_supported = rename_provider.value.bool_value;
    } else if (rename_provider.type == JSON_VALUE_OBJECT) {
        result.rename_provider.is_supported = 1;
        result.rename_provider.work_done_progress = work_done_progress_parse(&rename_provider);
        
        JSONValue prepare_provider = json_object_get(&rename_provider, "renameProvider");
        if (prepare_provider.type == JSON_VALUE_BOOLEAN) {
            result.rename_provider.prepare_provider = prepare_provider.value.bool_value;
        }
    }

    JSONValue* folding_range_provider = json_object_get_ptr(value, "foldingRangeProvider");
    if (folding_range_provider != NULL) {
        if (folding_range_provider->type == JSON_VALUE_BOOLEAN) {
            result.folding_range_provider.is_supported = folding_range_provider->value.bool_value;
        } else if (folding_range_provider->type == JSON_VALUE_OBJECT) {
            result.folding_range_provider.is_supported = 1;
            result.folding_range_provider.work_done_progress = work_done_progress_parse(folding_range_provider);
            result.folding_range_provider.text_document_registration = text_document_registration_options_parse(folding_range_provider, allocator);
            result.folding_range_provider.static_registration = static_registration_options_parse(folding_range_provider);
        }
    }

    JSONValue* execute_command_provider = json_object_get_ptr(value, "executeCommandProvider");
    if (execute_command_provider != NULL && execute_command_provider->type == JSON_VALUE_OBJECT) {
        result.execute_command_provider.work_done_progress = work_done_progress_parse(execute_command_provider);
        result.execute_command_provider.commands =
            parse_string_array(execute_command_provider, "commands", &result.execute_command_provider.commands_count, allocator);
    }

    JSONValue* selection_range_provider = json_object_get_ptr(value, "selectionRangeProvider");
    if (selection_range_provider != NULL) {
        if (selection_range_provider->type == JSON_VALUE_BOOLEAN) {
            result.selection_range_provider.is_supported = selection_range_provider->value.bool_value;
        } else if (selection_range_provider->type == JSON_VALUE_OBJECT) {
            result.selection_range_provider.is_supported = 1;
            result.selection_range_provider.work_done_progress = work_done_progress_parse(selection_range_provider);
            result.selection_range_provider.text_document_registration = text_document_registration_options_parse(selection_range_provider, allocator);
            result.selection_range_provider.static_registration = static_registration_options_parse(selection_range_provider);
        }
    }

    JSONValue* linked_editing_range_provider = json_object_get_ptr(value, "linkedEditingRangeProvider");
    if (linked_editing_range_provider != NULL) {
        if (linked_editing_range_provider->type == JSON_VALUE_BOOLEAN) {
            result.linked_editing_range_provider.is_supported = linked_editing_range_provider->value.bool_value;
        } else if (linked_editing_range_provider->type == JSON_VALUE_OBJECT) {
            result.linked_editing_range_provider.is_supported = 1;
            result.linked_editing_range_provider.work_done_progress = work_done_progress_parse(linked_editing_range_provider);
            result.linked_editing_range_provider.text_document_registration = text_document_registration_options_parse(linked_editing_range_provider, allocator);
            result.linked_editing_range_provider.static_registration = static_registration_options_parse(linked_editing_range_provider);
        }
    }

    JSONValue* call_hierarchy_provider = json_object_get_ptr(value, "callHierarchyProvider");
    if (call_hierarchy_provider != NULL) {
        if (call_hierarchy_provider->type == JSON_VALUE_BOOLEAN) {
            result.call_hierarchy_provider.is_supported = call_hierarchy_provider->value.bool_value;
        } else if (call_hierarchy_provider->type == JSON_VALUE_OBJECT) {
            result.call_hierarchy_provider.is_supported = 1;
            result.call_hierarchy_provider.work_done_progress = work_done_progress_parse(call_hierarchy_provider);
            result.call_hierarchy_provider.text_document_registration = text_document_registration_options_parse(call_hierarchy_provider, allocator);
            result.call_hierarchy_provider.static_registration = static_registration_options_parse(call_hierarchy_provider);
        }
    }

    JSONValue* semantic_tokens_provider = json_object_get_ptr(value, "semanticTokensProvider");
    if (semantic_tokens_provider != NULL && semantic_tokens_provider->type == JSON_VALUE_OBJECT) {
        result.semantic_tokens_provider.semantic_tokens.work_done_progress = work_done_progress_parse(semantic_tokens_provider);
        result.semantic_tokens_provider.text_document_registration = text_document_registration_options_parse(semantic_tokens_provider, allocator);
        result.semantic_tokens_provider.static_registration = static_registration_options_parse(semantic_tokens_provider);

        JSONValue* legend = json_object_get_ptr(semantic_tokens_provider, "legend");
        if (legend != NULL && legend->type == JSON_VALUE_OBJECT) {
            result.semantic_tokens_provider.semantic_tokens.legend.token_types =
                parse_string_array(legend, "tokenTypes", &result.semantic_tokens_provider.semantic_tokens.legend.token_types_count, allocator);
            result.semantic_tokens_provider.semantic_tokens.legend.token_modifiers =
                parse_string_array(legend, "tokenModifiers", &result.semantic_tokens_provider.semantic_tokens.legend.token_modifiers_count, allocator);
        }

        JSONValue range = json_object_get(semantic_tokens_provider, "range");
        if (range.type == JSON_VALUE_BOOLEAN) {
            result.semantic_tokens_provider.semantic_tokens.range = range.value.bool_value;
        }

        JSONValue full = json_object_get(semantic_tokens_provider, "full");
        if (full.type == JSON_VALUE_OBJECT) {
            JSONValue delta = json_object_get(&full, "delta");
            if (delta.type == JSON_VALUE_BOOLEAN) {
                result.semantic_tokens_provider.semantic_tokens.full_delta = delta.value.bool_value;
            }
        }
    }

    JSONValue* moniker_provider = json_object_get_ptr(value, "monikerProvider");
    if (moniker_provider != NULL) {
        if (moniker_provider->type == JSON_VALUE_BOOLEAN) {
            result.moniker_provider.is_supported = moniker_provider->value.bool_value;
        } else if (moniker_provider->type == JSON_VALUE_OBJECT) {
            result.moniker_provider.is_supported = 1;
            result.moniker_provider.work_done_progress = work_done_progress_parse(moniker_provider);
            result.moniker_provider.text_document_registration = text_document_registration_options_parse(moniker_provider, allocator);
        }
    }

    JSONValue* type_hierarchy_provider = json_object_get_ptr(value, "typeHierarchyProvider");
    if (type_hierarchy_provider != NULL) {
        if (type_hierarchy_provider->type == JSON_VALUE_BOOLEAN) {
            result.type_hierarchy_provider.is_supported = type_hierarchy_provider->value.bool_value;
        } else if (type_hierarchy_provider->type == JSON_VALUE_OBJECT) {
            result.type_hierarchy_provider.is_supported = 1;
            result.type_hierarchy_provider.work_done_progress = work_done_progress_parse(type_hierarchy_provider);
            result.type_definition_provider.text_document_registration = text_document_registration_options_parse(type_hierarchy_provider, allocator);
            result.type_definition_provider.static_registration = static_registration_options_parse(type_hierarchy_provider);
        }
    }

    JSONValue* inline_value_provider = json_object_get_ptr(value, "inlineValueProvider");
    if (inline_value_provider != NULL) {
        if (inline_value_provider->type == JSON_VALUE_BOOLEAN) {
            result.inline_value_provider.is_supported = inline_value_provider->value.bool_value;
        } else if (inline_value_provider->type == JSON_VALUE_OBJECT) {
            result.inline_value_provider.is_supported = 1;
            result.inline_value_provider.work_done_progress = work_done_progress_parse(inline_value_provider);
            result.inline_value_provider.text_document_registration = text_document_registration_options_parse(inline_value_provider, allocator);
            result.inline_value_provider.static_registration = static_registration_options_parse(inline_value_provider);
        }
    }

    JSONValue* inlay_hint_provider = json_object_get_ptr(value, "inlayHintProvider");
    if (inlay_hint_provider != NULL) {
        if (inlay_hint_provider->type == JSON_VALUE_BOOLEAN) {
            result.inlay_hint_provider.is_supported = inlay_hint_provider->value.bool_value;
        } else if (inlay_hint_provider->type == JSON_VALUE_OBJECT) {
            result.inlay_hint_provider.is_supported = 1;
            result.inlay_hint_provider.work_done_progress = work_done_progress_parse(inlay_hint_provider);
            result.inlay_hint_provider.text_document_registration = text_document_registration_options_parse(inlay_hint_provider, allocator);
            result.inlay_hint_provider.static_registration = static_registration_options_parse(inlay_hint_provider);
            JSONValue resolve_provider = json_object_get(inlay_hint_provider, "resolveProvider");
            result.inlay_hint_provider.resolve_provider = resolve_provider.type == JSON_VALUE_BOOLEAN ? resolve_provider.value.bool_value : lstalk_false;
        }
    }

    JSONValue* diagnostic_provider = json_object_get_ptr(value, "diagnosticProvider");
    if (diagnostic_provider != NULL && diagnostic_provider->type == JSON_VALUE_OBJECT) {
        result.diagnostic_provider.work_done_progress = work_done_progress_parse(diagnostic_provider);
        result.diagnostic_provider.text_document_registration = text_document_registration_options_parse(diagnostic_provider, allocator);
        result.diagnostic_provider.static_registration = static_registration_options_parse(diagnostic_provider);

        JSONValue* identifier = json_object_get_ptr(diagnostic_provider, "identifier");
        if (identifier != NULL && identifier->type == JSON_VALUE_STRING) {
            result.diagnostic_provider.identifier = json_move_string(identifier);
        }

        JSONValue inter_file_dependencies = json_object_get(diagnostic_provider, "interFileDependencies");
        if (inter_file_dependencies.type == JSON_VALUE_BOOLEAN) {
            result.diagnostic_provider.inter_file_dependencies = inter_file_dependencies.value.bool_value;
        }

        JSONValue workspace_diagnostics = json_object_get(diagnostic_provider, "workspaceDiagnostics");
        if (workspace_diagnostics.type == JSON_VALUE_BOOLEAN) {
            result.diagnostic_provider.workspace_diagnostics = workspace_diagnostics.value.bool_value;
        }
    }

    JSONValue workspace_symbol_provider = json_object_get(value, "workspaceSymbolProvider");
    if (workspace_symbol_provider.type == JSON_VALUE_BOOLEAN) {
        result.workspace_symbol_provider.is_supported = workspace_symbol_provider.value.bool_value;
    } else if (workspace_symbol_provider.type == JSON_VALUE_OBJECT) {
        result.workspace_symbol_provider.is_supported = 1;
        result.workspace_symbol_provider.work_done_progress = work_done_progress_parse(&workspace_symbol_provider);

        JSONValue resolve_provider = json_object_get(&workspace_symbol_provider, "resolveProvider");
        if (resolve_provider.type == JSON_VALUE_BOOLEAN) {
            result.workspace_symbol_provider.resolve_provider = resolve_provider.value.bool_value;
        }
    }

    JSONValue* workspace = json_object_get_ptr(value, "workspace");
    if (workspace != NULL && workspace->type == JSON_VALUE_OBJECT) {
        JSONValue* workspace_folders = json_object_get_ptr(workspace, "workspaceFolders");
        if (workspace_folders != NULL && workspace_folders->type == JSON_VALUE_OBJECT) {
            JSONValue supported = json_object_get(workspace_folders, "supported");
            if (supported.type == JSON_VALUE_BOOLEAN) {
                result.workspace.workspace_folders.supported = supported.value.bool_value;
            }

            JSONValue* change_notifications = json_object_get_ptr(workspace_folders, "changeNotifications");
            if (change_notifications != NULL) {
                if (change_notifications->type == JSON_VALUE_BOOLEAN) {
                    result.workspace.workspace_folders.change_notifications_boolean = change_notifications->value.bool_value;
                    result.workspace.workspace_folders.change_notifications = NULL;
                } else if (change_notifications->type == JSON_VALUE_STRING) {
                    result.workspace.workspace_folders.change_notifications_boolean = 1;
                    result.workspace.workspace_folders.change_notifications = json_move_string(change_notifications);
                }
            }
        }

        JSONValue* file_operations = json_object_get_ptr(workspace, "fileOperations");
        if (file_operations != NULL && file_operations->type == JSON_VALUE_OBJECT) {
            result.workspace.file_operations.did_create = file_operation_registration_options_parse(file_operations, "didCreate", allocator);
            result.workspace.file_operations.will_create = file_operation_registration_options_parse(file_operations, "will_create", allocator);
            result.workspace.file_operations.did_rename = file_operation_registration_options_parse(file_operations, "didRename", allocator);
            result.workspace.file_operations.will_rename = file_operation_registration_options_parse(file_operations, "willRename", allocator);
            result.workspace.file_operations.did_delete = file_operation_registration_options_parse(file_operations, "didDelete", allocator);
            result.workspace.file_operations.will_delete = file_operation_registration_options_parse(file_operations, "willDelete", allocator);
        }
    }

    return result;
}

#if LSTALK_TESTS
// This function is currently only used in testing. Add to main library when needed.
static JSONValue server_capabilities_json(ServerCapabilities* capabilities, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_null();

    if (capabilities == NULL) {
        return result;
    }

    #define WORK_TEXT_STATIC_OPTIONS(property, name) \
    { \
        JSONValue value = json_make_object(allocator); \
        work_done_progress_json_set(&property.work_done_progress, &value, allocator); \
        text_document_registration_options_json_set(&property.text_document_registration, &value, allocator); \
        static_registration_options_json_set(&property.static_registration, &value, allocator); \
        json_object_const_key_set(&result, name, value, allocator); \
    }

    result = json_make_object(allocator);

    json_object_const_key_set(&result, "positionEncoding", json_make_int(capabilities->position_encoding), allocator);

    {
        TextDocumentSyncOptions* options = &capabilities->text_document_sync;
        JSONValue value = json_make_object(allocator);
        json_object_const_key_set(&value, "openClose", json_make_boolean(options->open_close), allocator);
        json_object_const_key_set(&value, "change", json_make_int(options->change), allocator);
        json_object_const_key_set(&result, "textDocumentSync", value, allocator);
    }

    {
        NotebookDocumentSyncOptions* options = &capabilities->notebook_document_sync;
        JSONValue value = json_make_object(allocator);
        static_registration_options_json_set(&options->static_registration, &value, allocator);

        JSONValue notebook_selector = json_make_array(allocator);
        for (int i = 0; i < options->notebook_selector_count; i++) {
            NotebookSelector* selector = &options->notebook_selector[i];
            JSONValue item = json_make_object(allocator);

            JSONValue notebook = json_make_object(allocator);
            json_object_const_key_set(&notebook, "notebookType", json_make_string(selector->notebook.notebook_type, allocator), allocator);
            json_object_const_key_set(&notebook, "scheme", json_make_string(selector->notebook.scheme, allocator), allocator);
            json_object_const_key_set(&notebook, "pattern", json_make_string(selector->notebook.pattern, allocator), allocator);
            json_object_const_key_set(&item, "notebook", notebook, allocator);

            JSONValue cells = json_make_array(allocator);
            for (int j = 0; j < selector->cells_count; j++) {
                JSONValue cell = json_make_object(allocator);
                json_object_const_key_set(&cell, "language", json_make_string(selector->cells[j], allocator), allocator);
            }
            json_object_const_key_set(&item, "cells", cells, allocator);
            json_array_push(&notebook_selector, item, allocator);
        }
        json_object_const_key_set(&value, "notebookSelector", notebook_selector, allocator);

        json_object_const_key_set(&value, "save", json_make_boolean(options->save), allocator);
        json_object_const_key_set(&result, "notebookDocumentSync", value, allocator);
    }

    {
        CompletionOptions* options = &capabilities->completion_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "triggerCharacters", json_make_string_array(options->trigger_characters, options->trigger_characters_count, allocator), allocator);
        json_object_const_key_set(&value, "allCommitCharacters", json_make_string_array(options->all_commit_characters, options->all_commit_characters_count, allocator), allocator);
        json_object_const_key_set(&value, "resolveProvider", json_make_boolean(options->resolve_provider), allocator);

        JSONValue completion_item = json_make_object(allocator);
        json_object_const_key_set(&completion_item, "labelDetailsSupport", json_make_boolean(options->completion_item_label_details_support), allocator);
        json_object_const_key_set(&value, "completionItem", completion_item, allocator);
        json_object_const_key_set(&result, "completionProvider", value, allocator);
    }

    json_object_const_key_set(&result, "hoverProvider", work_done_progress_json_make(&capabilities->hover_provider.work_done_progress, allocator), allocator);

    {
        SignatureHelpOptions* options = &capabilities->signature_help_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "triggerCharacters", json_make_string_array(options->trigger_characters, options->trigger_characters_count, allocator), allocator);
        json_object_const_key_set(&value, "retriggerCharacters", json_make_string_array(options->retrigger_characters, options->retrigger_characters_count, allocator), allocator);

        json_object_const_key_set(&result, "signatureHelpProvider", value, allocator);
    }

    WORK_TEXT_STATIC_OPTIONS(capabilities->declaration_provider, "declarationProvider");
    json_object_const_key_set(&result, "definitionProvider", work_done_progress_json_make(&capabilities->definition_provider.work_done_progress, allocator), allocator);
    WORK_TEXT_STATIC_OPTIONS(capabilities->type_definition_provider, "typeDefinitionProvider");
    WORK_TEXT_STATIC_OPTIONS(capabilities->implementation_provider, "implementationProvider");
    json_object_const_key_set(&result, "referencesProvider", work_done_progress_json_make(&capabilities->references_provider.work_done_progress, allocator), allocator);
    json_object_const_key_set(&result, "documentHighlightProvider", work_done_progress_json_make(&capabilities->document_highlight_provider.work_done_progress, allocator), allocator);

    {
        DocumentSymbolOptions* options = &capabilities->document_symbol_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        if (options->label != NULL) {
            json_object_const_key_set(&value, "label", json_make_string(options->label, allocator), allocator);
        }

        json_object_const_key_set(&result, "documentSymbolProvider", value, allocator);
    }

    {
        CodeActionOptions* options = &capabilities->code_action_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "codeActionKinds", code_action_kind_make_array(options->code_action_kinds, allocator), allocator);
        json_object_const_key_set(&value, "resolveProvider", json_make_boolean(options->resolve_provider), allocator);

        json_object_const_key_set(&result, "codeActionProvider", value, allocator);
    }

    {
        CodeLensOptions* options = &capabilities->code_lens_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "resolveProvider", json_make_boolean(options->resolve_provider), allocator);

        json_object_const_key_set(&result, "codeLensProvider", value, allocator);
    }

    {
        DocumentLinkOptions* options = &capabilities->document_link_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "resolveProvider", json_make_boolean(options->resolve_provider), allocator);

        json_object_const_key_set(&result, "documentLinkProvider", value, allocator);
    }

    WORK_TEXT_STATIC_OPTIONS(capabilities->color_provider, "colorProvider");
    json_object_const_key_set(&result, "documentFormattingProvider", work_done_progress_json_make(&capabilities->document_formatting_provider.work_done_progress, allocator), allocator);
    json_object_const_key_set(&result, "documentRangeFormattingProvider", work_done_progress_json_make(&capabilities->document_range_rormatting_provider.work_done_progress, allocator), allocator);

    {
        DocumentOnTypeFormattingOptions* options = &capabilities->document_on_type_formatting_provider;
        JSONValue value = json_make_object(allocator);

        json_object_const_key_set(&value, "firstTriggerCharacter", json_make_string(options->first_trigger_character, allocator), allocator);
        if (options->more_trigger_character_count > 0) {
            json_object_const_key_set(&value, "moreTriggerCharacter", json_make_string_array(options->more_trigger_character, options->more_trigger_character_count, allocator), allocator);
        }

        json_object_const_key_set(&result, "documentOnTypeFormattingProvider", value, allocator);
    }

    {
        RenameOptions* options = &capabilities->rename_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "prepareProvider", json_make_boolean(options->prepare_provider), allocator);

        json_object_const_key_set(&result, "renameProvider", value, allocator);
    }

    WORK_TEXT_STATIC_OPTIONS(capabilities->folding_range_provider, "foldingRangeProvider");

    {
        ExecuteCommandOptions* options = &capabilities->execute_command_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "commands", json_make_string_array(options->commands, options->commands_count, allocator), allocator);

        json_object_const_key_set(&result, "executeCommandProvider", value, allocator);
    }

    WORK_TEXT_STATIC_OPTIONS(capabilities->selection_range_provider, "selectionRangeProvider");
    WORK_TEXT_STATIC_OPTIONS(capabilities->linked_editing_range_provider, "linkedEditingRangeProvider");
    WORK_TEXT_STATIC_OPTIONS(capabilities->call_hierarchy_provider, "callHierarchyProvider");

    {
        SemanticTokensRegistrationOptions* options = &capabilities->semantic_tokens_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->semantic_tokens.work_done_progress, &value, allocator);
        text_document_registration_options_json_set(&options->text_document_registration, &value, allocator);
        static_registration_options_json_set(&options->static_registration, &value, allocator);

        JSONValue legend = json_make_object(allocator);
        json_object_const_key_set(&legend, "tokenTypes", json_make_string_array(options->semantic_tokens.legend.token_types, options->semantic_tokens.legend.token_types_count, allocator), allocator);
        json_object_const_key_set(&legend, "tokenModifiers", json_make_string_array(options->semantic_tokens.legend.token_modifiers, options->semantic_tokens.legend.token_modifiers_count, allocator), allocator);
        json_object_const_key_set(&value, "legend", legend, allocator);
        json_object_const_key_set(&value, "range", json_make_boolean(options->semantic_tokens.range), allocator);
        
        JSONValue full = json_make_object(allocator);
        json_object_const_key_set(&full, "delta", json_make_boolean(options->semantic_tokens.full_delta), allocator);
        json_object_const_key_set(&value, "full", full, allocator);

        json_object_const_key_set(&result, "semanticTokensProvider", value, allocator);
    }

    {
        MonikerRegistrationOptions* options = &capabilities->moniker_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        text_document_registration_options_json_set(&options->text_document_registration, &value, allocator);

        json_object_const_key_set(&result, "monikerProvider", value, allocator);
    }

    WORK_TEXT_STATIC_OPTIONS(capabilities->type_hierarchy_provider, "typeHierarchyProvider");
    WORK_TEXT_STATIC_OPTIONS(capabilities->inline_value_provider, "inlineValueProvider");

    {
        InlayHintRegistrationOptions* options = &capabilities->inlay_hint_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        text_document_registration_options_json_set(&options->text_document_registration, &value, allocator);
        static_registration_options_json_set(&options->static_registration, &value, allocator);
        json_object_const_key_set(&value, "resolveProvider", json_make_boolean(options->resolve_provider), allocator);

        json_object_const_key_set(&result, "inlayHintProvider", value, allocator);
    }

    {
        DiagnosticRegistrationOptions* options = &capabilities->diagnostic_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        text_document_registration_options_json_set(&options->text_document_registration, &value, allocator);
        static_registration_options_json_set(&options->static_registration, &value, allocator);
        if (options->identifier != NULL) {
            json_object_const_key_set(&value, "identifier", json_make_string(options->identifier, allocator), allocator);
        }
        json_object_const_key_set(&value, "interFileDependencies", json_make_boolean(options->inter_file_dependencies), allocator);
        json_object_const_key_set(&value, "workspaceDiagnostics", json_make_boolean(options->workspace_diagnostics), allocator);

        json_object_const_key_set(&result, "diagnosticProvider", value, allocator);
    }

    {
        WorkspaceSymbolOptions* options = &capabilities->workspace_symbol_provider;
        JSONValue value = json_make_object(allocator);

        work_done_progress_json_set(&options->work_done_progress, &value, allocator);
        json_object_const_key_set(&value, "resolveProvider", json_make_boolean(options->resolve_provider), allocator);

        json_object_const_key_set(&result, "workspaceSymbolProvider", value, allocator);
    }

    {
        WorkspaceServer* workspace = &capabilities->workspace;
        JSONValue value = json_make_object(allocator);

        WorkspaceFoldersServerCapabilities* folders = &workspace->workspace_folders;
        JSONValue workspace_folders = json_make_object(allocator);
        json_object_const_key_set(&workspace_folders, "supported", json_make_boolean(folders->supported), allocator);
        if (folders->change_notifications != NULL) {
            json_object_const_key_set(&workspace_folders, "changeNotifications", json_make_string(folders->change_notifications, allocator), allocator);
        } else {
            json_object_const_key_set(&workspace_folders, "changeNotifications", json_make_boolean(folders->change_notifications_boolean), allocator);
        }
        json_object_const_key_set(&value, "workspaceFolders", workspace_folders, allocator);

        FileOperationsServer* operations = &workspace->file_operations;
        JSONValue file_operations = json_make_object(allocator);
        json_object_const_key_set(&file_operations, "didCreate", file_operation_registration_json(&operations->did_create, allocator), allocator);
        json_object_const_key_set(&file_operations, "willCreate", file_operation_registration_json(&operations->will_create, allocator), allocator);
        json_object_const_key_set(&file_operations, "didRename", file_operation_registration_json(&operations->did_rename, allocator), allocator);
        json_object_const_key_set(&file_operations, "willRename", file_operation_registration_json(&operations->will_rename, allocator), allocator);
        json_object_const_key_set(&file_operations, "didDelete", file_operation_registration_json(&operations->did_delete, allocator), allocator);
        json_object_const_key_set(&file_operations, "willDelete", file_operation_registration_json(&operations->will_delete, allocator), allocator);
        json_object_const_key_set(&value, "fileOperations", file_operations, allocator);

        json_object_const_key_set(&result, "workspace", value, allocator);
    }

    return result;
}
#endif

static void server_capabilities_free(ServerCapabilities* capabilities, LSTalk_MemoryAllocator* allocator) {
    for (int i = 0; i < capabilities->notebook_document_sync.notebook_selector_count; i++) {
        NotebookSelector* selector = &capabilities->notebook_document_sync.notebook_selector[i];

        if (selector->notebook.notebook_type != NULL) {
            allocator->free(selector->notebook.notebook_type);
        }

        if (selector->notebook.scheme != NULL) {
            allocator->free(selector->notebook.scheme);
        }

        if (selector->notebook.pattern != NULL) {
            allocator->free(selector->notebook.pattern);
        }

        string_free_array(selector->cells, selector->cells_count, allocator);
    }

    static_registration_options_free(&capabilities->notebook_document_sync.static_registration, allocator);

    if (capabilities->notebook_document_sync.notebook_selector != NULL) {
        allocator->free(capabilities->notebook_document_sync.notebook_selector);
    }

    string_free_array(capabilities->completion_provider.trigger_characters, capabilities->completion_provider.trigger_characters_count, allocator);
    string_free_array(capabilities->completion_provider.all_commit_characters, capabilities->completion_provider.all_commit_characters_count, allocator);
    string_free_array(capabilities->signature_help_provider.trigger_characters, capabilities->signature_help_provider.trigger_characters_count, allocator);
    string_free_array(capabilities->signature_help_provider.retrigger_characters, capabilities->signature_help_provider.retrigger_characters_count, allocator);

    static_registration_options_free(&capabilities->declaration_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->declaration_provider.text_document_registration, allocator);

    static_registration_options_free(&capabilities->type_definition_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->type_definition_provider.text_document_registration, allocator);

    static_registration_options_free(&capabilities->implementation_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->implementation_provider.text_document_registration, allocator);

    if (capabilities->document_symbol_provider.label != NULL) {
        allocator->free(capabilities->document_symbol_provider.label);
    }

    static_registration_options_free(&capabilities->color_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->color_provider.text_document_registration, allocator);

    if (capabilities->document_on_type_formatting_provider.first_trigger_character != NULL) {
        allocator->free(capabilities->document_on_type_formatting_provider.first_trigger_character);
    }

    string_free_array(capabilities->document_on_type_formatting_provider.more_trigger_character, capabilities->document_on_type_formatting_provider.more_trigger_character_count, allocator);

    static_registration_options_free(&capabilities->folding_range_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->folding_range_provider.text_document_registration, allocator);

    string_free_array(capabilities->execute_command_provider.commands, capabilities->execute_command_provider.commands_count, allocator);

    static_registration_options_free(&capabilities->selection_range_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->selection_range_provider.text_document_registration, allocator);

    static_registration_options_free(&capabilities->linked_editing_range_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->linked_editing_range_provider.text_document_registration, allocator);

    static_registration_options_free(&capabilities->call_hierarchy_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->call_hierarchy_provider.text_document_registration, allocator);

    string_free_array(capabilities->semantic_tokens_provider.semantic_tokens.legend.token_types,
        capabilities->semantic_tokens_provider.semantic_tokens.legend.token_types_count, allocator);
    string_free_array(capabilities->semantic_tokens_provider.semantic_tokens.legend.token_modifiers,
        capabilities->semantic_tokens_provider.semantic_tokens.legend.token_modifiers_count, allocator);
    static_registration_options_free(&capabilities->semantic_tokens_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->semantic_tokens_provider.text_document_registration, allocator);

    text_document_registration_options_free(&capabilities->moniker_provider.text_document_registration, allocator);

    static_registration_options_free(&capabilities->type_hierarchy_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->type_hierarchy_provider.text_document_registration, allocator);

    static_registration_options_free(&capabilities->inline_value_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->inline_value_provider.text_document_registration, allocator);

    static_registration_options_free(&capabilities->inlay_hint_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->inlay_hint_provider.text_document_registration, allocator);

    if (capabilities->diagnostic_provider.identifier != NULL) {
        allocator->free(capabilities->diagnostic_provider.identifier);
    }

    static_registration_options_free(&capabilities->diagnostic_provider.static_registration, allocator);
    text_document_registration_options_free(&capabilities->diagnostic_provider.text_document_registration, allocator);

    file_operation_registration_options_free(&capabilities->workspace.file_operations.did_create, allocator);
    file_operation_registration_options_free(&capabilities->workspace.file_operations.will_create, allocator);
    file_operation_registration_options_free(&capabilities->workspace.file_operations.did_rename, allocator);
    file_operation_registration_options_free(&capabilities->workspace.file_operations.will_rename, allocator);
    file_operation_registration_options_free(&capabilities->workspace.file_operations.did_delete, allocator);
    file_operation_registration_options_free(&capabilities->workspace.file_operations.will_delete, allocator);
}

//
// End Server Capabilities
//

//
// Begin Notifications
//

static LSTalk_Position position_parse(JSONValue* value) {
    LSTalk_Position result;
    memset(&result, 0, sizeof(LSTalk_Position));

    if (value == NULL) {
        return result;
    }

    JSONValue line = json_object_get(value, "line");
    if (line.type == JSON_VALUE_INT) {
        result.line = (unsigned int)line.value.int_value;
    }

    JSONValue character = json_object_get(value, "character");
    if (character.type == JSON_VALUE_INT) {
        result.character = (unsigned int)character.value.int_value;
    }

    return result;
}

static JSONValue position_json(LSTalk_Position position, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);

    json_object_const_key_set(&result, "line", json_make_int(position.line), allocator);
    json_object_const_key_set(&result, "character", json_make_int(position.character), allocator);

    return result;
}

static LSTalk_Range range_parse(JSONValue* value) {
    LSTalk_Range result;
    memset(&result, 0, sizeof(LSTalk_Range));

    if (value == NULL) {
        return result;
    }

    JSONValue start = json_object_get(value, "start");
    if (start.type == JSON_VALUE_OBJECT) {
        result.start = position_parse(&start);
    }

    JSONValue end = json_object_get(value, "end");
    if (end.type == JSON_VALUE_OBJECT) {
        result.end = position_parse(&end);
    }

    return result;
}

static JSONValue range_json(LSTalk_Range range, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_object(allocator);

    json_object_const_key_set(&result, "start", position_json(range.start, allocator), allocator);
    json_object_const_key_set(&result, "end", position_json(range.end, allocator), allocator);

    return result;
}

static LSTalk_Location location_parse(JSONValue* value) {
    LSTalk_Location result;
    memset(&result, 0, sizeof(LSTalk_Location));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue* uri = json_object_get_ptr(value, "uri");
    if (uri != NULL && uri->type == JSON_VALUE_STRING) {
        result.uri = json_move_string(uri);
    }

    JSONValue range = json_object_get(value, "range");
    if (range.type == JSON_VALUE_OBJECT) {
        result.range = range_parse(&range);
    }

    return result;
}

MAYBE_UNUSED static JSONValue location_json(LSTalk_Location* location, LSTalk_MemoryAllocator* allocator) {
    if (location == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_object(allocator);

    if (location->uri != NULL) {
        json_object_const_key_set(&result, "uri", json_make_string(location->uri, allocator), allocator);
    }

    json_object_const_key_set(&result, "range", range_json(location->range, allocator), allocator);

    return result;
}

//
// LSTalk_PublishDiagnostics
//

static LSTalk_PublishDiagnostics publish_diagnostics_parse(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    LSTalk_PublishDiagnostics result;
    memset(&result, 0, sizeof(LSTalk_PublishDiagnostics));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue* uri = json_object_get_ptr(value, "uri");
    result.uri = json_move_string(uri);
    
    JSONValue version = json_object_get(value, "version");
    if (version.type == JSON_VALUE_INT) {
        result.version = version.value.int_value;
    }

    JSONValue* diagnostics = json_object_get_ptr(value, "diagnostics");
    if (diagnostics != NULL && diagnostics->type == JSON_VALUE_ARRAY) {
        result.diagnostics_count = (int)json_array_length(diagnostics);
        if (result.diagnostics_count > 0) {
            result.diagnostics = (LSTalk_Diagnostic*)allocator->calloc(result.diagnostics_count, sizeof(LSTalk_Diagnostic));
            for (size_t i = 0; i < (size_t)result.diagnostics_count; i++) {
                JSONValue* item = json_array_get_ptr(diagnostics, i);
                LSTalk_Diagnostic* diagnostic = &result.diagnostics[i];
                
                JSONValue range = json_object_get(item, "range");
                if (range.type == JSON_VALUE_OBJECT) {
                    diagnostic->range = range_parse(&range);
                }

                JSONValue severity = json_object_get(item, "severity");
                if (severity.type == JSON_VALUE_INT) {
                    diagnostic->severity = (LSTalk_DiagnosticSeverity)severity.value.int_value;
                }

                JSONValue* code = json_object_get_ptr(item, "code");
                if (code != NULL) {
                    if (code->type == JSON_VALUE_STRING) {
                        diagnostic->code = json_move_string(code);
                    } else if (code->type == JSON_VALUE_INT) {
                        char buffer[40] = "";
                        sprintf_s(buffer, sizeof(buffer), "%d", code->value.int_value);
                        diagnostic->code = string_alloc_copy(buffer, allocator);
                    }
                }

                JSONValue* code_description = json_object_get_ptr(item, "codeDescription");
                if (code_description != NULL && code_description->type == JSON_VALUE_OBJECT) {
                    JSONValue* href = json_object_get_ptr(code_description, "href");
                    if (href != NULL && href->type == JSON_VALUE_STRING) {
                        diagnostic->code_description.href = json_move_string(href);
                    }
                }

                JSONValue* source = json_object_get_ptr(item, "source");
                if (source != NULL && source->type == JSON_VALUE_STRING) {
                    diagnostic->source = json_move_string(source);
                }

                JSONValue* message = json_object_get_ptr(item, "message");
                if (message != NULL && message->type == JSON_VALUE_STRING) {
                    diagnostic->message = json_move_string(message);
                }

                JSONValue tags = json_object_get(item, "tags");
                if (tags.type == JSON_VALUE_ARRAY) {
                    diagnostic->tags = diagnostic_tags_parse(&tags);
                }

                JSONValue* related_information = json_object_get_ptr(item, "relatedInformation");
                if (related_information != NULL && related_information->type == JSON_VALUE_ARRAY) {
                    diagnostic->related_information_count = (int)json_array_length(related_information);
                    if (diagnostic->related_information_count > 0) {
                        size_t size = sizeof(LSTalk_DiagnosticRelatedInformation) * diagnostic->related_information_count;
                        diagnostic->related_information = (LSTalk_DiagnosticRelatedInformation*)allocator->malloc(size);
                        for (size_t j = 0; j < (size_t)diagnostic->related_information_count; j++) {
                            JSONValue* related_information_item = json_array_get_ptr(related_information, j);
                            LSTalk_DiagnosticRelatedInformation* diagnostic_related_information = &diagnostic->related_information[i];
                            
                            JSONValue* location = json_object_get_ptr(related_information_item, "location");
                            if (location != NULL && location->type == JSON_VALUE_OBJECT) {
                                diagnostic_related_information->location = location_parse(location);
                            }

                            JSONValue* message_str = json_object_get_ptr(related_information_item, "message");
                            if (message_str != NULL && message_str->type == JSON_VALUE_STRING) {
                                diagnostic_related_information->message = json_move_string(message_str);
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

static void publish_diagnostics_free(LSTalk_PublishDiagnostics* publish_diagnostics, LSTalk_MemoryAllocator* allocator) {
    if (publish_diagnostics == NULL) {
        return;
    }

    if (publish_diagnostics->uri != NULL) {
        allocator->free(publish_diagnostics->uri);
    }

    for (int i = 0; i < publish_diagnostics->diagnostics_count; i++) {
        LSTalk_Diagnostic* diagnostics = &publish_diagnostics->diagnostics[i];

        if (diagnostics->code != NULL) {
            allocator->free(diagnostics->code);
        }

        if (diagnostics->code_description.href != NULL) {
            allocator->free(diagnostics->code_description.href);
        }

        if (diagnostics->source != NULL) {
            allocator->free(diagnostics->source);
        }

        if (diagnostics->message != NULL) {
            allocator->free(diagnostics->message);
        }

        for (size_t j = 0; j < (size_t)diagnostics->related_information_count; j++) {
            LSTalk_DiagnosticRelatedInformation* related_information = &diagnostics->related_information[j];

            if (related_information->location.uri != NULL) {
                allocator->free(related_information->location.uri);
            }

            if (related_information->message != NULL) {
                allocator->free(related_information->message);
            }
        }

        if (diagnostics->related_information != NULL) {
            allocator->free(diagnostics->related_information);
        }
    }

    if (publish_diagnostics->diagnostics != NULL) {
        allocator->free(publish_diagnostics->diagnostics);
    }
}

//
// LSTalk_DocumentSymbol
//

static LSTalk_DocumentSymbol document_symbol_parse(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    LSTalk_DocumentSymbol result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue* name = json_object_get_ptr(value, "name");
    if (name != NULL && name->type == JSON_VALUE_STRING) {
        result.name = json_move_string(name);
    }

    JSONValue* detail = json_object_get_ptr(value, "detail");
    if (detail != NULL && detail->type == JSON_VALUE_STRING) {
        result.detail = json_move_string(detail);
    }

    JSONValue kind = json_object_get(value, "kind");
    result.kind = symbol_kind_parse(&kind);

    JSONValue range = json_object_get(value, "range");
    if (range.type == JSON_VALUE_OBJECT) {
        result.range = range_parse(&range);
    }

    JSONValue selection_range = json_object_get(value, "selectionRange");
    if (selection_range.type == JSON_VALUE_OBJECT) {
        result.selection_range = range_parse(&selection_range);
    }

    JSONValue* children = json_object_get_ptr(value, "children");
    if (children != NULL && children->type == JSON_VALUE_ARRAY) {
        result.children_count = (int)json_array_length(children);
        if (result.children_count > 0) {
            result.children = (LSTalk_DocumentSymbol*)allocator->calloc(json_array_length(children), sizeof(LSTalk_DocumentSymbol));
            for (size_t i = 0; i < json_array_length(children); i++) {
                JSONValue* item = json_array_get_ptr(children, i);
                result.children[i] = document_symbol_parse(item, allocator);
            }
        }
    }

    return result;
}

static JSONValue document_symbol_json(LSTalk_DocumentSymbol* document_symbol, LSTalk_MemoryAllocator* allocator) {
    if (document_symbol == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_object(allocator);

    if (document_symbol->name != NULL) {
        json_object_const_key_set(&result, "name", json_make_string(document_symbol->name, allocator), allocator);
    }

    if (document_symbol->detail != NULL) {
        json_object_const_key_set(&result, "detail", json_make_string(document_symbol->detail, allocator), allocator);
    }

    json_object_const_key_set(&result, "kind", json_make_int((int)document_symbol->kind), allocator);
    json_object_const_key_set(&result, "range", range_json(document_symbol->range, allocator), allocator);
    json_object_const_key_set(&result, "selectionRange", range_json(document_symbol->selection_range, allocator), allocator);

    if (document_symbol->children_count > 0) {
        JSONValue children = json_make_array(allocator);
        for (int i = 0; i < document_symbol->children_count; i++) {
            LSTalk_DocumentSymbol* child = &document_symbol->children[i];
            json_array_push(&children, document_symbol_json(child, allocator), allocator);
        }
    }

    return result;
}

static void document_symbol_free(LSTalk_DocumentSymbol* document_symbol, LSTalk_MemoryAllocator* allocator) {
    if (document_symbol == NULL) {
        return;
    }

    if (document_symbol->name != NULL) {
        allocator->free(document_symbol->name);
    }

    if (document_symbol->detail != NULL) {
        allocator->free(document_symbol->detail);
    }

    if (document_symbol->children != NULL) {
        for (int i = 0; i < document_symbol->children_count; i++) {
            document_symbol_free(&document_symbol->children[i], allocator);
        }

        allocator->free(document_symbol->children);
    }
}

//
// LSTalk_DocumentSymbolNotification
//

static LSTalk_DocumentSymbolNotification document_symbol_notification_parse(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    LSTalk_DocumentSymbolNotification result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_ARRAY) {
        return result;
    }

    result.symbols_count = (int)json_array_length(value);
    if (result.symbols_count > 0) {
        result.symbols = (LSTalk_DocumentSymbol*)allocator->calloc(json_array_length(value), sizeof(LSTalk_DocumentSymbol));
        for (size_t i = 0; i < json_array_length(value); i++) {
            JSONValue* item = json_array_get_ptr(value, i);
            result.symbols[i] = document_symbol_parse(item, allocator);
        }
    }

    return result;
}

MAYBE_UNUSED static JSONValue document_symbol_notification_json(LSTalk_DocumentSymbolNotification* notification, LSTalk_MemoryAllocator* allocator) {
    if (notification == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_array(allocator);
    for (int i = 0; i < notification->symbols_count; i++) {
        LSTalk_DocumentSymbol* document_symbol = &notification->symbols[i];
        json_array_push(&result, document_symbol_json(document_symbol, allocator), allocator);
    }

    return result;
}

static void document_symbol_notification_free(LSTalk_DocumentSymbolNotification* notification, LSTalk_MemoryAllocator* allocator) {
    if (notification == NULL) {
        return;
    }

    if (notification->uri != NULL) {
        allocator->free(notification->uri);
    }

    if (notification->symbols != NULL) {
        for (size_t i = 0; i < (size_t)notification->symbols_count; i++) {
            document_symbol_free(&notification->symbols[i], allocator);
        }

        allocator->free(notification->symbols);
    }
}

//
// Semantic Tokens
//

static LSTalk_SemanticTokens semantic_tokens_parse(JSONValue* value, SemanticTokensLegend* legend, LSTalk_MemoryAllocator* allocator) {
    LSTalk_SemanticTokens result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_OBJECT || legend == NULL) {
        return result;
    }

    JSONValue* result_id = json_object_get_ptr(value, "resultId");
    if (result_id != NULL && result_id->type == JSON_VALUE_STRING) {
        result.result_id = json_move_string(result_id);
    }

    JSONValue* data = json_object_get_ptr(value, "data");
    size_t count = json_array_length(data);
    if (count > 0) {
        result.tokens_count = (int)(count / 5);
        result.tokens = allocator->malloc((size_t)result.tokens_count * sizeof(LSTalk_SemanticToken));
        int previous_line = 0;
        int previous_character = 0;
        int index = 0;
        for (size_t i = 0; i < count; i += 5) {
            int line = json_array_get(data, i).value.int_value;
            int character = json_array_get(data, i + 1).value.int_value;
            int length = json_array_get(data, i + 2).value.int_value;
            int token_type = json_array_get(data, i + 3).value.int_value;
            int token_modifiers = json_array_get(data, i + 4).value.int_value;

            if (line > 0) {
                previous_character = 0;
            }

            LSTalk_SemanticToken* token = &result.tokens[index++];
            token->line = previous_line + line;
            token->character = previous_character + character;
            token->length = length;
            token->token_type = token_type < legend->token_types_count ? legend->token_types[token_type] : NULL;

            previous_line += line;
            previous_character += character;

            Vector modifiers = vector_create(sizeof(int), allocator);
            for (int pos = 0; pos < 32; pos++) {
                if ((token_modifiers & (1 << pos)) != 0) {
                    vector_push(&modifiers, &pos, allocator);
                }
            }

            token->token_modifiers_count = (int)modifiers.length;
            token->token_modifiers = (char**)allocator->malloc(sizeof(char*) * modifiers.length);
            for (size_t j = 0; j < modifiers.length; j++) {
                int pos = *(int*)vector_get(&modifiers, j);
                token->token_modifiers[j] = legend->token_modifiers[pos];
            }
            vector_destroy(&modifiers, allocator);
        }
    }

    return result;
}

static void semantic_token_json(LSTalk_SemanticToken* semantic_token, JSONValue* array, SemanticTokensLegend* legend, LSTalk_MemoryAllocator* allocator) {
    if (semantic_token == NULL || array == NULL || array->type != JSON_VALUE_ARRAY || legend == NULL) {
        return;
    }

    json_array_push(array, json_make_int(semantic_token->line), allocator);
    json_array_push(array, json_make_int(semantic_token->character), allocator);
    json_array_push(array, json_make_int(semantic_token->length), allocator);

    int token_type = 0;
    for (int i = 0; i < legend->token_types_count; i++) {
        if (strcmp(semantic_token->token_type, legend->token_types[i]) == 0) {
            token_type = i;
            break;
        }
    }

    json_array_push(array, json_make_int(token_type), allocator);

    int token_modifiers = 0;
    for (int i = 0; i < semantic_token->token_modifiers_count; i++) {
        for (int pos = 0; pos < legend->token_modifiers_count; pos++) {
            if (strcmp(semantic_token->token_modifiers[i], legend->token_modifiers[pos]) == 0) {
                token_modifiers |= 1 << pos;
                break;
            }
        }
    }

    json_array_push(array, json_make_int(token_modifiers), allocator);
}

MAYBE_UNUSED static JSONValue semantic_tokens_json(LSTalk_SemanticTokens* semantic_tokens, SemanticTokensLegend* legend, LSTalk_MemoryAllocator* allocator) {
    if (semantic_tokens == NULL || legend == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_object(allocator);

    json_object_const_key_set(&result, "resultId", json_make_string(semantic_tokens->result_id, allocator), allocator);

    JSONValue tokens = json_make_array(allocator);
    for (int i = 0; i < semantic_tokens->tokens_count; i++) {
        semantic_token_json(&semantic_tokens->tokens[i], &tokens, legend, allocator);
    }

    json_object_const_key_set(&result, "data", tokens, allocator);

    return result;
}

static void semantic_tokens_free(LSTalk_SemanticTokens* semantic_tokens, LSTalk_MemoryAllocator* allocator) {
    if (semantic_tokens == NULL) {
        return;
    }

    if (semantic_tokens->result_id != NULL) {
        allocator->free(semantic_tokens->result_id);
    }

    if (semantic_tokens->tokens != NULL) {
        for (int i = 0; i < semantic_tokens->tokens_count; i++) {
            if (semantic_tokens->tokens[i].token_modifiers != NULL) {
                allocator->free(semantic_tokens->tokens[i].token_modifiers);
            }
        }
        allocator->free(semantic_tokens->tokens);
    }
}

static LSTalk_Hover hover_parse(JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    LSTalk_Hover result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue* contents = json_object_get_ptr(value, "contents");
    if (contents != NULL) {
        if (contents->type == JSON_VALUE_STRING) {
            result.contents = json_move_string(contents);
        } else if (contents->type == JSON_VALUE_OBJECT) {
            JSONValue kind = json_object_get(contents, "kind");
            if (kind.type == JSON_VALUE_STRING) {
                JSONValue* text = json_object_get_ptr(contents, "value");
                if (text != NULL && text->type == JSON_VALUE_STRING) {
                    result.contents = json_unescape_string(text->value.string_value, allocator);
                }
            }

            JSONValue* language = json_object_get_ptr(contents, "language");
            if (language != NULL && language->type == JSON_VALUE_STRING) {
                JSONValue* text = json_object_get_ptr(contents, "value");
                if (text != NULL && text->type == JSON_VALUE_STRING) {
                    result.contents = json_move_string(text);
                }
            }
        } else if (contents->type == JSON_VALUE_ARRAY) {
            printf("TODO: Handle MarkedString array\n");
        }
    }

    JSONValue range = json_object_get(value, "range");
    result.range = range_parse(&range);

    return result;
}

static JSONValue hover_json(LSTalk_Hover* hover, LSTalk_MemoryAllocator* allocator) {
    if (hover == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_object(allocator);

    if (hover->uri != NULL) {
        json_object_const_key_set(&result, "uri", json_make_string(hover->uri, allocator), allocator);
    }

    if (hover->contents != NULL) {
        json_object_const_key_set(&result, "contents", json_make_string(hover->contents, allocator), allocator);
    }

    json_object_const_key_set(&result, "range", range_json(hover->range, allocator), allocator);

    return result;
}

static void hover_free(LSTalk_Hover* hover, LSTalk_MemoryAllocator* allocator) {
    if (hover == NULL) {
        return;
    }

    if (hover->uri != NULL) {
        allocator->free(hover->uri);
    }

    if (hover->contents != NULL) {
        allocator->free(hover->contents);
    }
}

static LSTalk_Log log_parse(JSONValue* value) {
    LSTalk_Log result;
    memset(&result, 0, sizeof(result));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue* message = json_object_get_ptr(value, "message");
    if (message != NULL) {
        result.message = json_move_string(message);
    }

    JSONValue* verbose = json_object_get_ptr(value, "verbose");
    if (verbose != NULL) {
        result.verbose = json_move_string(verbose);
    }

    return result;
}

static void log_free(LSTalk_Log* log, LSTalk_MemoryAllocator* allocator) {
    if (log == NULL) {
        return;
    }

    if (log->message != NULL) {
        allocator->free(log->message);
    }

    if (log->verbose != NULL) {
        allocator->free(log->verbose);
    }
}

static LSTalk_Notification notification_make(LSTalk_NotificationType type) {
    LSTalk_Notification result;
    memset(&result, 0, sizeof(LSTalk_Notification));
    result.type = type;
    return result;
}

static void notification_free(LSTalk_Notification* notification, LSTalk_MemoryAllocator* allocator) {
    if (notification == NULL) {
        return;
    }

    switch (notification->type) {
        case LSTALK_NOTIFICATION_TEXT_DOCUMENT_SYMBOLS: {
            document_symbol_notification_free(&notification->data.document_symbols, allocator);
            break;
        }

        case LSTALK_NOTIFICATION_PUBLISHDIAGNOSTICS: {
            publish_diagnostics_free(&notification->data.publish_diagnostics, allocator);
            break;
        }

        case LSTALK_NOTIFICATION_SEMANTIC_TOKENS: {
            semantic_tokens_free(&notification->data.semantic_tokens, allocator);
            break;
        }

        case LSTALK_NOTIFICATION_HOVER: {
            hover_free(&notification->data.hover, allocator);
            break;
        }

        case LSTALK_NOTIFICATION_LOG: {
            log_free(&notification->data.log, allocator);
            break;
        }

        case LSTALK_NOTIFICATION_NONE:
        default: break;
    }
}

//
// End Notifications
//

//
// Server
//

typedef struct TextDocumentItem {
    /**
     * The text document's URI.
     */
    char* uri;

    /**
     * The text document's language identifier.
     */
    char* language_id;

    /**
     * The version number of this document (it will increase after each
     * change, including undo/redo).
     */
    int version;

    /**
     * The content of the opened text document.
     */
    char* text;
} TextDocumentItem;

static void text_document_item_free(TextDocumentItem* item, LSTalk_MemoryAllocator* allocator) {
    if (item == NULL) {
        return;
    }

    if (item->uri != NULL) {
        allocator->free(item->uri);
    }

    if (item->language_id != NULL) {
        allocator->free(item->language_id);
    }

    if (item->text != NULL) {
        allocator->free(item->text);
    }
}

typedef struct Message {
    char* buffer;
    size_t length;
    size_t expected_length;
} Message;

static Message message_create() {
    Message message;
    memset(&message, 0, sizeof(message));
    return message;
}

static void message_free(Message* message, LSTalk_MemoryAllocator* allocator) {
    if (message == NULL) {
        return;
    }

    if (message->buffer != NULL) {
        allocator->free(message->buffer);
    }

    memset(message, 0, sizeof(Message));
}

static int message_has_pending(Message* message) {
    if (message == NULL) {
        return 0;
    }
    
    return message->buffer != NULL && message->expected_length > 0;
}

static JSONValue message_to_json(Message* message, char** request, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_null();

    if (message == NULL || request == NULL) {
        return result;
    }

    char* anchor = *request;
    size_t length = strlen(anchor);

    // Pending message already exists.
    if (message_has_pending(message)) {
        if (message->length + length < message->expected_length) {
            // Still not enough data. Store the rest of this response into the current
            // pending buffer and continue waiting.
            size_t size = message->length + length;
            message->buffer = (char*)allocator->realloc(message->buffer, size + 1);
            strncpy_s(message->buffer + message->length, size + 1, anchor, length);
            message->buffer[size] = 0;
            message->length = size;
            *request = NULL;
        } else {
            // The data has arrived. Parse the contents into a JSON object.
            size_t remaining = message->expected_length - message->length;
            size_t size = message->expected_length + 1;
            char* content = (char*)allocator->malloc(sizeof(char) * size);
            strncpy_s(content, size, message->buffer, message->length);
            strncpy_s(content + message->length, size - message->length, anchor, remaining);
            content[message->expected_length] = 0;
            result = json_decode(content, allocator);
            *request += remaining + 1;
            allocator->free(content);
            message_free(message, allocator);
        }
    } else {
        size_t content_length = 0;
        char* content_length_str = NULL;
        if (message->expected_length > 0) {
            content_length = message->expected_length;
            content_length_str = anchor;
        } else {
            // Find the next response to parse.
            content_length_str = strstr(anchor, "Content-Length");
            if (content_length_str == NULL) {
                *request = NULL;
                return result;
            }

            // TODO: Make sure the full line has been received before attempting to read length.

            // Retrieve the length of the response.
            sscanf_s(content_length_str, "Content-Length: %zu", &content_length);
        }

        // Find the start of the JSON string.
        char* content_start = strchr(content_length_str, '{');
        if (content_start != NULL) {
            size_t remaining = strlen(content_start);
            if (remaining < content_length) {
                // The full content string is not part of this read. Store for the
                // next read.
                message->expected_length = content_length;
                message->buffer = string_alloc_copy(content_start, allocator);
                message->length = strlen(content_start);
                *request = NULL;
            } else {
                // The full content is available. Decode the response into a JSON object.
                char* content = (char*)allocator->malloc(sizeof(char) * content_length + 1);
                strncpy_s(content, content_length + 1, content_start, content_length);
                content[content_length] = 0;
                result = json_decode(content, allocator);
                allocator->free(content);
                *request = content_start + content_length;
                message_free(message, allocator);
            }
        } else {
            // The length may have been parsed, but the content hasn't been parsed.
            // Store the length to be used on the next read.
            if (content_length > 0) {
                message->expected_length = content_length;
            }

            *request = NULL;
        }
    }

    return result;
}

typedef struct Server {
    LSTalk_ServerID id;
    Process* process;
    LSTalk_ConnectionStatus connection_status;
    Vector requests;
    int request_id;
    LSTalk_ServerInfo info;
    ServerCapabilities capabilities;
    Vector text_documents;
    Vector notifications;
    Message pending_message;
} Server;

static LSTalk_ServerInfo server_info_parse(JSONValue* value) {
    LSTalk_ServerInfo info;
    memset(&info, 0, sizeof(info));

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return info;
    }

    JSONValue* name = json_object_get_ptr(value, "name");
    if (name != NULL && name->type == JSON_VALUE_STRING) {
        info.name = json_move_string(name);
    }

    JSONValue* version = json_object_get_ptr(value, "version");
    if (version != NULL && version->type == JSON_VALUE_STRING) {
        info.version = json_move_string(version);
    }

    return info;
}

static void server_initialized_parse(Server* server, JSONValue* value, LSTalk_MemoryAllocator* allocator) {
    if (server == NULL || value == NULL || value->type != JSON_VALUE_OBJECT) {
        return;
    }

    JSONValue* result = json_object_get_ptr(value, "result");
    if (result != NULL && result->type == JSON_VALUE_OBJECT) {
        JSONValue* capabilities = json_object_get_ptr(result, "capabilities");
        server->capabilities = server_capabilities_parse(capabilities, allocator);

        JSONValue* server_info = json_object_get_ptr(result, "serverInfo");
        server->info = server_info_parse(server_info);
    }
}

static void server_send_request(Server* server, Request* request, int debug_flags, LSTalk_MemoryAllocator* allocator) {
    rpc_send_request(server->process, request, debug_flags & LSTALK_DEBUGFLAGS_PRINT_REQUESTS, allocator);
}

static void server_close(Server* server, LSTalk_MemoryAllocator* allocator) {
    if (server == NULL) {
        return;
    }

    process_close(server->process, allocator);

    for (size_t i = 0; i < server->requests.length; i++) {
        Request* request = (Request*)vector_get(&server->requests, i);
        rpc_close_request(request, allocator);
    }
    vector_destroy(&server->requests, allocator);

    if (server->info.name != NULL) {
        allocator->free(server->info.name);
    }

    if (server->info.version != NULL) {
        allocator->free(server->info.version);
    }

    server_capabilities_free(&server->capabilities, allocator);

    for (size_t i = 0; i < server->text_documents.length; i++) {
        TextDocumentItem* item = (TextDocumentItem*)vector_get(&server->text_documents, i);
        text_document_item_free(item, allocator);
    }
    vector_destroy(&server->text_documents, allocator);

    for (size_t i = 0; i < server->notifications.length; i++) {
        LSTalk_Notification* notification = (LSTalk_Notification*)vector_get(&server->notifications, i);
        notification_free(notification, allocator);
    }
    vector_destroy(&server->notifications, allocator);

    message_free(&server->pending_message, allocator);
}

static int server_has_text_document(Server* server, const char* uri) {
    if (server == NULL) {
        return 0;
    }

    for (size_t i = 0; i < server->text_documents.length; i++) {
        TextDocumentItem* item = (TextDocumentItem*)vector_get(&server->text_documents, i);
        if (strcmp(item->uri, uri) == 0) {
            return 1;
        }
    }

    return 0;
}

typedef struct ClientInfo {
    char* name;
    char* version;
} ClientInfo;

static void client_info_clear(ClientInfo* info, LSTalk_MemoryAllocator* allocator) {
    if (info == NULL) {
        return;
    }

    if (info->name != NULL) {
        allocator->free(info->name);
    }

    if (info->version != NULL) {
        allocator->free(info->version);
    }
}

static JSONValue client_info(ClientInfo* info, LSTalk_MemoryAllocator* allocator) {
    if (info == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_object(allocator);
    json_object_const_key_set(&result, "name", json_make_string_const(info->name), allocator);
    json_object_const_key_set(&result, "version", json_make_string_const(info->version), allocator);
    return result;
}

//
// LSTalk_Context
//

typedef struct LSTalk_Context {
    Vector servers;
    LSTalk_ServerID server_id;
    ClientInfo client_info;
    char* locale;
    ClientCapabilities client_capabilities;
    int debug_flags;
    LSTalk_MemoryAllocator allocator;
} LSTalk_Context;

static void server_make_and_send_notification(LSTalk_Context* context, Server* server, char* method, JSONValue params) {
    Request request = rpc_make_notification_request(method, params, &context->allocator);
    server_send_request(server, &request, context->debug_flags, &context->allocator);
    rpc_close_request(&request, &context->allocator);
}

static void server_make_and_send_request(LSTalk_Context* context, Server* server, char* method, JSONValue params) {
    Request request = rpc_make_request(&server->request_id, method, params, &context->allocator);
    server_send_request(server, &request, context->debug_flags, &context->allocator);
    vector_push(&server->requests, &request, &context->allocator);
}

static Server* context_get_server(LSTalk_Context* context, LSTalk_ServerID id) {
    if (context == NULL || id == LSTALK_INVALID_SERVER_ID) {
        return NULL;
    }

    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);
        if (server->id == id) {
            return server;
        }
    }

    return NULL;
}

//
// lstalk API
//
// This is the beginning of the exposed API functions for the library.

LSTalk_Context* lstalk_init() {
    LSTalk_MemoryAllocator allocator;
    memset(&allocator, 0, sizeof(allocator));
    return lstalk_init_with_allocator(allocator);
}

LSTalk_Context* lstalk_init_with_allocator(LSTalk_MemoryAllocator allocator) {
    allocator.malloc = allocator.malloc != NULL ? allocator.malloc : malloc;
    allocator.calloc = allocator.calloc != NULL ? allocator.calloc : calloc;
    allocator.realloc = allocator.realloc != NULL ? allocator.realloc : realloc;
    allocator.free = allocator.free != NULL ? allocator.free : free;
    LSTalk_Context* result = (LSTalk_Context*)allocator.malloc(sizeof(LSTalk_Context));
    result->servers = vector_create(sizeof(Server), &allocator);
    result->server_id = 1;
    char buffer[40];
    sprintf_s(buffer, sizeof(buffer), "%d.%d.%d", LSTALK_MAJOR, LSTALK_MINOR, LSTALK_REVISION);
    result->client_info.name = string_alloc_copy("lstalk", &allocator);
    result->client_info.version = string_alloc_copy(buffer, &allocator);
    result->locale = string_alloc_copy("en", &allocator);
    memset(&result->client_capabilities, 0, sizeof(result->client_capabilities));
    result->debug_flags = LSTALK_DEBUGFLAGS_NONE;
    result->allocator = allocator;
    return result;
}

void lstalk_shutdown(LSTalk_Context* context) {
    if (context == NULL) {
        return;
    }

    // Close all connected servers.
    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);
        server_close(server, &context->allocator);
    }
    vector_destroy(&context->servers, &context->allocator);

    client_info_clear(&context->client_info, &context->allocator);
    if (context->locale != NULL) {
        context->allocator.free(context->locale);
    }
    context->allocator.free(context);
}

void lstalk_version(int* major, int* minor, int* revision) {
    if (major != NULL) {
        *major = LSTALK_MAJOR;
    }

    if (minor != NULL) {
        *minor = LSTALK_MINOR;
    }

    if (revision != NULL) {
        *revision = LSTALK_REVISION;
    }
}

void lstalk_set_client_info(LSTalk_Context* context, const char* name, const char* version) {
    if (context == NULL) {
        return;
    }

    client_info_clear(&context->client_info, &context->allocator);

    if (name != NULL) {
        context->client_info.name = string_alloc_copy(name, &context->allocator);
    }

    if (version != NULL) {
        context->client_info.version = string_alloc_copy(version, &context->allocator);
    }
}

void lstalk_set_locale(LSTalk_Context* context, const char* locale) {
    if (context == NULL) {
        return;
    }

    if (context->locale != NULL) {
        context->allocator.free(context->locale);
    }

    context->locale = string_alloc_copy(locale, &context->allocator);
}

void lstalk_set_debug_flags(LSTalk_Context* context, int flags) {
    if (context == NULL) {
        return;
    }

    context->debug_flags = flags;
}

LSTalk_ServerID lstalk_connect(LSTalk_Context* context, const char* uri, LSTalk_ConnectParams* connect_params) {
    if (context == NULL || uri == NULL || connect_params == NULL) {
        return LSTALK_INVALID_SERVER_ID;
    }

    Server server;
    memset(&server, 0, sizeof(server));
    server.process = process_create(uri, connect_params->seek_path_env, &context->allocator);
    if (server.process == NULL) {
        return LSTALK_INVALID_SERVER_ID;
    }

    server.id = context->server_id++;
    server.request_id = 1;
    server.requests = vector_create(sizeof(Request), &context->allocator);
    memset(&server.info, 0, sizeof(server.info));
    server.text_documents = vector_create(sizeof(TextDocumentItem), &context->allocator);
    server.notifications = vector_create(sizeof(LSTalk_Notification), &context->allocator);
    server.pending_message = message_create();

    JSONValue params = json_make_object(&context->allocator);
    json_object_const_key_set(&params, "processId", json_make_int(process_get_current_id()), &context->allocator);
    json_object_const_key_set(&params, "clientInfo", client_info(&context->client_info, &context->allocator), &context->allocator);
    json_object_const_key_set(&params, "locale", json_make_string_const(context->locale), &context->allocator);
    json_object_const_key_set(&params, "rootUri", json_make_string(connect_params->root_uri, &context->allocator), &context->allocator);
    json_object_const_key_set(&params, "clientCapabilities", client_capabilities_make(&context->client_capabilities, &context->allocator), &context->allocator);
    json_object_const_key_set(&params, "trace", json_make_string_const(trace_to_string(connect_params->trace)), &context->allocator);

    server_make_and_send_request(context, &server, "initialize", params);
    server.connection_status = LSTALK_CONNECTION_STATUS_CONNECTING;
    vector_push(&context->servers, &server, &context->allocator);
    return server.id;
}

LSTalk_ConnectionStatus lstalk_get_connection_status(LSTalk_Context* context, LSTalk_ServerID id) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return LSTALK_CONNECTION_STATUS_NOT_CONNECTED;
    }

    return server->connection_status;
}

LSTalk_ServerInfo* lstalk_get_server_info(LSTalk_Context* context, LSTalk_ServerID id) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return NULL;
    }

    return &server->info;
}

int lstalk_close(LSTalk_Context* context, LSTalk_ServerID id) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    server_make_and_send_request(context, server, "shutdown", json_make_null());
    return 1;
}

int lstalk_process_responses(LSTalk_Context* context) {
    if (context == NULL) {
        return 0;
    }

    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);
        char* response = process_read(server->process, &context->allocator);

        if (response != NULL) {
            if (context->debug_flags & LSTALK_DEBUGFLAGS_PRINT_RESPONSES) {
                printf("Response: %s\n", response);
            }

            char* anchor = response;
            while (anchor != NULL) {
                JSONValue value = message_to_json(&server->pending_message, &anchor, &context->allocator);

                if (value.type == JSON_VALUE_OBJECT) {
                    JSONValue id = json_object_get(&value, "id");

                    // Find the associated request for this response.
                    for (size_t request_index = 0; request_index < server->requests.length; request_index++) {
                        Request* request = (Request*)vector_get(&server->requests, request_index);
                        if (request->id == id.value.int_value) {
                            int remove_request = 1;
                            char* method = rpc_get_method(request);
                            JSONValue* result = json_object_get_ptr(&value, "result");
                            if (strcmp(method, "initialize") == 0) {
                                server->connection_status = LSTALK_CONNECTION_STATUS_CONNECTED;
                                server_initialized_parse(server, &value, &context->allocator);
                                server_make_and_send_notification(context, server, "initialized", json_make_null());
                            } else if (strcmp(method, "shutdown") == 0) {
                                server_make_and_send_notification(context, server, "exit", json_make_null());
                                server_close(server, &context->allocator);
                                vector_remove(&context->servers, i);
                                i--;
                                remove_request = 0;
                            } else if (strcmp(method, "textDocument/documentSymbol") == 0) {
                                LSTalk_Notification notification = notification_make(LSTALK_NOTIFICATION_TEXT_DOCUMENT_SYMBOLS);
                                notification.data.document_symbols = document_symbol_notification_parse(result, &context->allocator);
                                JSONValue params = json_object_get(&request->payload, "params");
                                JSONValue text_document = json_object_get(&params, "textDocument");
                                notification.data.document_symbols.uri = json_unescape_string(json_object_get(&text_document, "uri").value.string_value, &context->allocator);
                                vector_push(&server->notifications, &notification, &context->allocator);
                            } else if (strcmp(method, "textDocument/semanticTokens/full") == 0) {
                                LSTalk_Notification notification = notification_make(LSTALK_NOTIFICATION_SEMANTIC_TOKENS);
                                notification.data.semantic_tokens = semantic_tokens_parse(result, &server->capabilities.semantic_tokens_provider.semantic_tokens.legend, &context->allocator);
                                vector_push(&server->notifications, &notification, &context->allocator);
                            } else if (strcmp(method, "textDocument/hover") == 0) {
                                LSTalk_Notification notification = notification_make(LSTALK_NOTIFICATION_HOVER);
                                notification.data.hover = hover_parse(result, &context->allocator);
                                JSONValue params = json_object_get(&request->payload, "params");
                                JSONValue text_document = json_object_get(&params, "textDocument");
                                notification.data.hover.uri = json_unescape_string(json_object_get(&text_document, "uri").value.string_value, &context->allocator);
                                vector_push(&server->notifications, &notification, &context->allocator);
                            }

                            if (remove_request) {
                                rpc_close_request(request, &context->allocator);
                                vector_remove(&server->requests, request_index);
                                request_index--;
                            }
                            break;
                        }
                    }

                    JSONValue method = json_object_get(&value, "method");
                    // This area is to handle notifications. These are sent from the server unprompted.
                    if (method.type == JSON_VALUE_STRING) {
                        char* method_str = method.value.string_value;
                        JSONValue* params = json_object_get_ptr(&value, "params");
                        if (strcmp(method_str, "textDocument/publishDiagnostics") == 0) {
                            LSTalk_Notification notification = notification_make(LSTALK_NOTIFICATION_PUBLISHDIAGNOSTICS);
                            notification.data.publish_diagnostics = publish_diagnostics_parse(params, &context->allocator);
                            vector_push(&server->notifications, &notification, &context->allocator);
                        } else if (strcmp(method_str, "$/logTrace") == 0) {
                            LSTalk_Notification notification = notification_make(LSTALK_NOTIFICATION_LOG);
                            notification.data.log = log_parse(params);
                            vector_push(&server->notifications, &notification, &context->allocator);
                        }
                    }

                    json_destroy_value(&value, &context->allocator);
                }
            }
            context->allocator.free(response);
        }

        for (size_t notify_index = 0; notify_index < server->notifications.length; notify_index++) {
            LSTalk_Notification* notification = (LSTalk_Notification*)vector_get(&server->notifications, notify_index);
            if (notification->polled) {
                notification_free(notification, &context->allocator);
                vector_remove(&server->notifications, notify_index);
                notify_index--;
            }
        }
    }

    return 1;
}

int lstalk_poll_notification(LSTalk_Context* context, LSTalk_ServerID id, LSTalk_Notification* notification) {
    if (notification == NULL) {
        return 0;
    }

    memset(notification, 0, sizeof(*notification));

    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    for (size_t i = 0; i < server->notifications.length; i++) {
        LSTalk_Notification* item = (LSTalk_Notification*)vector_get(&server->notifications, i);
        if (!item->polled) {
            *notification = *item;
            item->polled = 1;
            return 1;
        }
    }

    return 0;
}

int lstalk_set_trace(LSTalk_Context* context, LSTalk_ServerID id, LSTalk_Trace trace) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    JSONValue params = json_make_object(&context->allocator);
    json_object_const_key_set(&params, "value", json_make_string_const(trace_to_string(trace)), &context->allocator);
    server_make_and_send_notification(context, server, "$/setTrace", params);
    return 1;
}

int lstalk_set_trace_from_string(LSTalk_Context* context, LSTalk_ServerID id, const char* trace) {
    return lstalk_set_trace(context, id, trace_from_string(trace));
}

int lstalk_text_document_did_open(LSTalk_Context* context, LSTalk_ServerID id, const char* path) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    char* uri = file_uri(path, &context->allocator);
    TextDocumentItem item;
    item.uri = json_escape_string(uri, &context->allocator);

    if (server_has_text_document(server, item.uri)) {
        context->allocator.free(uri);
        context->allocator.free(item.uri);
        return 1;
    }

    char* contents = file_get_contents(path, &context->allocator);
    if (contents == NULL) {
        return 0;
    }

    item.language_id = file_extension(path, &context->allocator);
    item.version = 1;
    item.text = json_escape_string(contents, &context->allocator);
    context->allocator.free(uri);
    context->allocator.free(contents);

    JSONValue text_document = json_make_object(&context->allocator);
    json_object_const_key_set(&text_document, "uri", json_make_string_const(item.uri), &context->allocator);
    json_object_const_key_set(&text_document, "languageId", json_make_string_const(item.language_id), &context->allocator);
    json_object_const_key_set(&text_document, "version", json_make_int(item.version), &context->allocator);
    json_object_const_key_set(&text_document, "text", json_make_string_const(item.text), &context->allocator);

    JSONValue params = json_make_object(&context->allocator);
    json_object_const_key_set(&params, "textDocument", text_document, &context->allocator);

    server_make_and_send_notification(context, server, "textDocument/didOpen", params);
    vector_push(&server->text_documents, &item, &context->allocator);
    return 1;
}

int lstalk_text_document_did_close(LSTalk_Context* context, LSTalk_ServerID id, const char* path) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    char* uri = file_uri(path, &context->allocator);
    char* escaped_uri = json_escape_string(uri, &context->allocator);

    for (size_t i = 0; i < server->text_documents.length; i++) {
        TextDocumentItem* item = (TextDocumentItem*)vector_get(&server->text_documents, i);
        if (strcmp(item->uri, escaped_uri) == 0) {
            text_document_item_free(item, &context->allocator);
            vector_remove(&server->text_documents, i);
            break;
        }
    }

    JSONValue text_document_identifier = json_make_object(&context->allocator);
    json_object_const_key_set(&text_document_identifier, "uri", json_make_owned_string(escaped_uri), &context->allocator);
    context->allocator.free(uri);

    JSONValue params = json_make_object(&context->allocator);
    json_object_const_key_set(&params, "textDocument", text_document_identifier, &context->allocator);

    server_make_and_send_notification(context, server, "textDocument/didClose", params);
    return 1;
}

static JSONValue text_document_identifier_json(const char* path, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_null();
    if (path == NULL) {
        return result;
    }

    char* uri = file_uri(path, allocator);
    JSONValue text_document_identifier = json_make_object(allocator);
    json_object_const_key_set(&text_document_identifier, "uri", json_make_owned_string(json_escape_string(uri, allocator)), allocator);
    allocator->free(uri);

    result = json_make_object(allocator);
    json_object_const_key_set(&result, "textDocument", text_document_identifier, allocator);

    return result;
}

int lstalk_text_document_symbol(LSTalk_Context* context, LSTalk_ServerID id, const char* path) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    server_make_and_send_request(context, server, "textDocument/documentSymbol", text_document_identifier_json(path, &context->allocator));
    return 1;
}

int lstalk_text_document_semantic_tokens(LSTalk_Context* context, LSTalk_ServerID id, const char* path) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    server_make_and_send_request(context, server, "textDocument/semanticTokens/full", text_document_identifier_json(path, &context->allocator));
    return 1;
}

int lstalk_text_document_hover(LSTalk_Context* context, LSTalk_ServerID id, const char* path, unsigned int line, unsigned int character) {
    Server* server = context_get_server(context, id);
    if (server == NULL) {
        return 0;
    }

    LSTalk_Position position;
    position.line = line;
    position.character = character;

    JSONValue params = text_document_identifier_json(path, &context->allocator);
    json_object_const_key_set(&params, "position", position_json(position, &context->allocator), &context->allocator);

    server_make_and_send_request(context, server, "textDocument/hover", params);
    return 1;
}

char* lstalk_symbol_kind_to_string(LSTalk_SymbolKind kind) {
    switch (kind) {
        case LSTALK_SYMBOLKIND_FILE: return "file";
        case LSTALK_SYMBOLKIND_MODULE: return "module";
        case LSTALK_SYMBOLKIND_NAMESPACE: return "namespace";
        case LSTALK_SYMBOLKIND_PACKAGE: return "package";
        case LSTALK_SYMBOLKIND_CLASS: return "class";
        case LSTALK_SYMBOLKIND_METHOD: return "method";
        case LSTALK_SYMBOLKIND_PROPERTY: return "property";
        case LSTALK_SYMBOLKIND_FIELD: return "field";
        case LSTALK_SYMBOLKIND_CONSTRUCTOR: return "constructor";
        case LSTALK_SYMBOLKIND_ENUM: return "enum";
        case LSTALK_SYMBOLKIND_INTERFACE: return "interface";
        case LSTALK_SYMBOLKIND_FUNCTION: return "function";
        case LSTALK_SYMBOLKIND_VARIABLE: return "variable";
        case LSTALK_SYMBOLKIND_CONSTANT: return "constant";
        case LSTALK_SYMBOLKIND_STRING: return "string";
        case LSTALK_SYMBOLKIND_NUMBER: return "number";
        case LSTALK_SYMBOLKIND_BOOLEAN: return "boolean";
        case LSTALK_SYMBOLKIND_ARRAY: return "array";
        case LSTALK_SYMBOLKIND_OBJECT: return "object";
        case LSTALK_SYMBOLKIND_KEY: return "key";
        case LSTALK_SYMBOLKIND_NULL: return "null";
        case LSTALK_SYMBOLKIND_ENUMMEMBER: return "enummember";
        case LSTALK_SYMBOLKIND_STRUCT: return "struct";
        case LSTALK_SYMBOLKIND_EVENT: return "event";
        case LSTALK_SYMBOLKIND_OPERATOR: return "operator";
        case LSTALK_SYMBOLKIND_TYPEPARAMETER: return "typeparameter";
        case LSTALK_SYMBOLKIND_NONE:
        default: break;
    }

    return "none";
}

#ifdef LSTALK_TESTS

//
// Testing Framework
//

typedef int (*TestCaseFn)();
typedef struct TestCase {
    TestCaseFn fn;
    char* name;
} TestCase;

typedef struct TestResults {
    int pass;
    int fail;
} TestResults;

static void add_test(Vector* tests, TestCaseFn fn, char* name, LSTalk_MemoryAllocator* allocator) {
    if (tests == NULL) {
        return;
    }

    TestCase test_case;
    test_case.fn = fn;
    test_case.name = name;
    vector_push(tests, &test_case, allocator);
}

static LSTalk_MemoryAllocator tests_default_allocator() {
    LSTalk_MemoryAllocator allocator;
    allocator.malloc = malloc;
    allocator.calloc = calloc;
    allocator.realloc = realloc;
    allocator.free = free;
    return allocator;
}

#define REGISTER_TEST(tests, fn, allocator) add_test(tests, fn, #fn, allocator)
#define RED_TEXT(text) printf("\033[0;31m"); printf("%s", text); printf("\033[0m");
#define GREEN_TEXT(text) printf("\033[0;32m"); printf("%s", text); printf("\033[0m");

static int tests_run(Vector* tests) {
    if (tests == NULL || tests->element_size != sizeof(TestCase)) {
        return 0;
    }

    printf("Running %zu tests...\n", tests->length);
    clock_t tests_start = clock();

    int failed = 0;
    for (size_t i = 0; i < tests->length; i++) {
        TestCase* test_case = (TestCase*)vector_get(tests, i);

        clock_t start = clock();
        int success = test_case->fn();
        if (success) {
            GREEN_TEXT("PASS");
        } else {
            RED_TEXT("FAIL");
            failed++;
        }

        double elapsed = (double)(clock() - start) / (double)CLOCKS_PER_SEC;
        printf(" ... (%fs) %s\n", elapsed, test_case->name);
    }
    double elapsed = (double)(clock() - tests_start) / (double)CLOCKS_PER_SEC;
    printf("Completed in %fs\n", elapsed);

    return failed;
}

// Vector Tests

static int test_vector_create() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector vector = vector_create(sizeof(int), &allocator);
    const size_t capacity = vector.capacity;
    const size_t element_size = vector.element_size;
    vector_destroy(&vector, &allocator);
    return capacity == 1 && element_size == sizeof(int);
}

static int test_vector_destroy() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector vector = vector_create(sizeof(int), &allocator);
    vector_destroy(&vector, &allocator);
    return vector.length == 0 && vector.data == NULL;
}

static int test_vector_resize() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector vector = vector_create(sizeof(int), &allocator);
    int result = vector.length == 0 && vector.capacity == 1;
    vector_resize(&vector, 5, &allocator);
    result &= vector.length == 0 && vector.capacity == 5;
    vector_destroy(&vector, &allocator);
    return result;
}

static int test_vector_push() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector vector = vector_create(sizeof(int), &allocator);
    int i = 5;
    vector_push(&vector, &i, &allocator);
    i = 10;
    vector_push(&vector, &i, &allocator);
    int result = vector.length == 2;
    vector_destroy(&vector, &allocator);
    return result;
}

static int test_vector_append() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector vector = vector_create(sizeof(char), &allocator);
    vector_append(&vector, (void*)"Hello", 5, &allocator);
    int result = strncmp(vector.data, "Hello", 5) == 0;
    vector_append(&vector, (void*)" World", 6, &allocator);
    result &= strncmp(vector.data, "Hello World", 11) == 0;
    vector_destroy(&vector, &allocator);
    return result;
}

static int test_vector_remove() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector vector = vector_create(sizeof(int), &allocator);
    for (int i = 0; i < 5; i++) {
        vector_push(&vector, &i, &allocator);
    }
    int result = vector.length == 5;
    result &= *(int*)vector_get(&vector, 2) == 2;
    vector_remove(&vector, 2);
    result &= *(int*)vector_get(&vector, 2) == 3;
    result &= vector.length == 4;
    vector_destroy(&vector, &allocator);
    return result;
}

static int test_vector_get() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector vector = vector_create(sizeof(int), &allocator);
    int i = 5;
    vector_push(&vector, &i, &allocator);
    i = 10;
    vector_push(&vector, &i, &allocator);
    int result = *(int*)vector_get(&vector, 0) == 5 && *(int*)vector_get(&vector, 1) == 10;
    vector_destroy(&vector, &allocator);
    return result;
}

static TestResults tests_vector() {
    TestResults result;
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector tests = vector_create(sizeof(TestCase), &allocator);

    REGISTER_TEST(&tests, test_vector_create, &allocator);
    REGISTER_TEST(&tests, test_vector_destroy, &allocator);
    REGISTER_TEST(&tests, test_vector_resize, &allocator);
    REGISTER_TEST(&tests, test_vector_push, &allocator);
    REGISTER_TEST(&tests, test_vector_append, &allocator);
    REGISTER_TEST(&tests, test_vector_remove, &allocator);
    REGISTER_TEST(&tests, test_vector_get, &allocator);

    result.fail = tests_run(&tests);
    result.pass = (int)tests.length - result.fail;
    vector_destroy(&tests, &allocator);

    return result;
}

// JSON Tests

static int test_json_decode_boolean_false() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("false", &allocator);
    return value.type == JSON_VALUE_BOOLEAN && value.value.bool_value == 0;
}

static int test_json_decode_boolean_true() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("true", &allocator);
    return value.type == JSON_VALUE_BOOLEAN && value.value.bool_value == 1;
}

static int test_json_decode_int() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("42", &allocator);
    return value.type == JSON_VALUE_INT && value.value.int_value == 42;
}

static int test_json_decode_float() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("3.14", &allocator);
    return value.type == JSON_VALUE_FLOAT && value.value.float_value == 3.14f;
}

static int test_json_decode_string() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("\"Hello World\"", &allocator);
    int result = value.type == JSON_VALUE_STRING && strcmp(value.value.string_value, "Hello World") == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_escaped_string() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("\"Hello \\\"World\\\"", &allocator);
    int result = value.type == JSON_VALUE_STRING && strcmp(value.value.string_value, "Hello \"World\"") == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_single_escaped_string() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("[\"'\", \"\\\\\"\", \":\"]", &allocator);
    int result = strcmp(json_array_get(&value, 0).value.string_value, "'") == 0;
    result &= strcmp(json_array_get(&value, 1).value.string_value, "\\\"") == 0;
    result &= strcmp(json_array_get(&value, 2).value.string_value, ":") == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("{\"Int\": 42, \"Float\": 3.14}", &allocator);
    int result = json_object_get(&value, "Int").value.int_value == 42 && json_object_get(&value, "Float").value.float_value == 3.14f;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_sub_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("{\"object\": {\"Int\": 42, \"Float\": 3.14}}", &allocator);
    JSONValue object = json_object_get(&value, "object");
    int result = json_object_get(&object, "Int").value.int_value == 42 && json_object_get(&object, "Float").value.float_value == 3.14f;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_empty_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("{}", &allocator);
    int result = value.type == JSON_VALUE_OBJECT && value.value.object_value->pairs.length == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_empty_sub_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("{\"Int\": 42, \"object\": {}}", &allocator);
    int result = json_object_get(&value, "Int").value.int_value == 42;
    result &= json_object_get(&value, "object").value.object_value->pairs.length == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_array() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("[42, 3.14, \"Hello World\"]", &allocator);
    int result = json_array_get(&value, 0).value.int_value == 42;
    result &= json_array_get(&value, 1).value.float_value == 3.14f;
    result &= strcmp(json_array_get(&value, 2).value.string_value, "Hello World") == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_array_of_objects() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("[{\"Int\": 42}, {\"Float\": 3.14}]", &allocator);
    JSONValue object = json_array_get(&value, 0);
    int result = json_object_get(&object, "Int").value.int_value == 42;
    object = json_array_get(&value, 1);
    result &= json_object_get(&object, "Float").value.float_value == 3.14f;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_decode_empty_array() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_decode("[]", &allocator);
    int result = value.type == JSON_VALUE_ARRAY && value.value.array_value->values.length == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_encode_boolean_false() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_boolean(0);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "false") == 0;
    json_destroy_encoder(&encoder, &allocator);
    return result;
}

static int test_json_encode_boolean_true() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_boolean(1);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "true") == 0;
    json_destroy_encoder(&encoder, &allocator);
    return result;
}

static int test_json_encode_int() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_int(42);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "42") == 0;
    json_destroy_encoder(&encoder, &allocator);
    return result;
}

static int test_json_encode_float() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_float(3.14f);
    JSONEncoder encoder = json_encode(&value, &allocator);
    char buffer[40];
    sprintf_s(buffer, sizeof(buffer), "%f", 3.14f);
    int result = strcmp(encoder.string.data, buffer) == 0;
    json_destroy_encoder(&encoder, &allocator);
    return result;
}

static int test_json_encode_string() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_string("Hello World", &allocator);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "\"Hello World\"") == 0;
    json_destroy_encoder(&encoder, &allocator);
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_encode_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_object(&allocator);
    json_object_const_key_set(&value, "Int", json_make_int(42), &allocator);
    json_object_const_key_set(&value, "String", json_make_string_const("Hello World"), &allocator);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "{\"Int\": 42, \"String\": \"Hello World\"}") == 0;
    json_destroy_encoder(&encoder, &allocator);
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_encode_sub_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue object = json_make_object(&allocator);
    json_object_const_key_set(&object, "Int", json_make_int(42), &allocator);
    json_object_const_key_set(&object, "String", json_make_string_const("Hello World"), &allocator);
    JSONValue value = json_make_object(&allocator);
    json_object_const_key_set(&value, "object", object, &allocator);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "{\"object\": {\"Int\": 42, \"String\": \"Hello World\"}}") == 0;
    json_destroy_encoder(&encoder, &allocator);
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_encode_array() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_array(&allocator);
    json_array_push(&value, json_make_int(42), &allocator);
    json_array_push(&value, json_make_string("Hello World", &allocator), &allocator);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "[42, \"Hello World\"]") == 0;
    json_destroy_encoder(&encoder, &allocator);
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_encode_array_of_objects() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_array(&allocator);
    JSONValue object = json_make_object(&allocator);
    json_object_const_key_set(&object, "Int", json_make_int(42), &allocator);
    json_array_push(&value, object, &allocator);
    object = json_make_object(&allocator);
    json_object_const_key_set(&object, "String", json_make_string_const("Hello World"), &allocator);
    json_array_push(&value, object, &allocator);
    JSONEncoder encoder = json_encode(&value, &allocator);
    int result = strcmp(encoder.string.data, "[{\"Int\": 42}, {\"String\": \"Hello World\"}]") == 0;
    json_destroy_encoder(&encoder, &allocator);
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_json_move_string() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    JSONValue value = json_make_string("Hello World", &allocator);
    size_t length = strlen(value.value.string_value);
    int result = strncmp(value.value.string_value, "Hello World", length) == 0;
    char* moved = json_move_string(&value);
    json_destroy_value(&value, &allocator);
    result &= strncmp(moved, "Hello World", length) == 0;
    allocator.free(moved);
    return result;
}

static int test_json_escape_string() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    char* escaped = json_escape_string("Hello\nworld\tfoo\\bar/", &allocator);
    int result = strcmp(escaped, "Hello\\nworld\\tfoo\\\\bar\\/") == 0;
    allocator.free(escaped);
    return result;
}

static int test_json_unescape_string() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    char* unescaped = json_unescape_string("Hello\\nworld\\tfoo\\\\bar\\/", &allocator);
    int result = strcmp(unescaped, "Hello\nworld\tfoo\\bar/") == 0;
    allocator.free(unescaped);
    return result;
}

static TestResults tests_json() {
    TestResults result;
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector tests = vector_create(sizeof(TestCase), &allocator);

    REGISTER_TEST(&tests, test_json_decode_boolean_false, &allocator);
    REGISTER_TEST(&tests, test_json_decode_boolean_true, &allocator);
    REGISTER_TEST(&tests, test_json_decode_int, &allocator);
    REGISTER_TEST(&tests, test_json_decode_float, &allocator);
    REGISTER_TEST(&tests, test_json_decode_string, &allocator);
    REGISTER_TEST(&tests, test_json_decode_escaped_string, &allocator);
    REGISTER_TEST(&tests, test_json_decode_single_escaped_string, &allocator);
    REGISTER_TEST(&tests, test_json_decode_object, &allocator);
    REGISTER_TEST(&tests, test_json_decode_sub_object, &allocator);
    REGISTER_TEST(&tests, test_json_decode_empty_object, &allocator);
    REGISTER_TEST(&tests, test_json_decode_empty_sub_object, &allocator);
    REGISTER_TEST(&tests, test_json_decode_array, &allocator);
    REGISTER_TEST(&tests, test_json_decode_array_of_objects, &allocator);
    REGISTER_TEST(&tests, test_json_decode_empty_array, &allocator);
    REGISTER_TEST(&tests, test_json_encode_boolean_false, &allocator);
    REGISTER_TEST(&tests, test_json_encode_boolean_true, &allocator);
    REGISTER_TEST(&tests, test_json_encode_int, &allocator);
    REGISTER_TEST(&tests, test_json_encode_float, &allocator);
    REGISTER_TEST(&tests, test_json_encode_string, &allocator);
    REGISTER_TEST(&tests, test_json_encode_object, &allocator);
    REGISTER_TEST(&tests, test_json_encode_array, &allocator);
    REGISTER_TEST(&tests, test_json_encode_sub_object, &allocator);
    REGISTER_TEST(&tests, test_json_encode_array_of_objects, &allocator);
    REGISTER_TEST(&tests, test_json_move_string, &allocator);
    REGISTER_TEST(&tests, test_json_escape_string, &allocator);
    REGISTER_TEST(&tests, test_json_unescape_string, &allocator);

    result.fail = tests_run(&tests);
    result.pass = (int)tests.length - result.fail;

    vector_destroy(&tests, &allocator);
    return result;
}

// Message tests

#define TEST_BUFFER_SIZE 1024

static void test_message_set(char* content, char* out, size_t out_size) {
    size_t length = strlen(content);
    sprintf_s(out, out_size, "Content-Length: %zu\r\n%s", length, content);
}

static int test_message_empty_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Message message = message_create();
    char buffer[TEST_BUFFER_SIZE];
    test_message_set("{}", buffer, sizeof(buffer));
    char* ptr = &buffer[0];
    JSONValue value = message_to_json(&message, &ptr, &allocator);
    int result = value.type == JSON_VALUE_OBJECT && value.value.object_value->pairs.length == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_message_object() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Message message = message_create();
    char buffer[TEST_BUFFER_SIZE];
    test_message_set("{\"Int\": 42}", buffer, sizeof(buffer));
    char* ptr = &buffer[0];
    JSONValue value = message_to_json(&message, &ptr, &allocator);
    int result = value.type == JSON_VALUE_OBJECT;
    JSONValue obj = json_object_get(&value, "Int");
    json_destroy_value(&value, &allocator);
    return result && obj.type == JSON_VALUE_INT && obj.value.int_value == 42;
}

static int test_message_object_and_invalid() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Message message = message_create();
    char buffer[TEST_BUFFER_SIZE];
    test_message_set("{}", buffer, sizeof(buffer));
    char* ptr = &buffer[0];
    JSONValue first = message_to_json(&message, &ptr, &allocator);
    JSONValue second = message_to_json(&message, &ptr, &allocator);
    int result = first.type == JSON_VALUE_OBJECT && first.value.object_value->pairs.length == 0;
    result &= second.type == JSON_VALUE_NULL;
    json_destroy_value(&first, &allocator);
    json_destroy_value(&second, &allocator);
    return result;
}

static int test_message_two_objects() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Message message = message_create();
    char buffer_1[TEST_BUFFER_SIZE];
    test_message_set("{\"Int\": 42}", buffer_1, sizeof(buffer_1));
    char buffer_2[TEST_BUFFER_SIZE];
    test_message_set("{\"Float\": 3.14}", buffer_2, sizeof(buffer_2));
    char buffer[TEST_BUFFER_SIZE * 3];
    sprintf_s(buffer, sizeof(buffer), "%s\r\n%s", buffer_1, buffer_2);
    char* ptr = &buffer[0];
    JSONValue first = message_to_json(&message, &ptr, &allocator);
    JSONValue second = message_to_json(&message, &ptr, &allocator);
    JSONValue first_int = json_object_get(&first, "Int");
    JSONValue second_float = json_object_get(&second, "Float");
    int result = first_int.type == JSON_VALUE_INT && first_int.value.int_value == 42;
    result &= second_float.type == JSON_VALUE_FLOAT && second_float.value.float_value == 3.14f;
    json_destroy_value(&first, &allocator);
    json_destroy_value(&second, &allocator);
    return result;
}

static int test_message_partial() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    char* data = "{\"String\": \"Hello World\"}";
    size_t data_length = strlen(data);
    Message message = message_create();
    char buffer[TEST_BUFFER_SIZE];
    test_message_set(data, buffer, sizeof(buffer));
    char partial[TEST_BUFFER_SIZE];
    size_t offset = 30;
    strncpy_s(partial, sizeof(partial), buffer, offset);
    partial[offset] = 0;
    char* ptr = &partial[0];
    JSONValue value = message_to_json(&message, &ptr, &allocator);
    int result = value.type == JSON_VALUE_NULL && message.expected_length == data_length;
    size_t remaining = strlen(buffer) - offset;
    strncpy_s(partial, sizeof(partial), buffer + offset, remaining);
    partial[remaining] = 0;
    ptr = &partial[0];
    value = message_to_json(&message, &ptr, &allocator);
    result &= value.type == JSON_VALUE_OBJECT;
    result &= message.buffer == NULL && message.length == 0 && message.expected_length == 0;
    JSONValue string_value = json_object_get(&value, "String");
    result &= string_value.type == JSON_VALUE_STRING && strcmp(string_value.value.string_value, "Hello World") == 0;
    json_destroy_value(&value, &allocator);
    return result;
}

static int test_message_partial_no_content() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    char* data = "{\"Int\": 42}";
    size_t data_length = strlen(data);
    char buffer[TEST_BUFFER_SIZE];
    sprintf_s(buffer, sizeof(buffer), "Content-Length: %zu\r\n", data_length);
    Message message = message_create();
    char* ptr = &buffer[0];
    JSONValue value = message_to_json(&message, &ptr, &allocator);
    int result = value.type == JSON_VALUE_NULL && message.expected_length == data_length;
    sprintf_s(buffer, sizeof(buffer), "%s", data);
    ptr = &buffer[0];
    value = message_to_json(&message, &ptr, &allocator);
    result &= value.type == JSON_VALUE_OBJECT;
    result &= message.buffer == NULL && message.length == 0 && message.expected_length == 0;
    JSONValue int_value = json_object_get(&value, "Int");
    result &= int_value.type == JSON_VALUE_INT && int_value.value.int_value == 42;
    json_destroy_value(&value, &allocator);
    return result;
}

static TestResults tests_message() {
    TestResults result;
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector tests = vector_create(sizeof(TestCase), &allocator);

    REGISTER_TEST(&tests, test_message_empty_object, &allocator);
    REGISTER_TEST(&tests, test_message_object, &allocator);
    REGISTER_TEST(&tests, test_message_object_and_invalid, &allocator);
    REGISTER_TEST(&tests, test_message_two_objects, &allocator);
    REGISTER_TEST(&tests, test_message_partial, &allocator);
    REGISTER_TEST(&tests, test_message_partial_no_content, &allocator);

    result.fail = tests_run(&tests);
    result.pass = (int)tests.length - result.fail;

    vector_destroy(&tests, &allocator);
    return result;
}

// Server tests

static LSTalk_Context* test_context = NULL;
static LSTalk_ServerID test_server = LSTALK_INVALID_SERVER_ID;
static char test_server_path[PATH_MAX] = "";
static int test_server_debug_flags = 0;
static const char* test_source = "void foo() {}";
static const char* test_source_file_name = "test_source.c";

#if LSTALK_WINDOWS
    #define TEST_SERVER_NAME "test_server.exe"
#else
    #define TEST_SERVER_NAME "test_server"
#endif

static void test_server_get_source_file_name(char* destination, size_t size) {
    if (destination == NULL) {
        return;
    }

    file_get_directory(test_server_path, destination, size);
    strncat_s(destination, size, "/", size);
    strncat_s(destination, size, test_source_file_name, size);
}

static int test_server_process_notification(LSTalk_Notification* notification, LSTalk_NotificationType filter) {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    if (notification == NULL) {
        return 0;
    }

    clock_t start = clock();
    while (1) {
        if (!lstalk_process_responses(test_context)) {
            break;
        }

        if (lstalk_poll_notification(test_context, test_server, notification)) {
            if (filter == LSTALK_NOTIFICATION_NONE || notification->type == filter) {
                return 1;
            }
        }

        double elapsed = (double)(clock() - start) / (double)CLOCKS_PER_SEC;
        if (elapsed >= 5.0) {
            break;
        }
    }

    return 0;
}

static int test_server_init() {
    if (test_context != NULL) {
        return 0;
    }
    test_context = lstalk_init();
    lstalk_set_debug_flags(test_context, test_server_debug_flags);
    return test_context != NULL;
}

static int test_server_connect() {
    if (test_context == NULL) {
        return 0;
    }

    if (test_server != LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    LSTalk_ConnectParams connect_params;
    connect_params.root_uri = NULL;
    connect_params.trace = LSTALK_TRACE_OFF;
    connect_params.seek_path_env = 0;
    test_server = lstalk_connect(test_context, test_server_path, &connect_params);
    if (test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    clock_t start = clock();
    int result = 0;
    while (1) {
        if (!lstalk_process_responses(test_context)) {
            break;
        }

        if (lstalk_get_connection_status(test_context, test_server) == LSTALK_CONNECTION_STATUS_CONNECTED) {
            result = 1;
            break;
        }

        double elapsed = (double)(clock() - start) / (double)CLOCKS_PER_SEC;
        if (elapsed >= 5.0) {
            break;
        }
    }

    return result;
}

static int test_server_trace() {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    if (!lstalk_set_trace(test_context, test_server, LSTALK_TRACE_MESSAGES)) {
        return 0;
    }

    LSTalk_Notification notification;
    if (!test_server_process_notification(&notification, LSTALK_NOTIFICATION_LOG)) {
        return 0;
    }

    if (strcmp(notification.data.log.message, "message") != 0 || notification.data.log.verbose != NULL) {
        return 0;
    }

    if (!lstalk_set_trace(test_context, test_server, LSTALK_TRACE_VERBOSE)) {
        return 0;
    }

    if (!test_server_process_notification(&notification, LSTALK_NOTIFICATION_LOG)) {
        return 0;
    }

    if (strcmp(notification.data.log.message, "message") != 0 || strcmp(notification.data.log.verbose, "verbose") != 0) {
        return 0;
    }

    return 1;
}

static int test_server_text_document_did_open() {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    char file_name[PATH_MAX];
    test_server_get_source_file_name(file_name, sizeof(file_name));

    if (!lstalk_text_document_did_open(test_context, test_server, file_name)) {
        return 0;
    }

    return 1;
}

static int test_server_document_symbols() {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    char file_name[PATH_MAX];
    test_server_get_source_file_name(file_name, sizeof(file_name));

    if (!lstalk_text_document_symbol(test_context, test_server, file_name)) {
        return 0;
    }

    LSTalk_Notification notification;
    if (!test_server_process_notification(&notification, LSTALK_NOTIFICATION_TEXT_DOCUMENT_SYMBOLS)) {
        return 0;
    }

    if (notification.data.document_symbols.symbols_count != 1) {
        return 0;
    }

    return 1;
}

static int test_server_document_semantic_tokens() {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    char file_name[PATH_MAX];
    test_server_get_source_file_name(file_name, sizeof(file_name));

    if (!lstalk_text_document_semantic_tokens(test_context, test_server, file_name)) {
        return 0;
    }

    LSTalk_Notification notification;
    if (!test_server_process_notification(&notification, LSTALK_NOTIFICATION_SEMANTIC_TOKENS)) {
        return 0;
    }

    if (strcmp(notification.data.semantic_tokens.result_id, "1") != 0) {
        return 0;
    }

    if (notification.data.semantic_tokens.tokens_count == 0) {
        return 0;
    }

    if (notification.data.semantic_tokens.tokens[0].line != 0 || notification.data.semantic_tokens.tokens[0].character != 0 || notification.data.semantic_tokens.tokens[0].length != 0) {
        return 0;
    }

    return 1;
}

static int test_server_text_document_hover() {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    char file_name[PATH_MAX];
    test_server_get_source_file_name(file_name, sizeof(file_name));

    if (!lstalk_text_document_hover(test_context, test_server, file_name, 0, 0)) {
        return 0;
    }

    LSTalk_Notification notification;
    if (!test_server_process_notification(&notification, LSTALK_NOTIFICATION_HOVER)) {
        return 0;
    }

    if (strcmp(notification.data.hover.contents, "contents") != 0) {
        return 0;
    }

    if (notification.data.hover.range.end.character != 5) {
        return 0;
    }

    return 1;
}

static int test_server_text_document_did_close() {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    char file_name[PATH_MAX];
    test_server_get_source_file_name(file_name, sizeof(file_name));

    if (!lstalk_text_document_did_close(test_context, test_server, file_name)) {
        return 0;
    }

    return 1;
}

static int test_server_close() {
    if (test_context == NULL || test_server == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    int result = lstalk_close(test_context, test_server);
    test_server = LSTALK_INVALID_SERVER_ID;
    return result;
}

static int test_server_shutdown() {
    if (test_context == NULL) {
        return 0;
    }
    lstalk_shutdown(test_context);
    test_context = NULL;
    return 1;
}

static TestResults tests_server() {
    TestResults results;
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector tests = vector_create(sizeof(TestCase), &allocator);

    char file_name[PATH_MAX];
    test_server_get_source_file_name(file_name, sizeof(file_name));

    file_write_contents(file_name, test_source);

    REGISTER_TEST(&tests, test_server_init, &allocator);
    REGISTER_TEST(&tests, test_server_connect, &allocator);
    REGISTER_TEST(&tests, test_server_trace, &allocator);
    REGISTER_TEST(&tests, test_server_text_document_did_open, &allocator);
    REGISTER_TEST(&tests, test_server_document_symbols, &allocator);
    REGISTER_TEST(&tests, test_server_document_semantic_tokens, &allocator);
    REGISTER_TEST(&tests, test_server_text_document_hover, &allocator);
    REGISTER_TEST(&tests, test_server_text_document_did_close, &allocator);
    REGISTER_TEST(&tests, test_server_close, &allocator);
    REGISTER_TEST(&tests, test_server_shutdown, &allocator);

    results.fail = tests_run(&tests);
    results.pass = (int)tests.length - results.fail;

    vector_destroy(&tests, &allocator);
    remove(file_name);

    return results;
}

typedef struct TestSuite {
    TestResults (*fn)();
    char* name;
} TestSuite;

static void add_test_suite(Vector* suites, TestResults (*fn)(), char* name, LSTalk_MemoryAllocator* allocator) {
    if (suites == NULL) {
        return;
    }

    TestSuite suite;
    suite.fn = fn;
    suite.name = name;
    vector_push(suites, &suite, allocator);
}

#define ADD_TEST_SUITE(suites, fn, allocator) add_test_suite(suites, fn, #fn, allocator)

void lstalk_tests(int argc, char** argv) {
    (void)argc;
    printf("Running tests for lstalk...\n\n");

    for (int i = 0; i < argc; i++) {
        char* arg = argv[i];
        if (strcmp(arg, "--server-responses") == 0) {
            test_server_debug_flags |= LSTALK_DEBUGFLAGS_PRINT_RESPONSES;
        }
    }

    size_t size = PATH_MAX;
    char absolute_path[PATH_MAX] = "";
    file_to_absolute_path(argv[0], absolute_path, size);
    file_get_directory(absolute_path, test_server_path, size);
    strncat_s(test_server_path, size, "/", size);
    strncat_s(test_server_path, size, TEST_SERVER_NAME, size);

    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Vector suites = vector_create(sizeof(TestSuite), &allocator);
    ADD_TEST_SUITE(&suites, tests_vector, &allocator);
    ADD_TEST_SUITE(&suites, tests_json, &allocator);
    ADD_TEST_SUITE(&suites, tests_message, &allocator);
    ADD_TEST_SUITE(&suites, tests_server, &allocator);

    TestResults results;
    results.pass = 0;
    results.fail = 0;

    clock_t start = clock();
    for (size_t i = 0; i < suites.length; i++) {
        TestSuite* suite = (TestSuite*)vector_get(&suites, i);

        printf("Test suite %s\n", suite->name);
        TestResults suite_results = suite->fn();
        results.fail += suite_results.fail;
        results.pass += suite_results.pass;
        printf("\n");
    }

    double elapsed = (double)(clock() - start) / (double)CLOCKS_PER_SEC;
    printf("TESTS TIME: %fs\n", elapsed);
    printf("TESTS PASSED: %d\n", results.pass);
    printf("TESTS FAILED: %d\n", results.fail);

    vector_destroy(&suites, &allocator);
}

static ServerCapabilities test_server_make_capabilities() {
    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    ServerCapabilities result;
    memset(&result, 0, sizeof(result));

    #define ALLOC_TEXT_DOCUMENT_REGISTRATION(property) \
    property.text_document_registration.document_selector_count = 1; \
    property.text_document_registration.document_selector = (DocumentFilter*)allocator.malloc(sizeof(DocumentFilter)); \
    property.text_document_registration.document_selector[0].language = string_alloc_copy("language", &allocator); \
    property.text_document_registration.document_selector[0].scheme = string_alloc_copy("scheme", &allocator); \
    property.text_document_registration.document_selector[0].pattern = string_alloc_copy("pattern", &allocator);

    #define ALLOC_FILE_OPERATIONS_REGISTRATION(property) \
    property.filters_count = 1; \
    property.filters = (FileOperationFilter*)allocator.malloc(sizeof(FileOperationFilter)); \
    property.filters[0].scheme = string_alloc_copy("scheme", &allocator); \
    property.filters[0].pattern.glob = string_alloc_copy("glob", &allocator);

    result.notebook_document_sync.static_registration.id = string_alloc_copy("id", &allocator);
    result.notebook_document_sync.notebook_selector_count = 1;
    result.notebook_document_sync.notebook_selector = (NotebookSelector*)allocator.malloc(sizeof(NotebookSelector));
    result.notebook_document_sync.notebook_selector[0].notebook.notebook_type = string_alloc_copy("notebook_type", &allocator);
    result.notebook_document_sync.notebook_selector[0].notebook.scheme = string_alloc_copy("scheme", &allocator);
    result.notebook_document_sync.notebook_selector[0].notebook.pattern = string_alloc_copy("pattern", &allocator);
    result.notebook_document_sync.notebook_selector[0].cells_count = 1;
    result.notebook_document_sync.notebook_selector[0].cells = (char**)allocator.malloc(sizeof(char*));
    result.notebook_document_sync.notebook_selector[0].cells[0] = string_alloc_copy("cells", &allocator);
    result.completion_provider.trigger_characters_count = 1;
    result.completion_provider.trigger_characters = (char**)allocator.malloc(sizeof(char*));
    result.completion_provider.trigger_characters[0] = string_alloc_copy("trigger_characters", &allocator);
    result.completion_provider.all_commit_characters_count = 1;
    result.completion_provider.all_commit_characters = (char**)allocator.malloc(sizeof(char*));
    result.completion_provider.all_commit_characters[0] = string_alloc_copy("all_commit_characters", &allocator);
    result.signature_help_provider.trigger_characters_count = 1;
    result.signature_help_provider.trigger_characters = (char**)allocator.malloc(sizeof(char*));
    result.signature_help_provider.trigger_characters[0] = string_alloc_copy("trigger_characters", &allocator);
    result.signature_help_provider.retrigger_characters_count = 1;
    result.signature_help_provider.retrigger_characters = (char**)allocator.malloc(sizeof(char*));
    result.signature_help_provider.retrigger_characters[0] = string_alloc_copy("retrigger_characters", &allocator);
    result.declaration_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.declaration_provider);
    result.type_definition_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.type_definition_provider);
    result.implementation_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.implementation_provider);
    result.document_symbol_provider.label = string_alloc_copy("label", &allocator);
    result.color_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.color_provider);
    result.document_on_type_formatting_provider.first_trigger_character = string_alloc_copy("first_trigger_character", &allocator);
    result.document_on_type_formatting_provider.more_trigger_character_count = 1;
    result.document_on_type_formatting_provider.more_trigger_character = (char**)allocator.malloc(sizeof(char*));
    result.document_on_type_formatting_provider.more_trigger_character[0] = string_alloc_copy("more_trigger_character", &allocator);
    result.folding_range_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.folding_range_provider);
    result.execute_command_provider.commands_count = 1;
    result.execute_command_provider.commands = (char**)allocator.malloc(sizeof(char*));
    result.execute_command_provider.commands[0] = string_alloc_copy("commands", &allocator);
    result.selection_range_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.selection_range_provider);
    result.linked_editing_range_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.linked_editing_range_provider);
    result.call_hierarchy_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.call_hierarchy_provider);
    result.semantic_tokens_provider.semantic_tokens.legend.token_modifiers_count = 1;
    result.semantic_tokens_provider.semantic_tokens.legend.token_modifiers = (char**)allocator.malloc(sizeof(char*));
    result.semantic_tokens_provider.semantic_tokens.legend.token_modifiers[0] = string_alloc_copy("token_modifiers", &allocator);
    result.semantic_tokens_provider.semantic_tokens.legend.token_types_count = 1;
    result.semantic_tokens_provider.semantic_tokens.legend.token_types = (char**)allocator.malloc(sizeof(char*));
    result.semantic_tokens_provider.semantic_tokens.legend.token_types[0] = string_alloc_copy("token_types", &allocator);
    result.semantic_tokens_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.semantic_tokens_provider);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.moniker_provider);
    result.type_hierarchy_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.type_hierarchy_provider);
    result.inline_value_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.inline_value_provider);
    result.inlay_hint_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.inlay_hint_provider);
    result.diagnostic_provider.static_registration.id = string_alloc_copy("id", &allocator);
    ALLOC_TEXT_DOCUMENT_REGISTRATION(result.diagnostic_provider);
    result.workspace.workspace_folders.change_notifications = string_alloc_copy("change_notifications", &allocator);
    ALLOC_FILE_OPERATIONS_REGISTRATION(result.workspace.file_operations.did_create);
    ALLOC_FILE_OPERATIONS_REGISTRATION(result.workspace.file_operations.will_create);
    ALLOC_FILE_OPERATIONS_REGISTRATION(result.workspace.file_operations.did_rename);
    ALLOC_FILE_OPERATIONS_REGISTRATION(result.workspace.file_operations.will_rename);
    ALLOC_FILE_OPERATIONS_REGISTRATION(result.workspace.file_operations.did_delete);
    ALLOC_FILE_OPERATIONS_REGISTRATION(result.workspace.file_operations.will_delete);

    #undef ALLOC_TEXT_DOCUMENT_REGISTRATION

    return result;
}

static LSTalk_DocumentSymbolNotification test_server_make_document_symbols(LSTalk_MemoryAllocator* allocator) {
    LSTalk_DocumentSymbolNotification result;
    memset(&result, 0, sizeof(result));

    result.symbols_count = 1;
    result.symbols = (LSTalk_DocumentSymbol*)allocator->calloc(result.symbols_count, sizeof(LSTalk_DocumentSymbol));

    LSTalk_DocumentSymbol* document_symbol = &result.symbols[0];
    document_symbol->name = string_alloc_copy("foo", allocator);
    document_symbol->detail = string_alloc_copy("Detail", allocator);
    document_symbol->kind = LSTALK_SYMBOLKIND_FUNCTION;
    document_symbol->range.start.line = 1;
    document_symbol->range.start.character = 2;
    document_symbol->range.end.line = 3;
    document_symbol->range.end.character = 4;
    document_symbol->selection_range.start.line = 5;
    document_symbol->selection_range.start.character = 6;
    document_symbol->selection_range.end.line = 7;
    document_symbol->selection_range.end.character = 8;
    document_symbol->children_count = 1;
    document_symbol->children = (LSTalk_DocumentSymbol*)allocator->calloc(document_symbol->children_count, sizeof(LSTalk_DocumentSymbol));
    document_symbol->children[0].name = string_alloc_copy("child", allocator);
    document_symbol->children[0].detail = string_alloc_copy("child detail", allocator);

    return result;
}

static LSTalk_SemanticTokens test_server_make_semantic_tokens(LSTalk_MemoryAllocator* allocator) {
    LSTalk_SemanticTokens result;
    memset(&result, 0, sizeof(result));

    result.result_id = string_alloc_copy("1", allocator);
    result.tokens_count = 1;
    result.tokens = (LSTalk_SemanticToken*)allocator->calloc(1, sizeof(LSTalk_SemanticToken));
    result.tokens[0].line = 0;
    result.tokens[0].character = 0;
    result.tokens[0].length = 0;
    result.tokens[0].token_type = "token_type";
    result.tokens[0].token_modifiers_count = 1;
    result.tokens[0].token_modifiers = (char**)allocator->calloc(1, sizeof(char*));
    result.tokens[0].token_modifiers[0] = string_alloc_copy("token_modifiers", allocator);

    return result;
}

static LSTalk_Hover test_server_make_hover(LSTalk_MemoryAllocator* allocator) {
    LSTalk_Hover result;
    memset(&result, 0, sizeof(result));

    result.contents = string_alloc_copy("contents", allocator);
    result.range.start.line = 0;
    result.range.start.character = 0;
    result.range.end.line = 0;
    result.range.end.character = 5;

    return result;
}

static JSONValue test_server_build_response(JSONValue* request, LSTalk_MemoryAllocator* allocator) {
    JSONValue result = json_make_null();

    if (request == NULL || request->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue method = json_object_get(request, "method");
    if (method.type == JSON_VALUE_STRING) {
        result = json_make_object(allocator);
        JSONValue id = json_object_get(request, "id");

        char* method_str = method.value.string_value;
        JSONValue* params = json_object_get_ptr(request, "params");
        if (strcmp(method_str, "initialize") == 0) {
            json_object_const_key_set(&result, "id", id, allocator);
            JSONValue results = json_make_object(allocator);
            json_object_const_key_set(&result, "results", results, allocator);
            char version[40];
            sprintf_s(version, sizeof(version), "%d.%d.%d", LSTALK_MAJOR, LSTALK_MINOR, LSTALK_REVISION);
            JSONValue server_info = json_make_object(allocator);
            json_object_const_key_set(&server_info, "name", json_make_string_const("Test Server"), allocator);
            json_object_const_key_set(&server_info, "version", json_make_string(version, allocator), allocator);
            json_object_const_key_set(&results, "serverInfo", server_info, allocator);
            
            ServerCapabilities server_capabilities = test_server_make_capabilities();
            JSONValue capabilities = server_capabilities_json(&server_capabilities, allocator);
            json_object_const_key_set(&results, "capabilities", capabilities, allocator);
            server_capabilities_free(&server_capabilities, allocator);
        } else if (strcmp(method_str, "$/setTrace") == 0) {
            JSONValue value = json_object_get(params, "value");
            if (value.type == JSON_VALUE_STRING) {
                LSTalk_Trace trace = trace_from_string(value.value.string_value);
                json_destroy_value(&result, allocator);
                if (trace == LSTALK_TRACE_MESSAGES) {
                    JSONValue result_params = json_make_object(allocator);
                    json_object_const_key_set(&result_params, "message", json_make_string_const("message"), allocator);
                    result = rpc_make_notification("$/logTrace", result_params, allocator);
                } else if (trace == LSTALK_TRACE_VERBOSE) {
                    JSONValue result_params = json_make_object(allocator);
                    json_object_const_key_set(&result_params, "message", json_make_string_const("message"), allocator);
                    json_object_const_key_set(&result_params, "verbose", json_make_string_const("verbose"), allocator);
                    result = rpc_make_notification("$/logTrace", result_params, allocator);
                }
            }
        } else if (strcmp(method_str, "textDocument/documentSymbol") == 0) {
            LSTalk_DocumentSymbolNotification notification = test_server_make_document_symbols(allocator);
            JSONValue results = document_symbol_notification_json(&notification, allocator);
            json_object_const_key_set(&result, "id", id, allocator);
            json_object_const_key_set(&result, "result", results, allocator);
            document_symbol_notification_free(&notification, allocator);
        } else if (strcmp(method_str, "textDocument/semanticTokens/full") == 0) {
            LSTalk_SemanticTokens notification = test_server_make_semantic_tokens(allocator);
            SemanticTokensLegend legend;
            memset(&legend, 0, sizeof(legend));
            JSONValue results = semantic_tokens_json(&notification, &legend, allocator);
            json_object_const_key_set(&result, "id", id, allocator);
            json_object_const_key_set(&result, "result", results, allocator);
            semantic_tokens_free(&notification, allocator);
        } else if (strcmp(method_str, "textDocument/hover") == 0) {
            LSTalk_Hover notification = test_server_make_hover(allocator);
            JSONValue results = hover_json(&notification, allocator);
            json_object_const_key_set(&result, "id", id, allocator);
            json_object_const_key_set(&result, "result", results, allocator);
            hover_free(&notification, allocator);
        }
    }

    return result;
}

static void test_server_send_response(JSONValue* response, LSTalk_MemoryAllocator* allocator) {
    if (response == NULL || response->type == JSON_VALUE_NULL) {
        return;
    }

    JSONEncoder encoder = json_encode(response, allocator);
    if (encoder.string.length > 0) {
        printf("Content-Length: %zu\r\n%s\r\n", encoder.string.length, encoder.string.data);
        fflush(stdout);
    }

    json_destroy_encoder(&encoder, allocator);
}

void lstalk_test_server(int argc, char** argv) {
    (void)argc;
    (void)argv;

    LSTalk_MemoryAllocator allocator = tests_default_allocator();
    Message message = message_create();
    while (1) {
        char* request = file_async_read_stdin(&allocator);
        if (request != NULL) {
            char* anchor = request;
            while (anchor != NULL) {
                JSONValue value = message_to_json(&message, &anchor, &allocator);

                JSONValue response = test_server_build_response(&value, &allocator);
                test_server_send_response(&response, &allocator);

                json_destroy_value(&value, &allocator);
            }
            allocator.free(request);
        }
    }
    message_free(&message, &allocator);
}

#endif
