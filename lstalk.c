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

void vector_push(Vector* vector, void* element) {
    if (vector == NULL || element == NULL || vector->element_size == 0) {
        return;
    }

    if (vector->length == vector->capacity) {
        vector->capacity = vector->capacity * 2;
        vector->data = realloc(vector->data, vector->element_size * vector->capacity);
    }

    size_t offset = vector->length * vector->element_size;
    memcpy(vector->data + offset, element, vector->element_size);
    vector->length++;
}

char* vector_get(Vector* vector, size_t index) {
    if (vector == NULL || vector->element_size == 0 || index >= vector->length) {
        return NULL;
    }

    return &vector->data[vector->element_size * index];
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
// lstalk API
//
// This is the beginning of the exposed API functions for the library.

int lstalk_init() {
    printf("Initialized lstalk version %d.%d.%d!\n", MAJOR, MINOR, REVISION);
    return 1;
}

void lstalk_shutdown() {
}
