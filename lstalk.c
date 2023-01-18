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

static void rpc_send_request(Process* server, Request* request) {
    if (server == NULL || request == NULL) {
        return;
    }

    JSONEncoder encoder = json_encode(&request->payload);
    process_request(server, encoder.string.data);
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

typedef struct LSTalk_Context {
    Vector servers;
    LSTalk_ServerID server_id;
    ClientInfo client_info;
    char* locale;
} LSTalk_Context;

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

LSTalk_Context* lstalk_init() {
    LSTalk_Context* result = (LSTalk_Context*)malloc(sizeof(LSTalk_Context));
    result->servers = vector_create(sizeof(Server));
    result->server_id = 1;
    char buffer[40];
    sprintf(buffer, "%d.%d.%d", LSTALK_MAJOR, LSTALK_MINOR, LSTALK_REVISION);
    result->client_info.name = string_alloc_copy("lstalk");
    result->client_info.version = string_alloc_copy(buffer);
    result->locale = string_alloc_copy("en");
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

    JSONValue client_info = json_make_object();
    json_object_const_key_set(&client_info, "name", json_make_string_const(context->client_info.name));
    json_object_const_key_set(&client_info, "version", json_make_string_const(context->client_info.version));

    JSONValue params = json_make_object();
    json_object_const_key_set(&params, "processId", json_make_int(process_get_current_id()));
    json_object_const_key_set(&params, "clientInfo", client_info);
    json_object_const_key_set(&params, "locale", json_make_string_const(context->locale));
    json_object_const_key_set(&params, "rootUri", json_make_string(connect_params.root_uri));
    json_object_const_key_set(&params, "clientCapabilities", json_make_object());
    json_object_const_key_set(&params, "trace", json_make_string_const(trace_to_string(connect_params.trace)));

    Request request = rpc_make_request(&server.request_id, "initialize", params);
    rpc_send_request(server.process, &request);
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

int lstalk_close(LSTalk_Context* context, LSTalk_ServerID id) {
    if (context == NULL || id == LSTALK_INVALID_SERVER_ID) {
        return 0;
    }

    for (size_t i = 0; i < context->servers.length; i++) {
        Server* server = (Server*)vector_get(&context->servers, i);

        if (server->id == id) {
            Request request = rpc_make_request(&server->request_id, "shutdown", json_make_null());
            rpc_send_request(server->process, &request);
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
                                Request initalized_request = rpc_make_notification("initialized", json_make_null());
                                rpc_send_request(server->process, &initalized_request);
                                rpc_close_request(&initalized_request);
                            } else if (strcmp(method, "shutdown") == 0) {
                                Request exit_notification = rpc_make_notification("exit", json_make_null());
                                rpc_send_request(server->process, &exit_notification);
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
