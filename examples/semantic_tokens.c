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
        printf("Failed to initialize LSTalk context!\n");
        return -1;
    }

    char exe_path[PATH_MAX];
    utility_absolute_path(argv[0], exe_path, sizeof(exe_path));

    char directory[PATH_MAX];
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

        printf("Requesting semantic tokens...\n");
        lstalk_text_document_semantic_tokens(context, server, file_path);

        char* contents = utility_file_contents(file_path);

        while (1) {
            lstalk_process_responses(context);

            LSTalk_Notification notification;
            lstalk_poll_notification(context, server, &notification);
            if (notification.type == LSTALK_NOTIFICATION_SEMANTIC_TOKENS) {
                printf("Result ID: %s\n", notification.data.semantic_tokens.result_id);
                printf("Tokens: %d\n", notification.data.semantic_tokens.tokens_count);
                for (int i = 0; i < notification.data.semantic_tokens.tokens_count; i++) {
                    LSTalk_SemanticToken* token = &notification.data.semantic_tokens.tokens[i];
                    printf("=== %.*s\n", token->length, utility_get_token_ptr(contents, token->line, token->character));
                    printf("   Token Type: %s\n", token->token_type);
                    if (token->token_modifiers_count > 0) {
                        printf("   Modifiers:\n");
                        for (int j = 0; j < token->token_modifiers_count; j++) {
                            printf("      %s\n", token->token_modifiers[j]);
                        }
                    }
                }

                break;
            }
        }

        if (contents != NULL) {
            free(contents);
        }

        lstalk_close(context, server);
    }

    lstalk_shutdown(context);

    return 0;
}
