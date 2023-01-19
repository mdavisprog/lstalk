
#ifndef __LSTALK_H__
#define __LSTALK_H__

struct LSTalk_Context;
typedef int LSTalk_ServerID;
#define LSTALK_INVALID_SERVER_ID -1

typedef enum {
    LSTALK_CONNECTION_STATUS_NOT_CONNECTED,
    LSTALK_CONNECTION_STATUS_CONNECTING,
    LSTALK_CONNECTION_STATUS_CONNECTED,
} LSTalk_ConnectionStatus;

typedef enum {
    LSTALK_TRACE_OFF,
    LSTALK_TRACE_MESSAGES,
    LSTALK_TRACE_VERBOSE,
} LSTalk_Trace;

typedef struct LSTalk_ConnectParams {
    /**
     * The rootUri of the workspace. Is null if no folder is open.
     */
    char* root_uri;

    /**
     * The initial trace setting. If omitted trace is disabled ('off').
     */
    LSTalk_Trace trace;
} LSTalk_ConnectParams;

struct LSTalk_Context* lstalk_init();
void lstalk_shutdown(struct LSTalk_Context* context);
void lstalk_version(int* major, int* minor, int* revision);
void lstalk_set_client_info(struct LSTalk_Context* context, char* name, char* version);
void lstalk_set_locale(struct LSTalk_Context* context, char* locale);
LSTalk_ServerID lstalk_connect(struct LSTalk_Context* context, const char* uri, LSTalk_ConnectParams connect_params);
LSTalk_ConnectionStatus lstalk_get_connection_status(struct LSTalk_Context* context, LSTalk_ServerID id);
int lstalk_close(struct LSTalk_Context* context, LSTalk_ServerID id);
int lstalk_process_responses(struct LSTalk_Context* context);

#ifdef LSTALK_TESTS
void lstalk_tests();
#endif

#endif
