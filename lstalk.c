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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Version information
//

#define LSTALK_MAJOR 0
#define LSTALK_MINOR 0
#define LSTALK_REVISION 1

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
    #include <fcntl.h>
    #include <signal.h>
    #include <unistd.h>
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

static Vector vector_create(size_t element_size) {
    Vector result;
    result.element_size = element_size;
    result.length = 0;
    result.capacity = 1;
    result.data = malloc(element_size * result.capacity);
    return result;
}

static void vector_destroy(Vector* vector) {
    if (vector == NULL) {
        return;
    }

    if (vector->data != NULL) {
        free(vector->data);
    }

    vector->data = NULL;
    vector->element_size = 0;
    vector->length = 0;
    vector->capacity = 0;
}

static void vector_resize(Vector* vector, size_t capacity) {
    if (vector == NULL || vector->element_size == 0) {
        return;
    }

    vector->capacity = capacity;
    vector->data = realloc(vector->data, vector->element_size * vector->capacity);
}

static void vector_push(Vector* vector, void* element) {
    if (vector == NULL || element == NULL || vector->element_size == 0) {
        return;
    }

    if (vector->length == vector->capacity) {
        vector_resize(vector, vector->capacity * 2);
    }

    size_t offset = vector->length * vector->element_size;
    memcpy(vector->data + offset, element, vector->element_size);
    vector->length++;
}

static void vector_append(Vector* vector, void* elements, size_t count) {
    if (vector == NULL || elements == NULL || vector->element_size == 0) {
        return;
    }

    size_t remaining = vector->capacity - vector->length;

    if (count > remaining) {
        vector_resize(vector, vector->capacity + (count - remaining) * 2);
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
    size_t count = vector->length - index + 1;
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

static char* string_alloc_copy(const char* source) {
    size_t length = strlen(source);
    char* result = (char*)malloc(length + 1);
    strcpy(result, source);
    return result;
}

static void string_free_array(char** array, size_t count) {
    if (array == NULL) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (array[i] != NULL) {
            free(array[i]);
        }
    }

    free(array);
}

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

static Process* process_create_windows(const char* path) {
    StdHandles handles;
    handles.child_stdin_read = NULL;
    handles.child_stdin_write = NULL;
    handles.child_stdout_read = NULL;
    handles.child_stdout_write = NULL;

    SECURITY_ATTRIBUTES security_attr;
    ZeroMemory(&security_attr, sizeof(security_attr));
    security_attr.bInheritHandle = TRUE;
    security_attr.lpSecurityDescriptor = NULL;

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

    // CreateProcessW does not accept a path larger than 32767.
    wchar_t wpath[PATH_MAX];
    mbstowcs(wpath, path, PATH_MAX);

    BOOL result = CreateProcessW(wpath, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &startup_info, &process_info);
    if (!result) {
        printf("Failed to create child process.\n");
        process_close_handles(&handles);
        return NULL;
    }

    // This is temporary to allow child process to startup.
    Sleep(500);

    Process* process = (Process*)malloc(sizeof(Process));
    process->std_handles = handles;
    process->info = process_info;
    return process;
}

static void process_close_windows(Process* process) {
    if (process == NULL) {
        return;
    }

    TerminateProcess(process->info.hProcess, 0);
    process_close_handles(&process->std_handles);
    free(process);
}

static char* process_read_windows(Process* process) {
    if (process == NULL) {
        return NULL;
    }

    DWORD total_bytes_avail = 0;
    if (!PeekNamedPipe(process->std_handles.child_stdout_read, NULL, 0, NULL, &total_bytes_avail, NULL)) {
        printf("Failed to peek for number of bytes!\n");
        return NULL;
    }

    if (total_bytes_avail == 0) {
        return NULL;
    }

    char* read_buffer = (char*)malloc(sizeof(char) * total_bytes_avail + 1);
    DWORD read = 0;
    BOOL read_result = ReadFile(process->std_handles.child_stdout_read, read_buffer, total_bytes_avail, &read, NULL);
    if (!read_result || read == 0) {
        printf("Failed to read from process stdout.\n");
        free(read_buffer);
        return NULL;
    }
    read_buffer[read] = 0;
    return read_buffer;
}

static void process_write_windows(Process* process, const char* request) {
    if (process == NULL) {
        return;
    }

    DWORD written = 0;
    if (!WriteFile(process->std_handles.child_stdin_write, (void*)request, strlen(request), &written, NULL)) {
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

static Process* process_create_posix(const char* path) {
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

        char** args = NULL;
        int error = execv(path, args);
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

    Process* process = (Process*)malloc(sizeof(Process));
    process->pipes = pipes;
    process->pid = pid;

    sleep(1);

    return process;
}

static void process_close_posix(Process* process) {
    if (process == NULL) {
        return;
    }

    process_close_pipes(&process->pipes);
    kill(process->pid, SIGKILL);
    free(process);
}

#define READ_SIZE 4096

static char* process_read_posix(Process* process) {
    if (process == NULL) {
        return NULL;
    }

    Vector array = vector_create(sizeof(char));
    char buffer[READ_SIZE];
    int bytes_read = read(process->pipes.out[PIPE_READ], (void*)buffer, sizeof(buffer));
    if (bytes_read == -1) {
        vector_destroy(&array);
        return NULL;
    }

    while (bytes_read > 0) {
        vector_append(&array, (void*)buffer, (size_t)bytes_read);
        if (bytes_read < READ_SIZE) {
            break;
        }

        bytes_read = read(process->pipes.out[PIPE_READ], (void*)buffer, sizeof(buffer));
    }

    char* result = (char*)malloc(sizeof(char) * array.length + 1);
    strncpy(result, array.data, array.length);
    result[array.length] = 0;
    vector_destroy(&array);
    return result;
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

static Process* process_create(const char* path) {
#if LSTALK_WINDOWS
    return process_create_windows(path);
#elif LSTALK_POSIX
    return process_create_posix(path);
#else
    #error "Current platform does not implement create_process"
#endif
}

static void process_close(Process* process) {
#if LSTALK_WINDOWS
    process_close_windows(process);
#elif LSTALK_POSIX
    process_close_posix(process);
#else
    #error "Current platform does not implement close_process"
#endif
}

// Platform specific handling should allocate the string on the heap
// and the caller is responsible for freeing the result.
static char* process_read(Process* process) {
#if LSTALK_WINDOWS
    return process_read_windows(process);
#elif LSTALK_POSIX
    process_read_posix(process);
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

static void process_request(Process* process, const char* request) {
    const char* content_length = "Content-Length:";
    size_t length = strlen(request);

    // Temporary buffer length.
    // TODO: Is there a way to eliminate this heap allocation?
    char* buffer = (char*)malloc(length + 40);
    sprintf(buffer, "Content-Length: %zu\r\n\r\n%s", length, request);
    process_write(process, buffer);
    free(buffer);
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

static const char* json_type_to_string(JSON_VALUE_TYPE type) {
    switch (type) {
        case JSON_VALUE_BOOLEAN: return "BOOLEAN";
        case JSON_VALUE_INT: return "INT";
        case JSON_VALUE_FLOAT: return "FLOAT";
        case JSON_VALUE_STRING: return "STRING";
        case JSON_VALUE_STRING_CONST: return "STRING CONST";
        case JSON_VALUE_OBJECT: return "OBJECT";
        case JSON_VALUE_ARRAY: return "ARRAY";
        case JSON_VALUE_NULL:
        default: break;
    }

    return "NULL";
}

struct JSONObject;
struct JSONArray;

typedef struct JSONValue {
    union {
        char bool_value;
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

static void json_to_string(JSONValue* value, Vector* vector);
static void json_object_to_string(JSONObject* object, Vector* vector) {
    if (object == NULL || vector == NULL || vector->element_size != 1) {
        return;
    }

    vector_append(vector, (void*)"{", 1);
    for (size_t i = 0; i < object->pairs.length; i++) {
        JSONPair* pair = (JSONPair*)vector_get(&object->pairs, i);
        json_to_string(&pair->key, vector);
        vector_append(vector, (void*)": ", 2);
        json_to_string(&pair->value, vector);

        if (i + 1 < object->pairs.length) {
            vector_append(vector, (void*)", ", 2);
        }
    }
    vector_append(vector, (void*)"}", 1);
}

static void json_array_to_string(JSONArray* array, Vector* vector) {
    if (array == NULL || vector == NULL || vector->element_size != 1) {
        return;
    }

    vector_append(vector, (void*)"[", 1);
    for (size_t i = 0; i < array->values.length; i++) {
        JSONValue* value = (JSONValue*)vector_get(&array->values, i);
        json_to_string(value, vector);

        if (i + 1 < array->values.length) {
            vector_append(vector, (void*)", ", 2);
        }
    }
    vector_append(vector, (void*)"]", 1);
}

static void json_to_string(JSONValue* value, Vector* vector) {
    // The vector object must be created with an element size of 1.

    if (value == NULL || vector == NULL || vector->element_size != 1) {
        return;
    }

    switch (value->type) {
        case JSON_VALUE_BOOLEAN: {
            if (value->value.bool_value) {
                vector_append(vector, (void*)"true", 4);
            } else {
                vector_append(vector, (void*)"false", 5);
            }
        } break;

        case JSON_VALUE_INT: {
            char buffer[40];
            sprintf(buffer, "%d", value->value.int_value);
            vector_append(vector, (void*)buffer, strlen(buffer));
        } break;

        case JSON_VALUE_FLOAT: {
            char buffer[40];
            sprintf(buffer, "%f", value->value.float_value);
            vector_append(vector, (void*)buffer, strlen(buffer));
        } break;

        case JSON_VALUE_STRING_CONST:
        case JSON_VALUE_STRING: {
            vector_append(vector, (void*)"\"", 1);
            vector_append(vector, (void*)value->value.string_value, strlen(value->value.string_value));
            vector_append(vector, (void*)"\"", 1);
        } break;

        case JSON_VALUE_OBJECT: {
            json_object_to_string(value->value.object_value, vector);
        } break;

        case JSON_VALUE_ARRAY: {
            json_array_to_string(value->value.array_value, vector);
        } break;

        case JSON_VALUE_NULL: {
            vector_append(vector, (void*)"null", 4);
        } break;

        default: break;
    }
}

static void json_destroy_value(JSONValue* value) {
    if (value == NULL) {
        return;
    }

    switch (value->type) {
        case JSON_VALUE_STRING: {
            if (value->value.string_value != NULL) {
                free(value->value.string_value);
            }
        } break;

        case JSON_VALUE_OBJECT: {
            JSONObject* object = value->value.object_value;
            if (object != NULL) {
                for (size_t i = 0; i < object->pairs.length; i++) {
                    JSONPair* pair = (JSONPair*)vector_get(&object->pairs, i);
                    json_destroy_value(&pair->key);
                    json_destroy_value(&pair->value);
                }
                vector_destroy(&object->pairs);
                free(object);
            }
        } break;

        case JSON_VALUE_ARRAY: {
            JSONArray* array = value->value.array_value;
            if (array != NULL) {
                for (size_t i = 0; i < array->values.length; i++) {
                    JSONValue* item = (JSONValue*)vector_get(&array->values, i);
                    json_destroy_value(item);
                }
                vector_destroy(&array->values);
                free(array);
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

static JSONValue json_make_boolean(char value) {
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

static JSONValue json_make_string(char* value) {
    if (value == NULL) {
        return json_make_null();
    }

    JSONValue result;
    result.type = JSON_VALUE_STRING;
    result.value.string_value = string_alloc_copy(value);
    return result;
}

static JSONValue json_make_string_const(char* value) {
    JSONValue result;
    result.type = JSON_VALUE_STRING_CONST;
    result.value.string_value = value;
    return result;
}

static JSONValue json_make_object() {
    JSONValue result;
    result.type = JSON_VALUE_OBJECT;
    result.value.object_value = (JSONObject*)malloc(sizeof(JSONObject));
    result.value.object_value->pairs = vector_create(sizeof(JSONPair));
    return result;
}

static JSONValue json_make_array() {
    JSONValue result;
    result.type = JSON_VALUE_ARRAY;
    result.value.array_value = (JSONArray*)malloc(sizeof(JSONArray));
    result.value.array_value->values = vector_create(sizeof(JSONValue));
    return result;
}

static JSONValue json_object_get(JSONValue* object, char* key) {
    JSONValue result = json_make_null();

    if (object == NULL || object->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONObject* obj = object->value.object_value;
    for (size_t i = 0; i < obj->pairs.length; i++) {
        JSONPair* pair = (JSONPair*)vector_get(&obj->pairs, i);

        if (strcmp(pair->key.value.string_value, key) == 0) {
            result = pair->value;
            break;
        }
    }

    return result;
}

static void json_object_set(JSONValue* object, JSONValue key, JSONValue value) {
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
            json_destroy_value(&pair->value);
            pair->value = value;
            found = 1;
            break;
        }
    }

    if (!found) {
        JSONPair pair;
        pair.key = key;
        pair.value = value;
        vector_push(&object->value.object_value->pairs, (void*)&pair);
    }
}

static void json_object_key_set(JSONValue* object, char* key, JSONValue value) {
    json_object_set(object, json_make_string(key), value);
}

static void json_object_const_key_set(JSONValue* object, char* key, JSONValue value) {
    json_object_set(object, json_make_string_const(key), value);
}

static void json_array_push(JSONValue* array, JSONValue value) {
    if (array == NULL || array->type != JSON_VALUE_ARRAY) {
        return;
    }

    JSONArray* arr = array->value.array_value;
    vector_push(&arr->values, (void*)&value);
}

static JSONValue json_array_get(JSONValue* array, size_t index) {
    JSONValue result = json_make_null();

    if (array == NULL || array->type != JSON_VALUE_ARRAY) {
        return result;
    }

    JSONArray* arr = array->value.array_value;

    if (index >= arr->values.length) {
        return result;
    }

    result = *(JSONValue*)vector_get(&arr->values, index);

    return result;
}

static JSONEncoder json_encode(JSONValue* value) {
    JSONEncoder encoder;
    encoder.string = vector_create(sizeof(char));
    json_to_string(value, &encoder.string);
    vector_append(&encoder.string, (void*)"\0", 1);
    return encoder;
}

static void json_destroy_encoder(JSONEncoder* encoder) {
    vector_destroy(&encoder->string);
}

static void json_print(JSONValue* value) {
    if (value == NULL) {
        return;
    }

    JSONEncoder encoder  = json_encode(value);
    printf("%s\n", encoder.string.data);
    json_destroy_encoder(&encoder);
}

//
// JSON Parsing Functions
//
// This section contains functions that parses a JSON stream into a JSONValue.

typedef struct Lexer {
    char* buffer;
    char* delimiters;
    char* ptr;
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

static char* token_make_string(Token* token) {
    if (token == NULL) {
        return NULL;
    }

    size_t length = token->length + 1;
    // Iterate through the token string and remove any escape characters.
    int is_escaped = 0;
    for (size_t i = 0; i < token->length; i++) {
        if (token->ptr[i] == '\\') {
            if (!is_escaped) {
                length--;
            }
            is_escaped = is_escaped > 0 ? 0 : 1;
        } else {
            is_escaped = 0;
        }
    }

    // The next steps will attempt to copy sub-strings ignoring all escape characters.
    char* result = (char*)malloc(sizeof(char) * length);
    char* dest = result;
    char* ptr = token->ptr;
    is_escaped = 0;
    for (size_t i = 0; i < token->length; i++) {
        if (token->ptr[i] == '\\') {
            if (!is_escaped) {
                size_t count = (token->ptr + i) - ptr;
                strncpy(dest, ptr, count);
                dest += count;
                ptr += count + 1;
            }
            is_escaped = is_escaped > 0 ? 0 : 1;
        } else {
            is_escaped = 0;
        }
    }
    strncpy(dest, ptr, (token->ptr + token->length) - ptr);
    result[length - 1] = 0;
    return result;
}

static Lexer lexer_init(char* buffer, char* delimiters) {
    Lexer result;
    result.buffer = buffer;
    result.delimiters = delimiters;
    result.ptr = buffer;
    return result;
}

static Token lexer_get_token(Lexer* lexer) {
    Token result;
    result.ptr = NULL;
    result.length = 0;

    int length = 0;
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
                int length = ptr - lexer->ptr;
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

    result = json_make_object();

    // Could be an empty object.
    while (!token_compare(&token, "}")) {
        if (!token_compare(&token, "\"")) {
            json_destroy_value(&result);
            return result;
        }

        token = lexer_parse_until(lexer, '"');
        JSONValue key;
        key.type = JSON_VALUE_STRING;
        key.value.string_value = token_make_string(&token);

        token = lexer_get_token(lexer);
        if (!token_compare(&token, ":")) {
            json_destroy_value(&key);
            json_destroy_value(&result);
            return result;
        }

        token = lexer_get_token(lexer);
        JSONValue value = json_decode_value(&token, lexer);
        json_object_set(&result, key, value);

        token = lexer_get_token(lexer);
        if (token_compare(&token, "}")) {
            break;
        }

        if (!token_compare(&token, ",")) {
            json_destroy_value(&result);
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

    result = json_make_array();

    Token token = lexer_get_token(lexer);
    while (!token_compare(&token, "]")) {
        JSONValue value = json_decode_value(&token, lexer);
        json_array_push(&result, value);

        token = lexer_get_token(lexer);
        if (token_compare(&token, "]")) {
            break;
        }

        if (!token_compare(&token, ",")) {
            json_destroy_value(&result);
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
            result.value.string_value = token_make_string(&literal);
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

static JSONValue json_decode(char* stream) {
    Lexer lexer;
    lexer.buffer = stream;
    lexer.delimiters = "\":{}[],";
    lexer.ptr = stream;

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

static void rpc_message(JSONValue* object) {
    if (object == NULL || object->type != JSON_VALUE_OBJECT) {
        return;
    }

    json_object_const_key_set(object, "jsonrpc", json_make_string_const("2.0"));
}

static Request rpc_make_notification(char* method, JSONValue params) {
    Request result;
    result.id = 0;
    result.payload = json_make_null();

    if (method == NULL) {
        return result;
    }

    JSONValue object = json_make_object();
    rpc_message(&object);
    json_object_const_key_set(&object, "method", json_make_string_const(method));

    if (params.type == JSON_VALUE_OBJECT || params.type == JSON_VALUE_ARRAY) {
        json_object_const_key_set(&object, "params", params);
    }

    result.payload = object;
    return result;
}

static Request rpc_make_request(int* id, char* method, JSONValue params) {
    Request result;
    result.id = 0;
    result.payload = json_make_null();

    if (id == NULL) {
        return result;
    }

    result = rpc_make_notification(method, params);
    json_object_const_key_set(&result.payload, "id", json_make_int(*id));
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

static void rpc_send_request(Process* server, Request* request, int print_request) {
    if (server == NULL || request == NULL) {
        return;
    }

    JSONEncoder encoder = json_encode(&request->payload);
    process_request(server, encoder.string.data);
    if (print_request) {
        printf("%s\n", encoder.string.data);
    }
    json_destroy_encoder(&encoder);
}

static void rpc_close_request(Request* request) {
    if (request == NULL) {
        return;
    }

    json_destroy_value(&request->payload);
}

//
// lstalk API
//
// This is the beginning of the exposed API functions for the library.

typedef struct Server {
    LSTalk_ServerID id;
    Process* process;
    LSTalk_ConnectionStatus connection_status;
    Vector requests;
    int request_id;
    LSTalk_ServerInfo info;
} Server;

typedef struct ClientInfo {
    char* name;
    char* version;
} ClientInfo;

static void client_info_clear(ClientInfo* info) {
    if (info == NULL) {
        return;
    }

    if (info->name != NULL) {
        free(info->name);
    }

    if (info->version != NULL) {
        free(info->version);
    }
}

static JSONValue client_info(ClientInfo* info) {
    if (info == NULL) {
        return json_make_null();
    }

    JSONValue result = json_make_object();
    json_object_const_key_set(&result, "name", json_make_string_const(info->name));
    json_object_const_key_set(&result, "version", json_make_string_const(info->version));
    return result;
}

typedef struct LSTalk_Context {
    Vector servers;
    LSTalk_ServerID server_id;
    ClientInfo client_info;
    char* locale;
    LSTalk_ClientCapabilities client_capabilities;
    int debug_flags;
} LSTalk_Context;

static void server_free_static_registration(LSTalk_StaticRegistrationOptions* static_registration) {
    if (static_registration == NULL || static_registration->id == NULL) {
        return;
    }

    free(static_registration->id);
}

static void server_free_capabilities(LSTalk_ServerCapabilities* capabilities) {
    for (int i = 0; i < capabilities->notebook_document_sync.notebook_selector_count; i++) {
        LSTalk_NotebookSelector* selector = &capabilities->notebook_document_sync.notebook_selector[i];

        if (selector->notebook.notebook_type != NULL) {
            free(selector->notebook.notebook_type);
        }

        if (selector->notebook.scheme != NULL) {
            free(selector->notebook.scheme);
        }

        if (selector->notebook.pattern != NULL) {
            free(selector->notebook.pattern);
        }

        string_free_array(selector->cells, selector->cells_count);
    }

    server_free_static_registration(&capabilities->notebook_document_sync.static_registration);

    if (capabilities->notebook_document_sync.notebook_selector != NULL) {
        free(capabilities->notebook_document_sync.notebook_selector);
    }

    string_free_array(capabilities->completion_provider.trigger_characters, capabilities->completion_provider.trigger_characters_count);
    string_free_array(capabilities->completion_provider.all_commit_characters, capabilities->completion_provider.all_commit_characters_count);
    string_free_array(capabilities->signature_help_provider.trigger_characters, capabilities->signature_help_provider.trigger_characters_count);
    string_free_array(capabilities->signature_help_provider.retrigger_characters, capabilities->signature_help_provider.retrigger_characters_count);
}

static void server_close(Server* server) {
    if (server == NULL) {
        return;
    }

    process_close(server->process);

    for (size_t i = 0; i < server->requests.length; i++) {
        Request* request = (Request*)vector_get(&server->requests, i);
        rpc_close_request(request);
    }
    vector_destroy(&server->requests);

    if (server->info.name != NULL) {
        free(server->info.name);
    }

    if (server->info.version != NULL) {
        free(server->info.version);
    }

    server_free_capabilities(&server->info.capabilities);
}

static void server_send_request(LSTalk_Context* context, Process* server, Request* request) {
    rpc_send_request(server, request, context->debug_flags & LSTALK_DEBUGFLAGS_PRINT_REQUESTS);
}

static char** parse_string_array(JSONValue* value, char* key, int* count) {
    if (value == NULL || value->type != JSON_VALUE_OBJECT || count == NULL) {
        return NULL;
    }

    JSONValue array = json_object_get(value, key);
    if (array.type != JSON_VALUE_ARRAY) {
        return NULL;
    }

    char** result = (char**)calloc(array.value.array_value->values.length, sizeof(char*));
    for (size_t i = 0; i < array.value.array_value->values.length; i++) {
        JSONValue item = json_array_get(value, i);
        if (item.type == JSON_VALUE_STRING) {
            result[i] = string_alloc_copy(item.value.string_value);
        }
    }

    *count = array.value.array_value->values.length;
    return result;
}

static LSTalk_WorkDoneProgressOptions parse_work_done_progress(JSONValue* value) {
    LSTalk_WorkDoneProgressOptions result;
    result.work_done_progress = 0;

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue work_done_progress = json_object_get(value, "workDoneProgress");
    if (work_done_progress.type != JSON_VALUE_BOOLEAN) {
        return result;
    }

    result.work_done_progress = work_done_progress.value.bool_value;
    return result;
}

static LSTalk_StaticRegistrationOptions parse_static_registration(JSONValue* value) {
    LSTalk_StaticRegistrationOptions result;
    result.id = NULL;

    if (value == NULL || value->type != JSON_VALUE_OBJECT) {
        return result;
    }

    JSONValue id = json_object_get(value, "id");
    if (id.type != JSON_VALUE_STRING) {
        return result;
    }

    result.id = string_alloc_copy(id.value.string_value);
    return result;
}

static LSTalk_PositionEncodingKind parse_position_encoding_kind(char* value);
static LSTalk_ServerInfo server_parse_initialized(JSONValue* value) {
    LSTalk_ServerInfo info;
    memset(&info, 0, sizeof(info));

    if (value == NULL) {
        return info;
    }

    JSONValue result = json_object_get(value, "result");
    if (result.type == JSON_VALUE_OBJECT) {
        JSONValue capabilities = json_object_get(&result, "capabilities");
        if (capabilities.type == JSON_VALUE_OBJECT) {
            JSONValue position_encoding = json_object_get(&capabilities, "positionEncoding");
            if (position_encoding.type == JSON_VALUE_STRING) {
                info.capabilities.position_encoding = parse_position_encoding_kind(position_encoding.value.string_value);
            } else {
                info.capabilities.position_encoding = LSTALK_POSITIONENCODINGKIND_UTF16;
            }

            JSONValue text_document_sync = json_object_get(&capabilities, "textDocumentSync");
            if (text_document_sync.type == JSON_VALUE_INT) {
                info.capabilities.text_document_sync.change = text_document_sync.value.int_value;
            } else if (text_document_sync.type == JSON_VALUE_OBJECT) {
                JSONValue open_close = json_object_get(&text_document_sync, "openClose");
                if (open_close.type == JSON_VALUE_BOOLEAN) {
                    info.capabilities.text_document_sync.open_close = open_close.value.bool_value;
                }

                JSONValue change = json_object_get(&text_document_sync, "change");
                if (change.type == JSON_VALUE_INT) {
                    info.capabilities.text_document_sync.change = change.value.int_value;
                }
            }

            JSONValue notebook_document_sync = json_object_get(&capabilities, "notebookDocumentSync");
            if (notebook_document_sync.type == JSON_VALUE_OBJECT) {
                info.capabilities.notebook_document_sync.static_registration = parse_static_registration(&notebook_document_sync);

                JSONValue notebook_selector = json_object_get(&notebook_document_sync, "notebookSelector");
                if (notebook_selector.type == JSON_VALUE_ARRAY) {
                    info.capabilities.notebook_document_sync.notebook_selector_count = notebook_selector.value.array_value->values.length;
                    if (info.capabilities.notebook_document_sync.notebook_selector_count > 0) {
                        LSTalk_NotebookSelector* selectors = (LSTalk_NotebookSelector*)calloc(info.capabilities.notebook_document_sync.notebook_selector_count, sizeof(LSTalk_NotebookSelector));
                        for (size_t i = 0; i < notebook_selector.value.array_value->values.length; i++) {
                            JSONValue item = json_array_get(&notebook_selector, i);
                            if (item.type != JSON_VALUE_OBJECT) {
                                continue;
                            }
                            JSONValue notebook = json_object_get(&item, "notebook");
                            if (notebook.type == JSON_VALUE_STRING) {
                                selectors[i].notebook.notebook_type = string_alloc_copy(notebook.value.string_value);
                            } else if (notebook.type == JSON_VALUE_OBJECT) {
                                JSONValue notebook_type = json_object_get(&notebook, "notebookType");
                                if (notebook_type.type == JSON_VALUE_STRING) {
                                    selectors[i].notebook.notebook_type = string_alloc_copy(notebook_type.value.string_value);
                                }
                                JSONValue scheme = json_object_get(&notebook, "scheme");
                                if (scheme.type == JSON_VALUE_STRING) {
                                    selectors[i].notebook.scheme = string_alloc_copy(scheme.value.string_value);
                                }
                                JSONValue pattern = json_object_get(&notebook, "pattern");
                                if (pattern.type == JSON_VALUE_STRING) {
                                    selectors[i].notebook.pattern = string_alloc_copy(pattern.value.string_value);
                                }
                            }

                            selectors[i].cells = parse_string_array(&item, "cells", &selectors[i].cells_count);
                        }
                        info.capabilities.notebook_document_sync.notebook_selector = selectors;
                    }
                }

                JSONValue save = json_object_get(&notebook_document_sync, "save");
                if (save.type == JSON_VALUE_BOOLEAN) {
                    info.capabilities.notebook_document_sync.save = save.value.bool_value;
                }
            }

            JSONValue completion_provider = json_object_get(&capabilities, "completionProvider");
            if (completion_provider.type == JSON_VALUE_OBJECT) {
                info.capabilities.completion_provider.work_done_progress = parse_work_done_progress(&completion_provider);

                info.capabilities.completion_provider.trigger_characters = 
                    parse_string_array(&completion_provider, "triggerCharacters", &info.capabilities.completion_provider.trigger_characters_count);

                info.capabilities.completion_provider.all_commit_characters =
                    parse_string_array(&completion_provider, "allCommitCharacters", &info.capabilities.completion_provider.all_commit_characters_count);

                JSONValue resolve_provider = json_object_get(&completion_provider, "resolveProvider");
                if (resolve_provider.type == JSON_VALUE_BOOLEAN) {
                    info.capabilities.completion_provider.resolve_provider = resolve_provider.value.bool_value;
                }

                JSONValue completion_item = json_object_get(&completion_provider, "completionItem");
                if (completion_item.type == JSON_VALUE_OBJECT) {
                    JSONValue label_data_support = json_object_get(&completion_item, "labelDataSupport");
                    if (label_data_support.type == JSON_VALUE_BOOLEAN) {
                        info.capabilities.completion_provider.completion_item_label_details_support = label_data_support.value.bool_value;
                    }
                }
            }

            JSONValue hover_provider = json_object_get(&capabilities, "hoverProvider");
            if (hover_provider.type == JSON_VALUE_BOOLEAN) {
                info.capabilities.hover_provider.is_supported = hover_provider.value.bool_value;
            } else if (hover_provider.type == JSON_VALUE_OBJECT) {
                info.capabilities.hover_provider.work_done_progress = parse_work_done_progress(&hover_provider);
            }

            JSONValue signature_help_provider = json_object_get(&capabilities, "signatureHelpProvider");
            if (signature_help_provider.type == JSON_VALUE_OBJECT) {
                info.capabilities.signature_help_provider.work_done_progress = parse_work_done_progress(&signature_help_provider);
                info.capabilities.signature_help_provider.trigger_characters =
                    parse_string_array(&signature_help_provider, "triggerCharacters", &info.capabilities.signature_help_provider.trigger_characters_count);
                info.capabilities.signature_help_provider.retrigger_characters =
                    parse_string_array(&signature_help_provider, "retriggerCharacters", &info.capabilities.signature_help_provider.retrigger_characters_count);
            }
        }

        JSONValue server_info = json_object_get(&result, "serverInfo");
        if (server_info.type == JSON_VALUE_OBJECT) {
            JSONValue name = json_object_get(&server_info, "name");
            if (name.type == JSON_VALUE_STRING) {
                info.name = string_alloc_copy(name.value.string_value);
            }

            JSONValue version = json_object_get(&server_info, "version");
            if (version.type == JSON_VALUE_STRING) {
                info.version = string_alloc_copy(version.value.string_value);
            }
        }
    }

    return info;
}

static char* trace_to_string(LSTalk_Trace trace) {
    switch (trace) {
        case LSTALK_TRACE_MESSAGES: return "messages";
        case LSTALK_TRACE_VERBOSE: return "verbose";
        case LSTALK_TRACE_OFF:
        default: break;
    }

    return "off";
}

static JSONValue resource_operation_kind_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_RESOURCEOPERATIONKIND_CREATE) {
        json_array_push(&result, json_make_string_const("create"));
    }

    if (value & LSTALK_RESOURCEOPERATIONKIND_RENAME) {
        json_array_push(&result, json_make_string_const("rename"));
    }

    if (value & LSTALK_RESOURCEOPERATIONKIND_DELETE) {
        json_array_push(&result, json_make_string_const("delete"));
    }

    return result;
}

static JSONValue failure_handling_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_FAILUREHANDLINGKIND_ABORT) {
        json_array_push(&result, json_make_string_const("abort"));
    }

    if (value & LSTALK_FAILUREHANDLINGKIND_TRANSACTIONAL) {
        json_array_push(&result, json_make_string_const("transactional"));
    }

    if (value & LSTALK_FAILUREHANDLINGKIND_TEXTONLYTRANSACTIONAL) {
        json_array_push(&result, json_make_string_const("textOnlyTransactional"));
    }

    if (value & LSTALK_FAILUREHANDLINGKIND_UNDO) {
        json_array_push(&result, json_make_string_const("undo"));
    }

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

static JSONValue symbol_kind_array(long long value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_SYMBOLKIND_FILE) { json_array_push(&result, json_make_int(SYMBOLKIND_File)); }
    if (value & LSTALK_SYMBOLKIND_MODULE) { json_array_push(&result, json_make_int(SYMBOLKIND_Module)); }
    if (value & LSTALK_SYMBOLKIND_NAMESPACE) { json_array_push(&result, json_make_int(SYMBOLKIND_Namespace)); }
    if (value & LSTALK_SYMBOLKIND_PACKAGE) { json_array_push(&result, json_make_int(SYMBOLKIND_Package)); }
    if (value & LSTALK_SYMBOLKIND_CLASS) { json_array_push(&result, json_make_int(SYMBOLKIND_Class)); }
    if (value & LSTALK_SYMBOLKIND_METHOD) { json_array_push(&result, json_make_int(SYMBOLKIND_Method)); }
    if (value & LSTALK_SYMBOLKIND_PROPERTY) { json_array_push(&result, json_make_int(SYMBOLKIND_Property)); }
    if (value & LSTALK_SYMBOLKIND_FIELD) { json_array_push(&result, json_make_int(SYMBOLKIND_Field)); }
    if (value & LSTALK_SYMBOLKIND_CONSTRUCTOR) { json_array_push(&result, json_make_int(SYMBOLKIND_Constructor)); }
    if (value & LSTALK_SYMBOLKIND_ENUM) { json_array_push(&result, json_make_int(SYMBOLKIND_Enum)); }
    if (value & LSTALK_SYMBOLKIND_INTERFACE) { json_array_push(&result, json_make_int(SYMBOLKIND_Interface)); }
    if (value & LSTALK_SYMBOLKIND_FUNCTION) { json_array_push(&result, json_make_int(SYMBOLKIND_Function)); }
    if (value & LSTALK_SYMBOLKIND_VARIABLE) { json_array_push(&result, json_make_int(SYMBOLKIND_Variable)); }
    if (value & LSTALK_SYMBOLKIND_CONSTANT) { json_array_push(&result, json_make_int(SYMBOLKIND_Constant)); }
    if (value & LSTALK_SYMBOLKIND_STRING) { json_array_push(&result, json_make_int(SYMBOLKIND_String)); }
    if (value & LSTALK_SYMBOLKIND_NUMBER) { json_array_push(&result, json_make_int(SYMBOLKIND_Number)); }
    if (value & LSTALK_SYMBOLKIND_BOOLEAN) { json_array_push(&result, json_make_int(SYMBOLKIND_Boolean)); }
    if (value & LSTALK_SYMBOLKIND_ARRAY) { json_array_push(&result, json_make_int(SYMBOLKIND_Array)); }
    if (value & LSTALK_SYMBOLKIND_OBJECT) { json_array_push(&result, json_make_int(SYMBOLKIND_Object)); }
    if (value & LSTALK_SYMBOLKIND_KEY) { json_array_push(&result, json_make_int(SYMBOLKIND_Key)); }
    if (value & LSTALK_SYMBOLKIND_NULL) { json_array_push(&result, json_make_int(SYMBOLKIND_Null)); }
    if (value & LSTALK_SYMBOLKIND_ENUMMEMBER) { json_array_push(&result, json_make_int(SYMBOLKIND_EnumMember)); }
    if (value & LSTALK_SYMBOLKIND_STRUCT) { json_array_push(&result, json_make_int(SYMBOLKIND_Struct)); }
    if (value & LSTALK_SYMBOLKIND_EVENT) { json_array_push(&result, json_make_int(SYMBOLKIND_Event)); }
    if (value & LSTALK_SYMBOLKIND_OPERATOR) { json_array_push(&result, json_make_int(SYMBOLKIND_Operator)); }
    if (value & LSTALK_SYMBOLKIND_TYPEPARAMETER) { json_array_push(&result, json_make_int(SYMBOLKIND_TypeParameter)); }

    return result;
}

typedef enum {
    SYMBOLTAG_Deprecated = 1,
} SymbolTag;

static JSONValue symbol_tag_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_SYMBOLTAG_DEPRECATED) { json_array_push(&result, json_make_int(SYMBOLTAG_Deprecated)); }

    return result;
}

static JSONValue markup_kind_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_MARKUPKIND_PLAINTEXT) { json_array_push(&result, json_make_string_const("plaintext")); }
    if (value & LSTALK_MARKUPKIND_MARKDOWN) { json_array_push(&result, json_make_string_const("markdown")); }

    return result;
}

typedef enum {
    COMPLETIONITEMTAG_Deprecated = 1,
} CompletionItemTag;

static JSONValue completion_item_tag_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_COMPLETIONITEMTAG_DEPRECATED) { json_array_push(&result, json_make_int(COMPLETIONITEMTAG_Deprecated)); }

    return result;
}

typedef enum {
    INSERTTEXTMODE_AsIs = 1,
    INSERTTEXTMODE_AdjustIndentation2,
} InsertTextMode;

static JSONValue insert_text_mode_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_INSERTTEXTMODE_ASIS) { json_array_push(&result, json_make_int(INSERTTEXTMODE_AsIs)); }
    if (value & LSTALK_INSERTTEXTMODE_ADJUSTINDENTATION) { json_array_push(&result, json_make_int(INSERTTEXTMODE_AdjustIndentation2)); }

    return result;
}

typedef enum {
    COMPLETIONITEMKIND_Text = 1,
    COMPLETIONITEMKIND_Method = 2,
    COMPLETIONITEMKIND_Function = 3,
    COMPLETIONITEMKIND_Constructor = 4,
    COMPLETIONITEMKIND_Field = 5,
    COMPLETIONITEMKIND_Variable = 6,
    COMPLETIONITEMKIND_Class = 7,
    COMPLETIONITEMKIND_Interface = 8,
    COMPLETIONITEMKIND_Module = 9,
    COMPLETIONITEMKIND_Property = 10,
    COMPLETIONITEMKIND_Unit = 11,
    COMPLETIONITEMKIND_Value = 12,
    COMPLETIONITEMKIND_Enum = 13,
    COMPLETIONITEMKIND_Keyword = 14,
    COMPLETIONITEMKIND_Snippet = 15,
    COMPLETIONITEMKIND_Color = 16,
    COMPLETIONITEMKIND_File = 17,
    COMPLETIONITEMKIND_Reference = 18,
    COMPLETIONITEMKIND_Folder = 19,
    COMPLETIONITEMKIND_EnumMember = 20,
    COMPLETIONITEMKIND_Constant = 21,
    COMPLETIONITEMKIND_Struct = 22,
    COMPLETIONITEMKIND_Event = 23,
    COMPLETIONITEMKIND_Operator = 24,
    COMPLETIONITEMKIND_TypeParameter = 25,
} CompletionItemKind;

static JSONValue completion_item_kind_array(long long value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_COMPLETIONITEMKIND_TEXT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Text)); }
    if (value & LSTALK_COMPLETIONITEMKIND_METHOD) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Method)); }
    if (value & LSTALK_COMPLETIONITEMKIND_FUNCTION) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Function)); }
    if (value & LSTALK_COMPLETIONITEMKIND_CONSTRUCTOR) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Constructor)); }
    if (value & LSTALK_COMPLETIONITEMKIND_FIELD) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Field)); }
    if (value & LSTALK_COMPLETIONITEMKIND_VARIABLE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Variable)); }
    if (value & LSTALK_COMPLETIONITEMKIND_CLASS) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Class)); }
    if (value & LSTALK_COMPLETIONITEMKIND_INTERFACE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Interface)); }
    if (value & LSTALK_COMPLETIONITEMKIND_MODULE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Module)); }
    if (value & LSTALK_COMPLETIONITEMKIND_PROPERTY) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Property)); }
    if (value & LSTALK_COMPLETIONITEMKIND_UNIT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Unit)); }
    if (value & LSTALK_COMPLETIONITEMKIND_VALUE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Value)); }
    if (value & LSTALK_COMPLETIONITEMKIND_ENUM) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Enum)); }
    if (value & LSTALK_COMPLETIONITEMKIND_KEYWORD) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Keyword)); }
    if (value & LSTALK_COMPLETIONITEMKIND_SNIPPET) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Snippet)); }
    if (value & LSTALK_COMPLETIONITEMKIND_COLOR) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Color)); }
    if (value & LSTALK_COMPLETIONITEMKIND_FILE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_File)); }
    if (value & LSTALK_COMPLETIONITEMKIND_REFERENCE) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Reference)); }
    if (value & LSTALK_COMPLETIONITEMKIND_FOLDER) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Folder)); }
    if (value & LSTALK_COMPLETIONITEMKIND_ENUMMEMBER) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_EnumMember)); }
    if (value & LSTALK_COMPLETIONITEMKIND_CONSTANT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Constant)); }
    if (value & LSTALK_COMPLETIONITEMKIND_STRUCT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Struct)); }
    if (value & LSTALK_COMPLETIONITEMKIND_EVENT) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Event)); }
    if (value & LSTALK_COMPLETIONITEMKIND_OPERATOR) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_Operator)); }
    if (value & LSTALK_COMPLETIONITEMKIND_TYPEPARAMETER) { json_array_push(&result, json_make_int(COMPLETIONITEMKIND_TypeParameter)); }

    return result;
}

static JSONValue code_action_kind_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_CODEACTIONKIND_EMPTY) { json_array_push(&result, json_make_string_const("")); }
    if (value & LSTALK_CODEACTIONKIND_QUICKFIX) { json_array_push(&result, json_make_string_const("quickfix")); }
    if (value & LSTALK_CODEACTIONKIND_REFACTOR) { json_array_push(&result, json_make_string_const("refactor")); }
    if (value & LSTALK_CODEACTIONKIND_REFACTOREXTRACT) { json_array_push(&result, json_make_string_const("refactor.extract")); }
    if (value & LSTALK_CODEACTIONKIND_REFACTORINLINE) { json_array_push(&result, json_make_string_const("refactor.inline")); }
    if (value & LSTALK_CODEACTIONKIND_REFACTORREWRITE) { json_array_push(&result, json_make_string_const("refactor.rewrite")); }
    if (value & LSTALK_CODEACTIONKIND_SOURCE) { json_array_push(&result, json_make_string_const("source")); }
    if (value & LSTALK_CODEACTIONKIND_SOURCEORGANIZEIMPORTS) { json_array_push(&result, json_make_string_const("source.organizeImports")); }
    if (value & LSTALK_CODEACTIONKIND_SOURCEFIXALL) { json_array_push(&result, json_make_string_const("source.fixAll")); }

    return result;
}

typedef enum {
    DIAGNOSTICTAG_Unnecessary = 1,
    DIAGNOSTICTAG_Deprecated = 2,
} DiagnosticTag;

static JSONValue diagnostic_tag_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_DIAGNOSTICTAG_UNNECESSARY) { json_array_push(&result, json_make_int(DIAGNOSTICTAG_Unnecessary)); }
    if (value & LSTALK_DIAGNOSTICTAG_DEPRECATED) { json_array_push(&result, json_make_int(DIAGNOSTICTAG_Deprecated)); }

    return result;
}

static JSONValue folding_range_kind_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_FOLDINGRANGEKIND_COMMENT) { json_array_push(&result, json_make_string_const("comment")); }
    if (value & LSTALK_FOLDINGRANGEKIND_IMPORTS) { json_array_push(&result, json_make_string_const("imports")); }
    if (value & LSTALK_FOLDINGRANGEKIND_REGION) { json_array_push(&result, json_make_string_const("region")); }

    return result;
}

static JSONValue token_format_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_TOKENFORMAT_RELATIVE) { json_array_push(&result, json_make_string_const("relative")); }

    return result;
}

static JSONValue position_encoding_kind_array(int value) {
    JSONValue result = json_make_array();

    if (value & LSTALK_POSITIONENCODINGKIND_UTF8) { json_array_push(&result, json_make_string_const("utf-8")); }
    if (value & LSTALK_POSITIONENCODINGKIND_UTF16) { json_array_push(&result, json_make_string_const("utf-16")); }
    if (value & LSTALK_POSITIONENCODINGKIND_UTF32) { json_array_push(&result, json_make_string_const("utf-32")); }

    return result;
}

static LSTalk_PositionEncodingKind parse_position_encoding_kind(char* value) {
    if (value == NULL) {
        return LSTALK_POSITIONENCODINGKIND_UTF16;
    }

    if (strcmp(value, "utf-8") == 0) { return LSTALK_POSITIONENCODINGKIND_UTF8; }
    if (strcmp(value, "utf-16") == 0) { return LSTALK_POSITIONENCODINGKIND_UTF16; }
    if (strcmp(value, "utf-32") == 0) { return LSTALK_POSITIONENCODINGKIND_UTF32; }

    return LSTALK_POSITIONENCODINGKIND_UTF16;
}

static void dynamic_registration(JSONValue* root, int value) {
    if (root == NULL || root->type != JSON_VALUE_OBJECT) {
        return;
    }

    json_object_const_key_set(root, "dynamicRegistration", json_make_boolean(value));
}

static void link_support(JSONValue* root, int value) {
    if (root == NULL || root->type != JSON_VALUE_OBJECT) {
        return;
    }

    json_object_const_key_set(root, "linkSupport", json_make_boolean(value));
}

static JSONValue string_array(char** array, int count) {
    JSONValue result = json_make_array();
    for (int i = 0; i < count; i++) {
        json_array_push(&result, json_make_string(array[i]));
    }
    return result;
}

//
// Begin Workspace Objects
//

static JSONValue make_workspace_edit_object(LSTalk_WorkspaceEditClientCapabilities* workspace_edit) {
    JSONValue result = json_make_object();

    json_object_const_key_set(&result, "documentChanges", json_make_boolean(workspace_edit->document_changes));
    json_object_const_key_set(&result, "resourceOperations", resource_operation_kind_array(workspace_edit->resource_operations));
    json_object_const_key_set(&result, "failureHandling", failure_handling_array(workspace_edit->failure_handling));
    json_object_const_key_set(&result, "normalizesLineEndings", json_make_boolean(workspace_edit->normalizes_line_endings));
    JSONValue change_annotation_support = json_make_object();
    json_object_const_key_set(&change_annotation_support, "groupsOnLabel", json_make_boolean(workspace_edit->groups_on_label));
    json_object_const_key_set(&result, "changeAnnotationSupport", change_annotation_support);

    return result;
}

static JSONValue make_workspace_symbol_object(LSTalk_WorkspaceSymbolClientCapabilities* symbol) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, symbol->dynamic_registration);
    JSONValue symbol_kind = json_make_object();
    json_object_const_key_set(&symbol_kind, "valueSet", symbol_kind_array(symbol->symbol_kind_value_set));
    json_object_const_key_set(&result, "symbolKind", symbol_kind);
    JSONValue tag_support = json_make_object();
    json_object_const_key_set(&tag_support, "valueSet", symbol_tag_array(symbol->tag_support_value_set));
    json_object_const_key_set(&result, "tagSupport", tag_support);
    JSONValue resolve_support = json_make_object();
    json_object_const_key_set(&resolve_support, "properties", string_array(symbol->resolve_support_properties, symbol->resolve_support_count));
    json_object_const_key_set(&result, "resolveSupport", resolve_support);

    return result;
}

static JSONValue make_workspace_file_operations_object(LSTalk_FileOperations* file_ops) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, file_ops->dynamic_registration);
    json_object_const_key_set(&result, "didCreate", json_make_boolean(file_ops->did_create));
    json_object_const_key_set(&result, "willCreate", json_make_boolean(file_ops->will_create));
    json_object_const_key_set(&result, "didRename", json_make_boolean(file_ops->did_rename));
    json_object_const_key_set(&result, "willRename", json_make_boolean(file_ops->will_rename));
    json_object_const_key_set(&result, "didDelete", json_make_boolean(file_ops->did_delete));
    json_object_const_key_set(&result, "willDelete", json_make_boolean(file_ops->will_delete));

    return result;
}

static JSONValue make_workspace_object(LSTalk_Workspace* workspace) {
    JSONValue result = json_make_object();

    JSONValue did_change_configuration = json_make_object();
    dynamic_registration(&did_change_configuration, workspace->did_change_configuration.dynamic_registration);

    JSONValue did_change_watched_files = json_make_object();
    dynamic_registration(&did_change_watched_files, workspace->did_change_watched_files.dynamic_registration);
    json_object_const_key_set(&did_change_watched_files, "relativePatternSupport", json_make_boolean(workspace->did_change_watched_files.relative_pattern_support));

    JSONValue execute_command = json_make_object();
    dynamic_registration(&execute_command, workspace->execute_command.dynamic_registration);

    JSONValue semantic_tokens = json_make_object();
    json_object_const_key_set(&semantic_tokens, "refreshSupport", json_make_boolean(workspace->semantic_tokens.refresh_support));

    JSONValue code_lens = json_make_object();
    json_object_const_key_set(&code_lens, "refreshSupport", json_make_boolean(workspace->code_lens.refresh_support));

    JSONValue inline_value = json_make_object();
    json_object_const_key_set(&inline_value, "refreshSupport", json_make_boolean(workspace->inline_value.refresh_support));

    JSONValue inlay_hint = json_make_object();
    json_object_const_key_set(&inlay_hint, "refreshSupport", json_make_boolean(workspace->inlay_hint.refresh_support));

    JSONValue diagnostics = json_make_object();
    json_object_const_key_set(&diagnostics, "refreshSupport", json_make_boolean(workspace->diagnostics.refresh_support));

    json_object_const_key_set(&result, "applyEdit", json_make_boolean(workspace->apply_edit));
    json_object_const_key_set(&result, "workspaceEdit", make_workspace_edit_object(&workspace->workspace_edit));
    json_object_const_key_set(&result, "didChangeConfiguration", did_change_configuration);
    json_object_const_key_set(&result, "didChangeWatchedFiles", did_change_watched_files);
    json_object_const_key_set(&result, "symbol", make_workspace_symbol_object(&workspace->symbol));
    json_object_const_key_set(&result, "executeCommand", execute_command);
    json_object_const_key_set(&result, "workspaceFolders", json_make_boolean(workspace->workspace_folders));
    json_object_const_key_set(&result, "configuration", json_make_boolean(workspace->configuration));
    json_object_const_key_set(&result, "semanticTokens", semantic_tokens);
    json_object_const_key_set(&result, "codeLens", code_lens);
    json_object_const_key_set(&result, "fileOperations", make_workspace_file_operations_object(&workspace->file_operations));
    json_object_const_key_set(&result, "inlineValue", inline_value);
    json_object_const_key_set(&result, "inlayHint", inlay_hint);
    json_object_const_key_set(&result, "diagnostics", diagnostics);

    return result;
}

//
// Begin Text Document Objects
//

static JSONValue make_text_document_synchronization_object(LSTalk_TextDocumentSyncClientCapabilities* sync) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, sync->dynamic_registration);
    json_object_const_key_set(&result, "willSave", json_make_boolean(sync->will_save));
    json_object_const_key_set(&result, "willSaveWaitUntil", json_make_boolean(sync->will_save_wait_until));
    json_object_const_key_set(&result, "didSave", json_make_boolean(sync->did_save));

    return result;
}

static JSONValue make_text_document_completion_item_object(LSTalk_CompletionItem* completion_item) {
    JSONValue result = json_make_object();

    json_object_const_key_set(&result, "snippetSupport", json_make_boolean(completion_item->snippet_support));
    json_object_const_key_set(&result, "commitCharactersSupport", json_make_boolean(completion_item->commit_characters_support));
    json_object_const_key_set(&result, "documentationFormat", markup_kind_array(completion_item->documentation_format));
    json_object_const_key_set(&result, "deprecatedSupport", json_make_boolean(completion_item->deprecated_support));
    json_object_const_key_set(&result, "preselectSupport", json_make_boolean(completion_item->preselect_support));
    JSONValue item_tag_support = json_make_object();
    json_object_const_key_set(&item_tag_support, "valueSet", completion_item_tag_array(completion_item->tag_support_value_set));
    json_object_const_key_set(&result, "tagSupport", item_tag_support);
    json_object_const_key_set(&result, "insertReplaceSupport", json_make_boolean(completion_item->insert_replace_support));
    JSONValue item_resolve_properties = json_make_object();
    json_object_const_key_set(&item_resolve_properties, "properties",
        string_array(completion_item->resolve_support_properties, completion_item->resolve_support_count));
    json_object_const_key_set(&result, "resolveSupport", item_resolve_properties);
    JSONValue insert_text_mode = json_make_object();
    json_object_const_key_set(&insert_text_mode, "valueSet", insert_text_mode_array(completion_item->insert_text_mode_support_value_set));
    json_object_const_key_set(&result, "insertTextModeSupport", insert_text_mode);
    json_object_const_key_set(&result, "labelDetailsSupport", json_make_boolean(completion_item->label_details_support));

    return result;
}

static JSONValue make_text_document_completion_object(LSTalk_CompletionClientCapabilities* completion) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, completion->dynamic_registration);
    json_object_const_key_set(&result, "completionItem", make_text_document_completion_item_object(&completion->completion_item));
    JSONValue item_kind = json_make_object();
    json_object_const_key_set(&item_kind, "valueSet", completion_item_kind_array(completion->completion_item_kind_value_set));
    json_object_const_key_set(&result, "completionItemKind", item_kind);
    json_object_const_key_set(&result, "contextSupport", json_make_boolean(completion->context_support));
    json_object_const_key_set(&result, "insertTextMode", json_make_int(completion->insert_text_mode));
    JSONValue item_defaults = json_make_object();
    json_object_const_key_set(&item_defaults, "itemDefaults",
        string_array(completion->completion_list_item_defaults, completion->completion_list_item_defaults_count));
    json_object_const_key_set(&result, "completionList", item_defaults);

    return result;
}

static JSONValue make_text_document_signature_object(LSTalk_SignatureHelpClientCapabilities* signature_help) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, signature_help->dynamic_registration);
    JSONValue info = json_make_object();
    json_object_const_key_set(&info, "documentationFormat", markup_kind_array(signature_help->signature_information.documentation_format));
    JSONValue parameter_info = json_make_object();
    json_object_const_key_set(&parameter_info, "labelOffsetSupport", json_make_boolean(signature_help->signature_information.label_offset_support));
    json_object_const_key_set(&info, "parameterInformation", parameter_info);
    json_object_const_key_set(&info, "activeParameterSupport", json_make_boolean(signature_help->signature_information.active_parameter_support));
    json_object_const_key_set(&result, "signatureInformation", info);
    json_object_const_key_set(&result, "contextSupport", json_make_boolean(signature_help->context_support));

    return result;
}

static JSONValue make_text_document_symbol_object(LSTalk_DocumentSymbolClientCapabilities* symbol) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, symbol->dynamic_registration);
    JSONValue symbol_kind = json_make_object();
    json_object_const_key_set(&symbol_kind, "valueSet", symbol_kind_array(symbol->symbol_kind_value_set));
    json_object_const_key_set(&result, "symbolKind", symbol_kind);
    json_object_const_key_set(&result, "hierarchicalDocumentSymbolSupport", json_make_boolean(symbol->hierarchical_document_symbol_support));
    JSONValue tag_support = json_make_object();
    json_object_const_key_set(&tag_support, "valueSet", symbol_tag_array(symbol->tag_support_value_set));
    json_object_const_key_set(&result, "tagSupport", tag_support);
    json_object_const_key_set(&result, "labelSupport", json_make_boolean(symbol->label_support));

    return result;
}

static JSONValue make_text_document_code_action_object(LSTalk_CodeActionClientCapabilities* code_action) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, code_action->dynamic_registration);
    JSONValue kind = json_make_object();
    json_object_const_key_set(&kind, "valueSet", code_action_kind_array(code_action->code_action_value_set));
    JSONValue literal_support = json_make_object();
    json_object_const_key_set(&literal_support, "codeActionKind", kind);
    json_object_const_key_set(&result, "codeActionLiteralSupport", literal_support);
    json_object_const_key_set(&result, "isPreferredSupport", json_make_boolean(code_action->is_preferred_support));
    json_object_const_key_set(&result, "disabledSupport", json_make_boolean(code_action->disabled_support));
    json_object_const_key_set(&result, "dataSupport", json_make_boolean(code_action->data_support));
    JSONValue resolve_support = json_make_object();
    json_object_const_key_set(&resolve_support, "properties",
        string_array(code_action->resolve_support_properties, code_action->resolve_support_count));
    json_object_const_key_set(&result, "resolveSupport", resolve_support);
    json_object_const_key_set(&result, "honorsChangeAnnotations", json_make_boolean(code_action->honors_change_annotations));

    return result;
}

static JSONValue make_text_document_rename_object(LSTalk_RenameClientCapabilities* rename) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, rename->dynamic_registration);
    json_object_const_key_set(&result, "prepareSupport", json_make_boolean(rename->prepare_support));
    json_object_const_key_set(&result, "prepareSupportDefaultBehavior", json_make_int(rename->prepare_support_default_behavior));
    json_object_const_key_set(&result, "honorsChangeAnnotations", json_make_boolean(rename->honors_change_annotations));

    return result;
}

static JSONValue make_text_document_publish_diagnostics_object(LSTalk_PublishDiagnosticsClientCapabilities* publish) {
    JSONValue result = json_make_object();

    json_object_const_key_set(&result, "relatedInformation", json_make_boolean(publish->related_information));
    JSONValue tag_support = json_make_object();
    json_object_const_key_set(&tag_support, "valueSet", diagnostic_tag_array(publish->value_set));
    json_object_const_key_set(&result, "tagSupport", tag_support);
    json_object_const_key_set(&result, "versionSupport", json_make_boolean(publish->version_support));
    json_object_const_key_set(&result, "codeDescriptionSupport", json_make_boolean(publish->code_description_support));
    json_object_const_key_set(&result, "dataSupport", json_make_boolean(publish->data_support));

    return result;
}

static JSONValue make_text_document_folding_range_object(LSTalk_FoldingRangeClientCapabilities* folding_range) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, folding_range->dynamic_registration);
    json_object_const_key_set(&result, "rangeLimit", json_make_int(folding_range->range_limit));
    json_object_const_key_set(&result, "lineFoldingOnly", json_make_boolean(folding_range->line_folding_only));
    JSONValue kind = json_make_object();
    json_object_const_key_set(&kind, "valueSet", folding_range_kind_array(folding_range->value_set));
    json_object_const_key_set(&result, "foldingRangeKind", kind);
    JSONValue range = json_make_object();
    json_object_const_key_set(&range, "collapsedText", json_make_boolean(folding_range->collapsed_text));
    json_object_const_key_set(&result, "foldingRange", range);

    return result;
}

static JSONValue make_text_document_semantic_tokens_object(LSTalk_SemanticTokensClientCapabilities* semantic_tokens) {
    JSONValue result = json_make_object();

    dynamic_registration(&result, semantic_tokens->dynamic_registration);
    JSONValue requests_full = json_make_object();
    json_object_const_key_set(&requests_full, "delta", json_make_boolean(semantic_tokens->delta));
    JSONValue requests = json_make_object();
    json_object_const_key_set(&requests, "range", json_make_boolean(semantic_tokens->range));
    json_object_const_key_set(&requests, "full", requests_full);
    json_object_const_key_set(&result, "requests", requests);
    json_object_const_key_set(&result, "tokenTypes",
        string_array(semantic_tokens->token_types, semantic_tokens->token_types_count));
    json_object_const_key_set(&result, "tokenModifiers",
        string_array(semantic_tokens->token_modifiers, semantic_tokens->token_modifiers_count));
    json_object_const_key_set(&result, "formats", token_format_array(semantic_tokens->formats));
    json_object_const_key_set(&result, "overlappingTokenSupport", json_make_boolean(semantic_tokens->overlapping_token_support));
    json_object_const_key_set(&result, "multilineTokenSupport", json_make_boolean(semantic_tokens->multiline_token_support));
    json_object_const_key_set(&result, "serverCancelSupport", json_make_boolean(semantic_tokens->server_cancel_support));
    json_object_const_key_set(&result, "augmentsSyntaxTokens", json_make_boolean(semantic_tokens->augments_syntax_tokens));

    return result;
}

static JSONValue make_text_document_object(LSTalk_TextDocumentClientCapabilities* text_document) {
    JSONValue result = json_make_object();

    JSONValue hover = json_make_object();
    dynamic_registration(&hover, text_document->hover.dynamic_registration);
    json_object_const_key_set(&hover, "contentFormat", markup_kind_array(text_document->hover.content_format));

    JSONValue declaration = json_make_object();
    dynamic_registration(&declaration, text_document->declaration.dynamic_registration);
    link_support(&declaration, text_document->declaration.link_support);

    JSONValue definition = json_make_object();
    dynamic_registration(&definition, text_document->definition.dynamic_registration);
    link_support(&definition, text_document->definition.link_support);

    JSONValue type_definition = json_make_object();
    dynamic_registration(&type_definition, text_document->type_definition.dynamic_registration);
    link_support(&type_definition, text_document->type_definition.link_support);

    JSONValue implementation = json_make_object();
    dynamic_registration(&implementation, text_document->implementation.dynamic_registration);
    link_support(&implementation, text_document->implementation.link_support);

    JSONValue references = json_make_object();
    dynamic_registration(&references, text_document->references.dynamic_registration);

    JSONValue document_highlight = json_make_object();
    dynamic_registration(&document_highlight, text_document->document_highlight.dynamic_registration);

    JSONValue code_lens = json_make_object();
    dynamic_registration(&code_lens, text_document->code_lens.dynamic_registration);

    JSONValue document_link = json_make_object();
    dynamic_registration(&document_link, text_document->document_link.dynamic_registration);
    json_object_const_key_set(&document_link, "tooltipSupport", json_make_boolean(text_document->document_link.tooltip_support));

    JSONValue color_provider = json_make_object();
    dynamic_registration(&color_provider, text_document->color_provider.dynamic_registration);

    JSONValue formatting = json_make_object();
    dynamic_registration(&formatting, text_document->formatting.dynamic_registration);

    JSONValue range_formatting = json_make_object();
    dynamic_registration(&range_formatting, text_document->range_formatting.dynamic_registration);

    JSONValue on_type_formatting = json_make_object();
    dynamic_registration(&on_type_formatting, text_document->on_type_formatting.dynamic_registration);

    JSONValue selection_range = json_make_object();
    dynamic_registration(&selection_range, text_document->selection_range.dynamic_registration);

    JSONValue linked_editing_range = json_make_object();
    dynamic_registration(&selection_range, text_document->linked_editing_range.dynamic_registration);

    JSONValue call_hierarchy = json_make_object();
    dynamic_registration(&call_hierarchy, text_document->call_hierarchy.dynamic_registration);

    JSONValue moniker = json_make_object();
    dynamic_registration(&moniker, text_document->moniker.dynamic_registration);

    JSONValue type_hierarchy = json_make_object();
    dynamic_registration(&type_hierarchy, text_document->type_hierarchy.dynamic_registration);

    JSONValue inline_value = json_make_object();
    dynamic_registration(&inline_value, text_document->inline_value.dynamic_registration);

    JSONValue inlay_hint = json_make_object();
    dynamic_registration(&inlay_hint, text_document->inlay_hint.dynamic_registration);
    JSONValue inlay_hint_resolve_support = json_make_object();
    json_object_const_key_set(&inlay_hint_resolve_support, "properties",
        string_array(text_document->inlay_hint.properties, text_document->inlay_hint.properties_count));
    json_object_const_key_set(&inlay_hint, "resolveSupport", inlay_hint_resolve_support);

    JSONValue diagnostic = json_make_object();
    dynamic_registration(&diagnostic, text_document->diagnostic.dynamic_registration);
    json_object_const_key_set(&diagnostic, "relatedDocumentSupport", json_make_boolean(text_document->diagnostic.related_document_support));

    json_object_const_key_set(&result, "synchronization", make_text_document_synchronization_object(&text_document->synchronization));
    json_object_const_key_set(&result, "completion", make_text_document_completion_object(&text_document->completion));
    json_object_const_key_set(&result, "hover", hover);
    json_object_const_key_set(&result, "signatureHelp", make_text_document_signature_object(&text_document->signature_help));
    json_object_const_key_set(&result, "declaration", declaration);
    json_object_const_key_set(&result, "definition", definition);
    json_object_const_key_set(&result, "typeDefinition", type_definition);
    json_object_const_key_set(&result, "implementation", implementation);
    json_object_const_key_set(&result, "references", references);
    json_object_const_key_set(&result, "documentHighlight", document_highlight);
    json_object_const_key_set(&result, "documentSymbol", make_text_document_symbol_object(&text_document->document_symbol));
    json_object_const_key_set(&result, "codeAction", make_text_document_code_action_object(&text_document->code_action));
    json_object_const_key_set(&result, "codeLens", code_lens);
    json_object_const_key_set(&result, "documentLink", document_link);
    json_object_const_key_set(&result, "colorProvider", color_provider);
    json_object_const_key_set(&result, "formatting", formatting);
    json_object_const_key_set(&result, "rangeFormatting", range_formatting);
    json_object_const_key_set(&result, "onTypeFormatting", on_type_formatting);
    json_object_const_key_set(&result, "rename", make_text_document_rename_object(&text_document->rename));
    json_object_const_key_set(&result, "publishDiagnostics", make_text_document_publish_diagnostics_object(&text_document->publish_diagnostics));
    json_object_const_key_set(&result, "foldingRange", make_text_document_folding_range_object(&text_document->folding_range));
    json_object_const_key_set(&result, "selectionRange", selection_range);
    json_object_const_key_set(&result, "linkedEditingRange", linked_editing_range);
    json_object_const_key_set(&result, "callHierarchy", call_hierarchy);
    json_object_const_key_set(&result, "semanticTokens", make_text_document_semantic_tokens_object(&text_document->semantic_tokens));
    json_object_const_key_set(&result, "moniker", moniker);
    json_object_const_key_set(&result, "typeHierarchy", type_hierarchy);
    json_object_const_key_set(&result, "inlineValue", inline_value);
    json_object_const_key_set(&result, "inlayHint", inlay_hint);
    json_object_const_key_set(&result, "diagnostic", diagnostic);

    return result;
}

//
// Begin Client Capabilities Objects
//

static JSONValue make_window_object(LSTalk_Window* window) {
    JSONValue result = json_make_object();

    JSONValue show_message_message_action_item = json_make_object();
    json_object_const_key_set(&show_message_message_action_item, "additionalPropertiesSupport", json_make_boolean(window->show_message.message_action_item_additional_properties_support));
    JSONValue show_message = json_make_object();
    json_object_const_key_set(&show_message, "messageActionItem", show_message_message_action_item);

    JSONValue show_document = json_make_object();
    json_object_const_key_set(&show_document, "support", json_make_boolean(window->show_document.support));

    json_object_const_key_set(&result, "workDoneProgress", json_make_boolean(window->work_done_progress));
    json_object_const_key_set(&result, "showMessage", show_message);
    json_object_const_key_set(&result, "showDocument", show_document);

    return result;
}

static JSONValue make_general_object(LSTalk_General* general) {
    JSONValue result = json_make_object();

    JSONValue stale_request_support = json_make_object();
    json_object_const_key_set(&stale_request_support, "cancel", json_make_boolean(general->cancel));
    json_object_const_key_set(&stale_request_support, "retryOnContentModified",
        string_array(general->retry_on_content_modified, general->retry_on_content_modified_count));
    
    JSONValue regular_expressions = json_make_object();
    json_object_const_key_set(&regular_expressions, "engine", json_make_string(general->regular_expressions.engine));
    json_object_const_key_set(&regular_expressions, "version", json_make_string(general->regular_expressions.version));

    JSONValue markdown = json_make_object();
    json_object_const_key_set(&markdown, "parser", json_make_string(general->markdown.parser));
    json_object_const_key_set(&markdown, "version", json_make_string(general->markdown.version));
    json_object_const_key_set(&markdown, "allowedTags",
        string_array(general->markdown.allowed_tags, general->markdown.allowed_tags_count));

    json_object_const_key_set(&result, "staleRequestSupport", stale_request_support);
    json_object_const_key_set(&result, "regularExpressions", regular_expressions);
    json_object_const_key_set(&result, "markdown", markdown);
    json_object_const_key_set(&result, "positionEncodings", position_encoding_kind_array(general->position_encodings));

    return result;
}

static JSONValue make_client_capabilities_object(LSTalk_ClientCapabilities* capabilities) {
    JSONValue result = json_make_object();

    JSONValue notebook_sync = json_make_object();
    dynamic_registration(&notebook_sync, capabilities->notebook_document.synchronization.dynamic_registration);
    json_object_const_key_set(&notebook_sync, "executionSummarySupport", json_make_boolean(capabilities->notebook_document.synchronization.execution_summary_support));

    JSONValue notebook_document = json_make_object();
    json_object_const_key_set(&notebook_document, "synchronization", notebook_sync);

    json_object_const_key_set(&result, "workspace", make_workspace_object(&capabilities->workspace));
    json_object_const_key_set(&result, "textDocument", make_text_document_object(&capabilities->text_document));
    json_object_const_key_set(&result, "notebookDocument", notebook_document);
    json_object_const_key_set(&result, "window", make_window_object(&capabilities->window));
    json_object_const_key_set(&result, "general", make_general_object(&capabilities->general));

    return result;
}

LSTalk_Context* lstalk_init() {
    LSTalk_Context* result = (LSTalk_Context*)malloc(sizeof(LSTalk_Context));
    result->servers = vector_create(sizeof(Server));
    result->server_id = 1;
    char buffer[40];
    sprintf(buffer, "%d.%d.%d", LSTALK_MAJOR, LSTALK_MINOR, LSTALK_REVISION);
    result->client_info.name = string_alloc_copy("lstalk");
    result->client_info.version = string_alloc_copy(buffer);
    result->locale = string_alloc_copy("en");
    memset(&result->client_capabilities, 0, sizeof(result->client_capabilities));
    result->debug_flags = LSTALK_DEBUGFLAGS_NONE;
    return result;
}

void lstalk_shutdown(LSTalk_Context* context) {
    if (context == NULL) {
        return;
    }

    // Close all connected servers.
    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);
        server_close(server);
    }
    vector_destroy(&context->servers);

    client_info_clear(&context->client_info);
    if (context->locale != NULL) {
        free(context->locale);
    }
    free(context);
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

void lstalk_set_client_info(LSTalk_Context* context, char* name, char* version) {
    if (context == NULL) {
        return;
    }

    client_info_clear(&context->client_info);

    if (name != NULL) {
        context->client_info.name = string_alloc_copy(name);
    }

    if (version != NULL) {
        context->client_info.version = string_alloc_copy(version);
    }
}

void lstalk_set_locale(LSTalk_Context* context, char* locale) {
    if (context == NULL) {
        return;
    }

    if (context->locale != NULL) {
        free(context->locale);
    }

    context->locale = string_alloc_copy(locale);
}

LSTalk_ClientCapabilities* lstalk_get_client_capabilities(LSTalk_Context* context) {
    if (context == NULL) {
        return NULL;
    }

    return &context->client_capabilities;
}

void lstalk_set_debug_flags(LSTalk_Context* context, int flags) {
    if (context == NULL) {
        return;
    }

    context->debug_flags = flags;
}

LSTalk_ServerID lstalk_connect(LSTalk_Context* context, const char* uri, LSTalk_ConnectParams connect_params) {
    if (context == NULL || uri == NULL) {
        return LSTALK_INVALID_SERVER_ID;
    }

    Server server;
    server.process = process_create(uri);
    if (server.process == NULL) {
        return LSTALK_INVALID_SERVER_ID;
    }

    server.id = context->server_id++;
    server.request_id = 1;
    server.requests = vector_create(sizeof(Request));
    memset(&server.info, 0, sizeof(server.info));

    JSONValue params = json_make_object();
    json_object_const_key_set(&params, "processId", json_make_int(process_get_current_id()));
    json_object_const_key_set(&params, "clientInfo", client_info(&context->client_info));
    json_object_const_key_set(&params, "locale", json_make_string_const(context->locale));
    json_object_const_key_set(&params, "rootUri", json_make_string(connect_params.root_uri));
    json_object_const_key_set(&params, "clientCapabilities", make_client_capabilities_object(&context->client_capabilities));
    json_object_const_key_set(&params, "trace", json_make_string_const(trace_to_string(connect_params.trace)));

    Request request = rpc_make_request(&server.request_id, "initialize", params);
    server_send_request(context, server.process, &request);
    server.connection_status = LSTALK_CONNECTION_STATUS_CONNECTING;
    vector_push(&server.requests, &request);
    vector_push(&context->servers, &server);
    return server.id;
}

LSTalk_ConnectionStatus lstalk_get_connection_status(LSTalk_Context* context, LSTalk_ServerID id) {
    LSTalk_ConnectionStatus result = LSTALK_CONNECTION_STATUS_NOT_CONNECTED;

    if (context == NULL) {
        return result;
    }

    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);

        if (server->id == id) {
            result = server->connection_status;
            break;
        }
    }

    return result;
}

LSTalk_ServerInfo* lstalk_get_server_info(LSTalk_Context* context, LSTalk_ServerID id) {
    if (context == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);
        if (server->id == id) {
            return &server->info;
        }
    }

    return NULL;
}

int lstalk_close(LSTalk_Context* context, LSTalk_ServerID id) {
    if (context == NULL || id == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);

        if (server->id == id) {
            Request request = rpc_make_request(&server->request_id, "shutdown", json_make_null());
            server_send_request(context, server->process, &request);
            vector_push(&server->requests, &request);
            return 1;
        }
    }

    return 0;
}

int lstalk_process_responses(LSTalk_Context* context) {
    if (context == NULL) {
        return 0;
    }

    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);
        char* response = process_read(server->process);

        if (response != NULL) {
            if (context->debug_flags & LSTALK_DEBUGFLAGS_PRINT_RESPONSES) {
                printf("%s\n", response);
            }
            char* content_length = strstr(response, "Content-Length");
            if (content_length != NULL) {
                int length = 0;
                sscanf(content_length, "Content-Length: %d", &length);

                char* content_start = strstr(content_length, "{");
                if (content_start != NULL) {
                    JSONValue value = json_decode(content_start);
                    JSONValue id = json_object_get(&value, "id");

                    // Find the associated request for this response.
                    for (size_t request_index = 0; request_index < server->requests.length; request_index++) {
                        Request* request = (Request*)vector_get(&server->requests, request_index);
                        if (request->id == id.value.int_value) {
                            int remove_request = 1;
                            char* method = rpc_get_method(request);
                            if (strcmp(method, "initialize") == 0) {
                                server->connection_status = LSTALK_CONNECTION_STATUS_CONNECTED;
                                server->info = server_parse_initialized(&value);
                                Request initalized_request = rpc_make_notification("initialized", json_make_null());
                                server_send_request(context, server->process, &initalized_request);
                                rpc_close_request(&initalized_request);
                            } else if (strcmp(method, "shutdown") == 0) {
                                Request exit_notification = rpc_make_notification("exit", json_make_null());
                                server_send_request(context, server->process, &exit_notification);
                                rpc_close_request(&exit_notification);
                                server_close(server);
                                vector_remove(&context->servers, i);
                                i--;
                                remove_request = 0;
                            }
                            if (remove_request) {
                                rpc_close_request(request);
                                vector_remove(&server->requests, request_index);
                                request_index--;
                            }
                            break;
                        }
                    }

                    json_destroy_value(&value);
                }
            }
            free(response);
        }
    }

    return 1;
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

static void add_test(Vector* tests, TestCaseFn fn, char* name) {
    if (tests == NULL) {
        return;
    }

    TestCase test_case;
    test_case.fn = fn;
    test_case.name = name;
    vector_push(tests, &test_case);
}

#define REGISTER_TEST(tests, fn) add_test(tests, fn, #fn)
#define RED_TEXT(text) printf("\033[0;31m"); printf("%s", text); printf("\033[0m");
#define GREEN_TEXT(text) printf("\033[0;32m"); printf("%s", text); printf("\033[0m");

static int tests_run(Vector* tests) {
    if (tests == NULL || tests->element_size != sizeof(TestCase)) {
        return 0;
    }

    printf("Running %zu tests...\n", tests->length);

    int failed = 0;
    for (size_t i = 0; i < tests->length; i++) {
        TestCase* test_case = (TestCase*)vector_get(tests, i);

        int success = test_case->fn();
        if (success) {
            GREEN_TEXT("PASS");
        } else {
            RED_TEXT("FAIL");
            failed++;
        }

        printf(" ... %s\n", test_case->name);
    }

    return failed;
}

// Vector Tests

static int test_vector_create() {
    Vector vector = vector_create(sizeof(int));
    const size_t capacity = vector.capacity;
    const size_t element_size = vector.element_size;
    vector_destroy(&vector);
    return capacity == 1 && element_size == sizeof(int);
}

static int test_vector_destroy() {
    Vector vector = vector_create(sizeof(int));
    vector_destroy(&vector);
    return vector.length == 0 && vector.data == NULL;
}

static int test_vector_resize() {
    Vector vector = vector_create(sizeof(int));
    int result = vector.length == 0 && vector.capacity == 1;
    vector_resize(&vector, 5);
    result &= vector.length == 0 && vector.capacity == 5;
    vector_destroy(&vector);
    return result;
}

static int test_vector_push() {
    Vector vector = vector_create(sizeof(int));
    int i = 5;
    vector_push(&vector, &i);
    i = 10;
    vector_push(&vector, &i);
    int result = vector.length == 2;
    vector_destroy(&vector);
    return result;
}

static int test_vector_append() {
    Vector vector = vector_create(sizeof(char));
    vector_append(&vector, (void*)"Hello", 5);
    int result = strncmp(vector.data, "Hello", 5) == 0;
    vector_append(&vector, (void*)" World", 6);
    result &= strncmp(vector.data, "Hello World", 11) == 0;
    vector_destroy(&vector);
    return result;
}

static int test_vector_remove() {
    Vector vector = vector_create(sizeof(int));
    for (int i = 0; i < 5; i++) {
        vector_push(&vector, &i);
    }
    int result = vector.length == 5;
    result &= *(int*)vector_get(&vector, 2) == 2;
    vector_remove(&vector, 2);
    result &= *(int*)vector_get(&vector, 2) == 3;
    result &= vector.length == 4;
    vector_destroy(&vector);
    return result;
}

static int test_vector_get() {
    Vector vector = vector_create(sizeof(int));
    int i = 5;
    vector_push(&vector, &i);
    i = 10;
    vector_push(&vector, &i);
    int result = *(int*)vector_get(&vector, 0) == 5 && *(int*)vector_get(&vector, 1) == 10;
    vector_destroy(&vector);
    return result;
}

static TestResults tests_vector() {
    TestResults result;
    Vector tests = vector_create(sizeof(TestCase));

    REGISTER_TEST(&tests, test_vector_create);
    REGISTER_TEST(&tests, test_vector_destroy);
    REGISTER_TEST(&tests, test_vector_resize);
    REGISTER_TEST(&tests, test_vector_push);
    REGISTER_TEST(&tests, test_vector_append);
    REGISTER_TEST(&tests, test_vector_remove);
    REGISTER_TEST(&tests, test_vector_get);

    result.fail = tests_run(&tests);
    result.pass = tests.length - result.fail;
    vector_destroy(&tests);

    return result;
}

// JSON Tests

static int test_json_decode_boolean_false() {
    JSONValue value = json_decode("false");
    return value.type == JSON_VALUE_BOOLEAN && value.value.bool_value == 0;
}

static int test_json_decode_boolean_true() {
    JSONValue value = json_decode("true");
    return value.type == JSON_VALUE_BOOLEAN && value.value.bool_value == 1;
}

static int test_json_decode_int() {
    JSONValue value = json_decode("42");
    return value.type == JSON_VALUE_INT && value.value.int_value == 42;
}

static int test_json_decode_float() {
    JSONValue value = json_decode("3.14");
    return value.type == JSON_VALUE_FLOAT && value.value.float_value == 3.14f;
}

static int test_json_decode_string() {
    JSONValue value = json_decode("\"Hello World\"");
    int result = value.type == JSON_VALUE_STRING && strcmp(value.value.string_value, "Hello World") == 0;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_escaped_string() {
    JSONValue value = json_decode("\"Hello \\\"World\\\"");
    int result = value.type == JSON_VALUE_STRING && strcmp(value.value.string_value, "Hello \"World\"") == 0;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_single_escaped_string() {
    JSONValue value = json_decode("[\"'\", \"\\\\\"\", \":\"]");
    int result = strcmp(json_array_get(&value, 0).value.string_value, "'") == 0;
    result &= strcmp(json_array_get(&value, 1).value.string_value, "\\\"") == 0;
    result &= strcmp(json_array_get(&value, 2).value.string_value, ":") == 0;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_object() {
    JSONValue value = json_decode("{\"Int\": 42, \"Float\": 3.14}");
    int result = json_object_get(&value, "Int").value.int_value == 42 && json_object_get(&value, "Float").value.float_value == 3.14f;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_sub_object() {
    JSONValue value = json_decode("{\"object\": {\"Int\": 42, \"Float\": 3.14}}");
    JSONValue object = json_object_get(&value, "object");
    int result = json_object_get(&object, "Int").value.int_value == 42 && json_object_get(&object, "Float").value.float_value == 3.14f;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_empty_object() {
    JSONValue value = json_decode("{}");
    int result = value.type == JSON_VALUE_OBJECT && value.value.object_value->pairs.length == 0;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_empty_sub_object() {
    JSONValue value = json_decode("{\"Int\": 42, \"object\": {}}");
    int result = json_object_get(&value, "Int").value.int_value == 42;
    result &= json_object_get(&value, "object").value.object_value->pairs.length == 0;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_array() {
    JSONValue value = json_decode("[42, 3.14, \"Hello World\"]");
    int result = json_array_get(&value, 0).value.int_value == 42;
    result &= json_array_get(&value, 1).value.float_value == 3.14f;
    result &= strcmp(json_array_get(&value, 2).value.string_value, "Hello World") == 0;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_array_of_objects() {
    JSONValue value = json_decode("[{\"Int\": 42}, {\"Float\": 3.14}]");
    JSONValue object = json_array_get(&value, 0);
    int result = json_object_get(&object, "Int").value.int_value == 42;
    object = json_array_get(&value, 1);
    result &= json_object_get(&object, "Float").value.float_value == 3.14f;
    json_destroy_value(&value);
    return result;
}

static int test_json_decode_empty_array() {
    JSONValue value = json_decode("[]");
    int result = value.type == JSON_VALUE_ARRAY && value.value.array_value->values.length == 0;
    json_destroy_value(&value);
    return result;
}

static int test_json_encode_boolean_false() {
    JSONValue value = json_make_boolean(0);
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "false") == 0;
    json_destroy_encoder(&encoder);
    return result;
}

static int test_json_encode_boolean_true() {
    JSONValue value = json_make_boolean(1);
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "true") == 0;
    json_destroy_encoder(&encoder);
    return result;
}

static int test_json_encode_int() {
    JSONValue value = json_make_int(42);
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "42") == 0;
    json_destroy_encoder(&encoder);
    return result;
}

static int test_json_encode_float() {
    JSONValue value = json_make_float(3.14f);
    JSONEncoder encoder = json_encode(&value);
    char buffer[40];
    sprintf(buffer, "%f", 3.14f);
    int result = strcmp(encoder.string.data, buffer) == 0;
    json_destroy_encoder(&encoder);
    return result;
}

static int test_json_encode_string() {
    JSONValue value = json_make_string("Hello World");
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "\"Hello World\"") == 0;
    json_destroy_encoder(&encoder);
    json_destroy_value(&value);
    return result;
}

static int test_json_encode_object() {
    JSONValue value = json_make_object();
    json_object_const_key_set(&value, "Int", json_make_int(42));
    json_object_const_key_set(&value, "String", json_make_string_const("Hello World"));
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "{\"Int\": 42, \"String\": \"Hello World\"}") == 0;
    json_destroy_encoder(&encoder);
    json_destroy_value(&value);
    return result;
}

static int test_json_encode_sub_object() {
    JSONValue object = json_make_object();
    json_object_const_key_set(&object, "Int", json_make_int(42));
    json_object_const_key_set(&object, "String", json_make_string_const("Hello World"));
    JSONValue value = json_make_object();
    json_object_const_key_set(&value, "object", object);
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "{\"object\": {\"Int\": 42, \"String\": \"Hello World\"}}") == 0;
    json_destroy_encoder(&encoder);
    json_destroy_value(&value);
    return result;
}

static int test_json_encode_array() {
    JSONValue value = json_make_array();
    json_array_push(&value, json_make_int(42));
    json_array_push(&value, json_make_string("Hello World"));
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "[42, \"Hello World\"]") == 0;
    json_destroy_encoder(&encoder);
    json_destroy_value(&value);
    return result;
}

static int test_json_encode_array_of_objects() {
    JSONValue value = json_make_array();
    JSONValue object = json_make_object();
    json_object_const_key_set(&object, "Int", json_make_int(42));
    json_array_push(&value, object);
    object = json_make_object();
    json_object_const_key_set(&object, "String", json_make_string_const("Hello World"));
    json_array_push(&value, object);
    JSONEncoder encoder = json_encode(&value);
    int result = strcmp(encoder.string.data, "[{\"Int\": 42}, {\"String\": \"Hello World\"}]") == 0;
    json_destroy_encoder(&encoder);
    json_destroy_value(&value);
    return result;
}

static TestResults tests_json() {
    TestResults result;
    Vector tests = vector_create(sizeof(TestCase));

    REGISTER_TEST(&tests, test_json_decode_boolean_false);
    REGISTER_TEST(&tests, test_json_decode_boolean_true);
    REGISTER_TEST(&tests, test_json_decode_int);
    REGISTER_TEST(&tests, test_json_decode_float);
    REGISTER_TEST(&tests, test_json_decode_string);
    REGISTER_TEST(&tests, test_json_decode_escaped_string);
    REGISTER_TEST(&tests, test_json_decode_single_escaped_string);
    REGISTER_TEST(&tests, test_json_decode_object);
    REGISTER_TEST(&tests, test_json_decode_sub_object);
    REGISTER_TEST(&tests, test_json_decode_empty_object);
    REGISTER_TEST(&tests, test_json_decode_empty_sub_object);
    REGISTER_TEST(&tests, test_json_decode_array);
    REGISTER_TEST(&tests, test_json_decode_array_of_objects);
    REGISTER_TEST(&tests, test_json_decode_empty_array);
    REGISTER_TEST(&tests, test_json_encode_boolean_false);
    REGISTER_TEST(&tests, test_json_encode_boolean_true);
    REGISTER_TEST(&tests, test_json_encode_int);
    REGISTER_TEST(&tests, test_json_encode_float);
    REGISTER_TEST(&tests, test_json_encode_string);
    REGISTER_TEST(&tests, test_json_encode_object);
    REGISTER_TEST(&tests, test_json_encode_array);
    REGISTER_TEST(&tests, test_json_encode_sub_object);
    REGISTER_TEST(&tests, test_json_encode_array_of_objects);

    result.fail = tests_run(&tests);
    result.pass = tests.length - result.fail;

    vector_destroy(&tests);
    return result;
}

typedef struct TestSuite {
    TestResults (*fn)();
    char* name;
} TestSuite;

void add_test_suite(Vector* suites, TestResults (*fn)(), char* name) {
    if (suites == NULL) {
        return;
    }

    TestSuite suite;
    suite.fn = fn;
    suite.name = name;
    vector_push(suites, &suite);
}

#define ADD_TEST_SUITE(suites, fn) add_test_suite(suites, fn, #fn)

void lstalk_tests() {
    printf("Running tests for lstalk...\n\n");

    Vector suites = vector_create(sizeof(TestSuite));
    ADD_TEST_SUITE(&suites, tests_vector);
    ADD_TEST_SUITE(&suites, tests_json);

    TestResults results;
    results.pass = 0;
    results.fail = 0;

    for (size_t i = 0; i < suites.length; i++) {
        TestSuite* suite = (TestSuite*)vector_get(&suites, i);

        printf("Test suite %s\n", suite->name);
        TestResults suite_results = suite->fn();
        results.fail += suite_results.fail;
        results.pass += suite_results.pass;
        printf("\n");
    }

    printf("TESTS PASSED: %d\n", results.pass);
    printf("TESTS FAILED: %d\n", results.fail);

    vector_destroy(&suites);
}

#endif
