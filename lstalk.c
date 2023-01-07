#include "lstalk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Version information
//

#define MAJOR 0
#define MINOR 0
#define REVISION 1

//
// Platform definitions
//

#if _WIN32 || _WIN64
    #define WINDOWS 1
#elif __APPLE__
    #define APPLE 1
#elif __linux__
    #define LINUX 1
#else
    #error "Current platform is not supported."
#endif

#if APPLE || LINUX
    #define POSIX 1
#endif

//
// Platform includes
//

#if WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#elif POSIX
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

Vector vector_create(size_t element_size) {
    Vector result;
    result.element_size = element_size;
    result.length = 0;
    result.capacity = 1;
    result.data = malloc(element_size * result.capacity);
    return result;
}

void vector_destroy(Vector* vector) {
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

void vector_resize(Vector* vector, size_t capacity) {
    if (vector == NULL || vector->element_size == 0) {
        return;
    }

    vector->capacity = capacity;
    vector->data = realloc(vector->data, vector->element_size * vector->capacity);
}

void vector_push(Vector* vector, void* element) {
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

void vector_append(Vector* vector, void* elements, size_t count) {
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

char* vector_get(Vector* vector, size_t index) {
    if (vector == NULL || vector->element_size == 0 || index >= vector->length) {
        return NULL;
    }

    return &vector->data[vector->element_size * index];
}

//
// String Functions
//
// Section that contains functions to help manage strings.

char* string_alloc_copy(const char* source) {
    size_t length = strlen(source);
    char* result = (char*)malloc(length + 1);
    strcpy(result, source);
    return result;
}

//
// Process Management
//
// This section will manage the creation/destruction of a process. All platform implementations should be
// provided here.

struct Process;

#if WINDOWS

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

void close_handles(StdHandles* handles) {
    CloseHandle(handles->child_stdin_read);
    CloseHandle(handles->child_stdin_write);
    CloseHandle(handles->child_stdout_read);
    CloseHandle(handles->child_stdout_write);
}

typedef struct Process {
    StdHandles std_handles;
    PROCESS_INFORMATION info;
} Process;

Process* create_process_windows(const char* path) {
    StdHandles handles;
    handles.child_stdin_read = NULL;
    handles.child_stdin_write = NULL;
    handles.child_stdout_read = NULL;
    handles.child_stdout_write = NULL;

    SECURITY_ATTRIBUTES security_attr;
    ZeroMemory(&security_attr, sizeof(security_attr));
    security_attr.bInheritHandle = TRUE;
    security_attr.lpSecurityDescriptor = NULL;

    // https://stackoverflow.com/questions/60645/overlapped-i-o-on-anonymous-pipe
    // TODO: Implement above for asynchronous anonymous pipe communication.

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
        close_handles(&handles);
        return NULL;
    }

    // This is temporary to allow child process to startup.
    Sleep(500);

    Process* process = (Process*)malloc(sizeof(Process));
    process->std_handles = handles;
    process->info = process_info;
    return process;
}

void close_process_windows(Process* process) {
    if (process == NULL) {
        return;
    }

    TerminateProcess(process->info.hProcess, 0);
    close_handles(&process->std_handles);
    free(process);
}

void read_response_windows(Process* process) {
    if (process == NULL) {
        return;
    }

    char read_buffer[PATH_MAX];
    DWORD read = 0;
    BOOL read_result = ReadFile(process->std_handles.child_stdout_read, read_buffer, sizeof(read_buffer), &read, NULL);
    if (!read_result || read == 0) {
        printf("Failed to read from process stdout.\n");
    }
    read_buffer[read] = 0;

    printf("%s\n", read_buffer);
}

void write_request_windows(Process* process, const char* request) {
    if (process == NULL) {
        return;
    }

    DWORD written = 0;
    if (!WriteFile(process->std_handles.child_stdin_write, (void*)request, strlen(request), &written, NULL)) {
        printf("Failed to write to process stdin.\n");
    }
}

#elif POSIX

//
// Process Management Posix
//

#define PIPE_READ 0
#define PIPE_WRITE 1

typedef struct Pipes {
    int in[2];
    int out[2];
} Pipes;

void close_pipes(Pipes* pipes) {
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

Process* create_process_posix(const char* path) {
    Pipes pipes;

    if (pipe(pipes.in) < 0) {
        printf("Failed to create stdin pipes!\n");
        return NULL;
    }

    if (pipe(pipes.out) < 0) {
        printf("Failed to create stdout pipes!\n");
        close_pipes(&pipes);
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
        close_pipes(&pipes);

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

    Process* process = (Process*)malloc(sizeof(Process));
    process->pipes = pipes;
    process->pid = pid;

    sleep(1);

    return process;
}

void close_process_posix(Process* process) {
    if (process == NULL) {
        return;
    }

    close_pipes(&process->pipes);
    kill(process->pid, SIGKILL);
    free(process);
}

void read_response_posix(Process* process) {
    if (process == NULL) {
        return;
    }

    char buffer[32676];
    int bytes_read = read(process->pipes.out[PIPE_READ], (void*)buffer, sizeof(buffer));
    if (bytes_read < 0) {
        printf("Failed to read from child process.\n");
    }
    buffer[bytes_read] = 0;
    printf("%s\n", buffer);
}

void write_request_posix(Process* process, const char* request) {
    if (process == NULL) {
        return;
    }

    ssize_t bytes_written = write(process->pipes.in[PIPE_WRITE], (void*)request, strlen(request));
    if (bytes_written < 0) {
        printf("Failed to write to child process.\n");
    }
}

#endif

//
// Process Management functions
//

Process* create_process(const char* path) {
#if WINDOWS
    return create_process_windows(path);
#elif POSIX
    return create_process_posix(path);
#else
    #error "Current platform does not implement create_process"
#endif
}

void close_process(Process* process) {
#if WINDOWS
    close_process_windows(process);
#elif POSIX
    close_process_posix(process);
#else
    #error "Current platform does not implement close_process"
#endif
}

void read_response(Process* process) {
#if WINDOWS
    read_response_windows(process);
#elif POSIX
    read_response_posix(process);
#else
    #error "Current platform does not implement read_response"
#endif
}

void write_request(Process* process, const char* request) {
#if WINDOWS
    write_request_windows(process, request);
#elif POSIX
    write_request_posix(process, request);
#else
    #error "Current platform does not implement write_request"
#endif
}

void make_request(Process* process, const char* request) {
    const char* content_length = "Content-Length:";
    size_t length = strlen(request);

    // Temporary buffer length.
    // TODO: Is there a way to eliminate this heap allocation?
    char* buffer = (char*)malloc(length + 40);
    sprintf(buffer, "Content-Length: %zu\r\n\r\n%s", length, request);
    write_request(process, buffer);
    free(buffer);
}

//
// JSON API
//
// This section contains all functionality for interacting with JSON objects
// and streams.

typedef enum {
    JSON_VALUE_NULL,
    JSON_VALUE_INT,
    JSON_VALUE_FLOAT,
    JSON_VALUE_STRING,
    // Special type used to point to a const string that doesn't need to be freed.
    JSON_VALUE_STRING_CONST,
    JSON_VALUE_OBJECT,
    JSON_VALUE_ARRAY,
} JSON_VALUE_TYPE;

const char* json_type_to_string(JSON_VALUE_TYPE type) {
    switch (type) {
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

void json_to_string(JSONValue* value, Vector* vector);
void json_object_to_string(JSONObject* object, Vector* vector) {
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

void json_array_to_string(JSONArray* array, Vector* vector) {
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

void json_to_string(JSONValue* value, Vector* vector) {
    // The vector object must be created with an element size of 1.

    if (value == NULL || vector == NULL || vector->element_size != 1) {
        return;
    }

    switch (value->type) {
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

        case JSON_VALUE_NULL:
        default: break;
    }
}

void json_destroy_value(JSONValue* value) {
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

JSONValue json_make_int(int value) {
    JSONValue result;
    result.type = JSON_VALUE_INT;
    result.value.int_value = value;
    return result;
}

JSONValue json_make_float(float value) {
    JSONValue result;
    result.type = JSON_VALUE_FLOAT;
    result.value.float_value = value;
    return result;
}

JSONValue json_make_string(char* value) {
    JSONValue result;
    result.type = JSON_VALUE_STRING;
    result.value.string_value = string_alloc_copy(value);
    return result;
}

JSONValue json_make_string_const(char* value) {
    JSONValue result;
    result.type = JSON_VALUE_STRING_CONST;
    result.value.string_value = value;
    return result;
}

JSONValue json_make_object() {
    JSONValue result;
    result.type = JSON_VALUE_OBJECT;
    result.value.object_value = (JSONObject*)malloc(sizeof(JSONObject));
    result.value.object_value->pairs = vector_create(sizeof(JSONPair));
    return result;
}

JSONValue json_make_array() {
    JSONValue result;
    result.type = JSON_VALUE_ARRAY;
    result.value.array_value = (JSONArray*)malloc(sizeof(JSONArray));
    result.value.array_value->values = vector_create(sizeof(JSONValue));
    return result;
}

void json_object_set(JSONValue* object, JSONValue key, JSONValue value) {
    if (object == NULL || object->value.object_value == NULL || object->type != JSON_VALUE_OBJECT) {
        return;
    }

    if (key.type != JSON_VALUE_STRING && key.type != JSON_VALUE_STRING_CONST) {
        return;
    }

    JSONPair pair;
    pair.key = key;
    pair.value = value;
    vector_push(&object->value.object_value->pairs, (void*)&pair);
}

void json_object_key_set(JSONValue* object, char* key, JSONValue value) {
    json_object_set(object, json_make_string(key), value);
}

void json_object_const_key_set(JSONValue* object, char* key, JSONValue value) {
    json_object_set(object, json_make_string_const(key), value);
}

void json_array_push(JSONValue* array, JSONValue value) {
    if (array == NULL || array->type != JSON_VALUE_ARRAY) {
        return;
    }

    JSONArray* arr = array->value.array_value;
    vector_push(&arr->values, (void*)&value);
}

JSONEncoder json_encode(JSONValue* value) {
    JSONEncoder encoder;
    encoder.string = vector_create(sizeof(char));
    json_to_string(value, &encoder.string);
    vector_append(&encoder.string, (void*)"\0", 1);
    return encoder;
}

void json_destroy_encoder(JSONEncoder* encoder) {
    vector_destroy(&encoder->string);
}

//
// lstalk API
//
// This is the beginning of the exposed API functions for the library.

int lstalk_init() {
    printf("Initialized lstalk version %d.%d.%d!\n", MAJOR, MINOR, REVISION);
    return 1;
}

void lstalk_shutdown() {
}
