
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

/**
 * The kind of resource operations supported by the client.
 */
typedef enum {
    /**
     * Supports creating new files and folders.
     */
    LSTALK_RESOURCEOPERATIONKIND_CREATE = 1 << 0,

    /**
     * Supports renaming existing files and folders.
     */
    LSTALK_RESOURCEOPERATIONKIND_RENAME = 1 << 1,

    /**
     * Supports deleting existing files and folders.
     */
    LSTALK_RESOURCEOPERATIONKIND_DELETE = 1 << 2,
} LSTalk_ResourceOperationKind;

/**
 * A symbol kind.
 */
typedef enum {
    LSTALK_SYMBOLKIND_FILE = 1 << 0,
    LSTALK_SYMBOLKIND_MODULE = 1 << 1,
    LSTALK_SYMBOLKIND_NAMESPACE = 1 << 2,
    LSTALK_SYMBOLKIND_PACKAGE = 1 << 3,
    LSTALK_SYMBOLKIND_CLASS = 1 << 4,
    LSTALK_SYMBOLKIND_METHOD = 1 << 5,
    LSTALK_SYMBOLKIND_PROPERTY = 1 << 6,
    LSTALK_SYMBOLKIND_FIELD = 1 << 7,
    LSTALK_SYMBOLKIND_CONSTRUCTOR = 1 << 8,
    LSTALK_SYMBOLKIND_ENUM = 1 << 9,
    LSTALK_SYMBOLKIND_INTERFACE = 1 << 10,
    LSTALK_SYMBOLKIND_FUNCTION = 1 << 11,
    LSTALK_SYMBOLKIND_VARIABLE = 1 << 12,
    LSTALK_SYMBOLKIND_CONSTANT = 1 << 13,
    LSTALK_SYMBOLKIND_STRING = 1 << 14,
    LSTALK_SYMBOLKIND_NUMBER = 1 << 15,
    LSTALK_SYMBOLKIND_BOOLEAN = 1 << 16,
    LSTALK_SYMBOLKIND_ARRAY = 1 << 17,
    LSTALK_SYMBOLKIND_OBJECT = 1 << 18,
    LSTALK_SYMBOLKIND_KEY = 1 << 19,
    LSTALK_SYMBOLKIND_NULL = 1 << 20,
    LSTALK_SYMBOLKIND_ENUMMEMBER = 1 << 21,
    LSTALK_SYMBOLKIND_STRUCT = 1 << 22,
    LSTALK_SYMBOLKIND_EVENT = 1 << 23,
    LSTALK_SYMBOLKIND_OPERATOR = 1 << 24,
    LSTALK_SYMBOLKIND_TYPEPARAMETER = 1 << 25,
    LSTALK_SYMBOLKIND_ALL =  LSTALK_SYMBOLKIND_FILE
        | LSTALK_SYMBOLKIND_MODULE
        | LSTALK_SYMBOLKIND_NAMESPACE
        | LSTALK_SYMBOLKIND_PACKAGE
        | LSTALK_SYMBOLKIND_CLASS
        | LSTALK_SYMBOLKIND_METHOD
        | LSTALK_SYMBOLKIND_PROPERTY
        | LSTALK_SYMBOLKIND_FIELD
        | LSTALK_SYMBOLKIND_CONSTRUCTOR
        | LSTALK_SYMBOLKIND_ENUM
        | LSTALK_SYMBOLKIND_INTERFACE
        | LSTALK_SYMBOLKIND_FUNCTION
        | LSTALK_SYMBOLKIND_VARIABLE
        | LSTALK_SYMBOLKIND_CONSTANT
        | LSTALK_SYMBOLKIND_STRING
        | LSTALK_SYMBOLKIND_NUMBER
        | LSTALK_SYMBOLKIND_BOOLEAN
        | LSTALK_SYMBOLKIND_ARRAY
        | LSTALK_SYMBOLKIND_OBJECT
        | LSTALK_SYMBOLKIND_KEY
        | LSTALK_SYMBOLKIND_NULL
        | LSTALK_SYMBOLKIND_ENUMMEMBER
        | LSTALK_SYMBOLKIND_STRUCT
        | LSTALK_SYMBOLKIND_EVENT
        | LSTALK_SYMBOLKIND_OPERATOR
        | LSTALK_SYMBOLKIND_TYPEPARAMETER,
} LSTalk_SymbolKind;

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
    LSTALK_FAILUREHANDLINGKIND_ABORT = 1 << 0,

    /**
     * All operations are executed transactional. That means they either all
     * succeed or no changes at all are applied to the workspace.
     */
    LSTALK_FAILUREHANDLINGKIND_TRANSACTIONAL = 1 << 1,

    /**
     * If the workspace edit contains only textual file changes they are
     * executed transactional. If resource changes (create, rename or delete
     * file) are part of the change the failure handling strategy is abort.
     */
    LSTALK_FAILUREHANDLINGKIND_TEXTONLYTRANSACTIONAL = 1 << 2,

    /**
     * The client tries to undo the operations already executed. But there is no
     * guarantee that this is succeeding.
     */
    LSTALK_FAILUREHANDLINGKIND_UNDO = 1 << 3,
} LSTalk_FailureHandlingKind;

/**
 * Capabilities specific to `WorkspaceEdit`s
 */
typedef struct LSTalk_WorkspaceEditClientCapabilities {
    /**
     * The client supports versioned document changes in `WorkspaceEdit`s
     */
    int document_changes;

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
    int normalizes_line_endings;

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
    int groups_on_label;
} LSTalk_WorkspaceEditClientCapabilities;

/**
 * Capabilities specific to the `workspace/didChangeConfiguration`
 * notification.
 */
typedef struct LSTalk_DidChangeConfigurationClientCapabilities {
    /**
     * Did change configuration notification supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_DidChangeConfigurationClientCapabilities;

/**
 * Capabilities specific to the `workspace/didChangeWatchedFiles`
 * notification.
 */
typedef struct LSTalk_DidChangeWatchedFilesClientCapabilities {
    /**
     * Did change watched files notification supports dynamic registration.
     * Please note that the current protocol doesn't support static
     * configuration for file changes from the server side.
     */
    int dynamic_registration;

    /**
     * Whether the client has support for relative patterns
     * or not.
     *
     * @since 3.17.0
     */
    int relative_pattern_support;
} LSTalk_DidChangeWatchedFilesClientCapabilities;

typedef struct LSTalk_WorkspaceSymbolClientCapabilities {
    /**
     * Symbol request supports dynamic registration.
     */
    int dynamic_registration;

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
    long long value_set;
} LSTalk_WorkspaceSymbolClientCapabilities;

/**
 * Workspace specific client capabilities.
 */
typedef struct LSTalk_Workspace {
    /**
     * The client supports applying batch edits
     * to the workspace by supporting the request
     * 'workspace/applyEdit'
     */
    int apply_edit;

    /**
     * Capabilities specific to `WorkspaceEdit`s
     */
    LSTalk_WorkspaceEditClientCapabilities workspace_edit;

    /**
     * Capabilities specific to the `workspace/didChangeConfiguration`
     * notification.
     */
    LSTalk_DidChangeConfigurationClientCapabilities did_change_configuration;

    /**
     * Capabilities specific to the `workspace/didChangeWatchedFiles`
     * notification.
     */
    LSTalk_DidChangeWatchedFilesClientCapabilities did_change_watched_files;

    /**
     * Capabilities specific to the `workspace/symbol` request.
     */
    LSTalk_WorkspaceSymbolClientCapabilities symbol;
} LSTalk_Workspace;

/**
 * The capabilities provided by the client (editor or tool)
 */
typedef struct LSTalk_ClientCapabilities {
    /**
     * Workspace specific client capabilities.
     */
    LSTalk_Workspace workspace;
} LSTalk_ClientCapabilities;

typedef struct LSTalk_ConnectParams {
    /**
     * The rootUri of the workspace. Is null if no folder is open.
     */
    char* root_uri;

    /**
     * The capabilities provided by the client (editor or tool)
     */
    LSTalk_ClientCapabilities capabilities;

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
