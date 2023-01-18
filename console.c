#include "lstalk.h"
#include <stdio.h>
#include <string.h>

#define INPUT_BUFFER_SIZE 2048

#if _WIN32 || _WIN64
    #define WINDOWS 1
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <wchar.h>
#elif __linux__
    #define LINUX 1
    #include <sys/select.h>
    #include <sys/time.h>
    #include <unistd.h>
#endif

#if LINUX
#define POSIX 1
#endif

#if WINDOWS
wchar_t user_input_buffer[INPUT_BUFFER_SIZE / 2];
size_t user_input_index = 0;

int read_input_windows(char* buffer, size_t size) {
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);

    DWORD num_events = 0;
    if (GetNumberOfConsoleInputEvents(stdin_handle, &num_events) == 0) {
        return 0;
    }

    int result = 0;
    for (DWORD i = 0; i < num_events; i++) {
        INPUT_RECORD record;
        DWORD num_events_read = 0;
        if (ReadConsoleInputW(stdin_handle, &record, 1, &num_events_read) == 0) {
            break;
        }

        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
            continue;
        }

        if (record.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
            wprintf(L"\n");

            const wchar_t* ptr = &user_input_buffer[0];
            mbstate_t state;
            wcsrtombs(buffer, &ptr, size, &state);

            user_input_index = 0;
            user_input_buffer[user_input_index] = L'\0';
            result = 1;
        } else if (record.Event.KeyEvent.wVirtualKeyCode == VK_BACK) {
            wprintf(L"\b \b");
            if (user_input_index > 0) {
                user_input_index--;
                user_input_buffer[user_input_index] = L'\0';
            }
        } else {
            if (record.Event.KeyEvent.uChar.UnicodeChar != 0) {
                wprintf(L"%c", record.Event.KeyEvent.uChar.UnicodeChar);
                user_input_buffer[user_input_index] = record.Event.KeyEvent.uChar.UnicodeChar;
                user_input_index++;
                user_input_buffer[user_input_index] = L'\0';
            }
        }
    }

    return result;
}
#elif POSIX
int read_input_posix(char* buffer, size_t size) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;

    int result = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
    if (result == -1) {
        return 0;
    }

    if (FD_ISSET(STDIN_FILENO, &set)) {
        ssize_t bytes_read = read(STDIN_FILENO, buffer, size);
        // Remove the newline character from the result.
        buffer[bytes_read - 1] = '\0';
        return (int)bytes_read;
    }

    return 0;
}
#endif

int read_input(char* buffer, size_t size) {
#if WINDOWS
    return read_input_windows(buffer, size);
#elif POSIX
    return read_input_posix(buffer, size);
#else
    #error "Current platform does not implement the read_input function!"
#endif
}

int is_command(char* buffer, const char* command) {
    return strcmp(buffer, command) == 0;
}

int main(int argc, char** argv) {
    struct LSTalk_Context* context = lstalk_init();
    if (context == NULL) {
        return -1;
    }

    int major = 0;
    int minor = 0;
    int revision = 0;
    lstalk_version(&major, &minor, &revision);
    printf("Welcome to the LSTalk console application.\n");
    printf("Currently using LSTalk version %d.%d.%d\n", major, minor, revision);
    printf("Provide path to language server:\n");

    char command[INPUT_BUFFER_SIZE];
    LSTalk_ServerID server_id = LSTALK_INVALID_SERVER_ID;
    LSTalk_ConnectParams params;
    params.root_uri = NULL;
    params.trace = LSTALK_TRACE_OFF;

    int quit = 0;
    while (!quit) {
        if (read_input(command, sizeof(command))) {
            if (is_command(command, "quit")) {
                quit = 1;
            } else if (is_command(command, "close")) {
                lstalk_close(context, server_id);
            } else {
                server_id = lstalk_connect(context, command, params);
            }
            command[0] = 0;
        }

        lstalk_process_responses(context);
    }

    lstalk_shutdown(context);
    return 0;
}
