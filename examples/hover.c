#include "../lstalk.h"
#include "utility.h"
#include <stdio.h>

#if WINDOWS
    #define CLANGD "clangd.exe"
#else
    #define CLANGD "clangd"
#endif

int main(int argc, char** argv) {
    (void)argc;

    struct LSTalk_Context* context = lstalk_init();
    if (context == NULL) {
        return -1;
    }

    int major = 0;
    int minor = 0;
    int revision = 0;
    lstalk_version(&major, &minor, &revision);
    printf("LSTalk version %d.%d.%d\n", major, minor, revision);
    printf("Hover example\n");

    char exe_path[PATH_MAX] = "";
    utility_absolute_path(argv[0], exe_path, sizeof(exe_path));

    char directory[PATH_MAX] = "";
    utility_get_directory(exe_path, directory, sizeof(directory));

    char file_path[PATH_MAX] = "";
    strcat_s(file_path, sizeof(file_path), directory);
    strcat_s(file_path, sizeof(file_path), "/");
    strcat_s(file_path, sizeof(file_path), "../examples/example.cpp");

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

        printf("Opening text document: %s...\n", file_path);
        lstalk_text_document_did_open(context, server, file_path);

        printf("Hovering...\n");
        lstalk_text_document_hover(context, server, file_path, 15, 7);

        while (1) {
            lstalk_process_responses(context);

            LSTalk_Notification notification;
            lstalk_poll_notification(context, server, &notification);
            if (notification.type == LSTALK_NOTIFICATION_HOVER) {
                printf("URI: %s\n", notification.data.hover.uri);
                printf("Contents: \n%s\n", notification.data.hover.contents);
                break;
            }
        }

        lstalk_close(context, server);
    }

    lstalk_shutdown(context);

    return 0;
}
