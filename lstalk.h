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


#ifndef __LSTALK_H__
#define __LSTALK_H__

/**
 * Data container for a LSTalk session. Must be created using lstalk_init
 * and destroyed with lstalk_shutdown.
 */
struct LSTalk_Context;

/**
 * ID represting a connection to a language server. The LSTalk_Context
 * object will hold these values when a connection is opened.
 */
typedef int LSTalk_ServerID;
#define LSTALK_INVALID_SERVER_ID -1

/**
 * The connection status to a server.
 */
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
 * Flags to set to aid in debugging the library.
 */
typedef enum {
    LSTALK_DEBUGFLAGS_NONE = 0,
    LSTALK_DEBUGFLAGS_PRINT_REQUESTS = 1 << 0,
    LSTALK_DEBUGFLAGS_PRINT_RESPONSES = 1 << 1,
} LSTalk_DebugFlags;

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

/**
 * Forward declaraction with the defintion defined below the API.
 */
struct LSTalk_ServerInfo;
struct LSTalk_ServerNotification;

/**
 * Initializes a LSTalk_Context object to be used with all of the API functions.
 * 
 * @return A heap-allocated LSTalk_Context object. Must be freed with lstalk_shutdown.
 */
struct LSTalk_Context* lstalk_init();

/**
 * Cleans up a LSTalk_Context object. This will close any existing connections to servers
 * and send shutdown/exit requests to them. The context object memory is then freed.
 * 
 * @param context - The context object to shutdown.
 */
void lstalk_shutdown(struct LSTalk_Context* context);

/**
 * Retrieves the current version number for the LSTalk library.
 * 
 * @param major - A pointer to store the major version number.
 * @param minor - A pointer to store the minor version number.
 * @param revision - A pointer to store the revision number.
 */
void lstalk_version(int* major, int* minor, int* revision);

/**
 * Sets the client information for the given LSTalk_Context object. The default
 * name is "lstalk" and the default version is the library's version number.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param name - A pointer to the name of the client. This function will allocate
 *               its own copy of the string.
 * @param version - A pointer to the version of the client. This function will
 *                  allocate its own copy of the string.
 */
void lstalk_set_client_info(struct LSTalk_Context* context, char* name, char* version);

/**
 * Sets the locale of the client. The default value is 'en'.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param locale - This is an IETF tag. The string is copied.
 */
void lstalk_set_locale(struct LSTalk_Context* context, char* locale);

/**
 * Sets debug flags for the given context object.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param flags - Bitwise flags set from LSTalk_DebugFlags.
 */
void lstalk_set_debug_flags(struct LSTalk_Context* context, int flags);

/**
 * Attempts to connect to a language server at the given URI. This should be a path on the machine to an
 * executable that can be started by the library.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param uri - File path to the language server executable.
 * 
 * @return - Server ID representing a connection to the language server. Will be LSTALK_INVALID_SERVER_ID if
 *           no connection can be made.
 */
LSTalk_ServerID lstalk_connect(struct LSTalk_Context* context, const char* uri, LSTalk_ConnectParams connect_params);

/**
 * Retrieve the current connection status given a Server ID.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - A LSTalk_ServerID to check the connection status for.
 * 
 * @return - The LSTalk_ConnectionStatus of the given server ID.
 */
LSTalk_ConnectionStatus lstalk_get_connection_status(struct LSTalk_Context* context, LSTalk_ServerID id);

/**
 * Retrieve the server information given a LSTalker_ServerID.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - A LSTalk_ServerID to check the connection status for.
 * 
 * @return - The LSTalk_ServerInfo containing information about the server.
 */
struct LSTalk_ServerInfo* lstalk_get_server_info(struct LSTalk_Context* context, LSTalk_ServerID id);

/**
 * Requests to close a connection to a connected language server given the LSTalk_ServerID.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - The LSTalk_ServerID connection to close.
 * 
 * @return - A non-zero value if closed successfully. 0 if there was an error.
 */
int lstalk_close(struct LSTalk_Context* context, LSTalk_ServerID id);

/**
 * Process responses for all connected server.
 * 
 * @param context - An initialized LSTalk_Context object.
 * 
 * @return - A non-zero value if response were processed. 0 if nothing was processed.
 */
int lstalk_process_responses(struct LSTalk_Context* context);

/**
 * Polls for any notifications received from the given server. The context will hol any
 * memory allocated for the notification. Once the notification has been polled, the
 * memory will be freed on the next lstalk_process_responses call. The caller will
 * need to create their own copy of any data they wish to hold onto.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - The server connection to poll.
 * @param notification - Next notification received from the server.
 * 
 * @return - A non-zero value if a notification was polled. 0 if no notification was polled.
 */
int lstalk_poll_notification(struct LSTalk_Context* context, LSTalk_ServerID id, struct LSTalk_ServerNotification* notification);

/**
 * A notification that should be used by the client to modify the trace setting of the server.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - The LSTalk_ServerID connection to set the trace setting on.
 * @param trace - A LSTalk_Trace value to set the new setting.
 * 
 * @return - Non-zero if the request was sent. 0 if it failed.
 */
int lstalk_set_trace(struct LSTalk_Context* context, LSTalk_ServerID id, LSTalk_Trace trace);

/**
 * Calls lstalk_set_trace by converting the string into an LSTalk_Trace value.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - The LSTalk_ServerID connection to set the trace setting on.
 * @param trace - A string representation of a LSTalk_Trace value.
 * 
 * @return - Non-zero if the request was sent. 0 if it failed.
 */
int lstalk_set_trace_from_string(struct LSTalk_Context* context, LSTalk_ServerID id, char* trace);

/**
 * The document open notification is sent from the client to the server to
 * signal newly opened text documents. The library will attempt to open the
 * file to send the contents to the server. The contents will be properly
 * escaped to fit the JSON rpc format.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - The LSTalk_ServerID connection to open the document on.
 * @param path - The absolute path to the file that is opened on the client.
 * 
 * @return - Non-zero if the request was sent. 0 if it failed.
 */
int lstalk_text_document_did_open(struct LSTalk_Context* context, LSTalk_ServerID id, char* path);

/**
 * The document close notification is sent from the client to the server
 * when the document got closed in the client.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - The LSTalk_ServerID connection to open the document on.
 * @param path - The absolute path to the file that is opened on the client.
 * 
 * @return - Non-zero if the request was sent. 0 if it failed.
 */
int lstalk_text_document_did_close(struct LSTalk_Context* context, LSTalk_ServerID id, char* path);

/**
 * Retrieves all symbols for a given document.
 * 
 * @param context - An initialized LSTalk_Context object.
 * @param id - The LSTalk_ServerID connection to open the document on.
 * @param path - The absolute path to the file that is opened on the client.
 * 
 * @return - Non-zero if the request was sent. 0 if it failed.
 */
int lstalk_text_document_document_symbol(struct LSTalk_Context* context, LSTalk_ServerID id, char* path);

//
// The section below contains the definitions of interfaces used in communicating
// with the language server.
//

/**
 * A symbol kind.
 */
typedef enum {
    LSTALK_SYMBOLKIND_NONE = 0,
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
 * Symbol tags are extra annotations that tweak the rendering of a symbol.
 *
 * @since 3.16
 */
typedef enum {
    LSTALK_SYMBOLTAG_DEPRECATED = 1 << 0,
} LSTalk_SymbolTag;

typedef struct LSTalk_WorkDoneProgressOptions {
    int work_done_progress;
} LSTalk_WorkDoneProgressOptions;

/**
 * Static registration options to be returned in the initialize request.
 */
typedef struct LSTalk_StaticRegistrationOptions {
    /**
     * The id used to register the request. The id can be used to deregister
     * the request again. See also Registration#id.
     */
    char* id;
} LSTalk_StaticRegistrationOptions;

/**
 * Defines how the host (editor) should sync document changes to the language
 * server.
 */
typedef enum {
    /**
     * Documents should not be synced at all.
     */
    LSTALK_TEXTDOCUMENTSYNCKIND_NONE = 0,

    /**
     * Documents are synced by always sending the full content
     * of the document.
     */
    LSTALK_TEXTDOCUMENTSYNCKIND_FULL = 1,

    /**
     * Documents are synced by sending the full content on open.
     * After that only incremental updates to the document are
     * sent.
     */
    LSTALK_TEXTDOCUMENTSYNCKIND_INCREMENTAL = 2,
} LSTalk_TextDocumentSyncKind;

/**
 * Defines how text documents are synced.
 */
typedef struct LSTalk_TextDocumentSyncOptions {
    /**
     * Open and close notifications are sent to the server. If omitted open
     * close notifications should not be sent.
     */
    int open_close;

    /**
     * Change notifications are sent to the server. See
     * TextDocumentSyncKind.None, TextDocumentSyncKind.Full and
     * TextDocumentSyncKind.Incremental. If omitted it defaults to
     * TextDocumentSyncKind.None.
     */
    LSTalk_TextDocumentSyncKind change;
} LSTalk_TextDocumentSyncOptions;

/**
 * A notebook document filter denotes a notebook document by
 * different properties.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_NotebookDocumentFilter {
    /** The type of the enclosing notebook. */
    char* notebook_type;

    /** A Uri [scheme](#Uri.scheme), like `file` or `untitled`. */
    char* scheme;

    /** A glob pattern. */
    char* pattern;
} LSTalk_NotebookDocumentFilter;

/**
 * The notebooks to be synced
 */
typedef struct LSTalk_NotebookSelector {
    /**
     * The notebook to be synced. If a string
     * value is provided it matches against the
     * notebook type. '*' matches every notebook.
     */
    LSTalk_NotebookDocumentFilter notebook;

    /**
     * The cells of the matching notebook to be synced.
     */
    char** cells;
    int cells_count;
} LSTalk_NotebookSelector;

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
typedef struct LSTalk_NotebookDocumentSyncOptions {
    LSTalk_StaticRegistrationOptions static_registration;

    /**
     * The notebooks to be synced
     */
    LSTalk_NotebookSelector* notebook_selector;
    int notebook_selector_count;

    /**
     * Whether save notification should be forwarded to
     * the server. Will only be honored if mode === `notebook`.
     */
    int save;
} LSTalk_NotebookDocumentSyncOptions;

/**
 * Completion options.
 */
typedef struct LSTalk_CompletionOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;

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
    int resolve_provider;

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
    int completion_item_label_details_support;
} LSTalk_CompletionOptions;

/**
 * Hover options.
 */
typedef struct LSTalk_HoverOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;

    /**
     * LSTalk specific value. Determines whether the server provides hover support.
     */
    int is_supported;
} LSTalk_HoverOptions;

/**
 * The server provides signature help support.
 */
typedef struct LSTalk_SignatureHelpOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;

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
} LSTalk_SignatureHelpOptions;

/**
 * A document filter denotes a document through properties like language, scheme or pattern.
 */
typedef struct LSTalk_DocumentFilter {
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
} LSTalk_DocumentFilter;

/**
 * General text document registration options.
 */
typedef struct LSTalk_TextDocumentRegistrationOptions {
    /**
     * A document selector to identify the scope of the registration. If set to
     * null the document selector provided on the client side will be used.
     */
    LSTalk_DocumentFilter* document_selector;
    int document_selector_count;
} LSTalk_TextDocumentRegistrationOptions;

/**
 * The server provides go to declaration support.
 *
 * @since 3.14.0
 */
typedef struct LSTalk_DeclarationRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_DeclarationRegistrationOptions;

/**
 * The server provides goto definition support.
 */
typedef struct LSTalk_DefinitionOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;
} LSTalk_DefinitionOptions;

/**
 * The server provides goto type definition support.
 *
 * @since 3.6.0
 */
typedef struct LSTalk_TypeDefinitionRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_TypeDefinitionRegistrationOptions;

/**
 * The server provides goto implementation support.
 *
 * @since 3.6.0
 */
typedef struct LSTalk_ImplementationRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_ImplementationRegistrationOptions;

/**
 * The server provides find references support.
 */
typedef struct LSTalk_ReferenceOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;
} LSTalk_ReferenceOptions;

/**
 * The server provides document highlight support.
 */
typedef struct LSTalk_DocumentHighlightOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;
} LSTalk_DocumentHighlightOptions;

/**
 * The server provides document symbol support.
 */
typedef struct LSTalk_DocumentSymbolOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;

    /**
     * A human-readable string that is shown when multiple outlines trees
     * are shown for the same document.
     *
     * @since 3.16.0
     */
    char* label;
} LSTalk_DocumentSymbolOptions;

/**
 * The server provides code actions. The `CodeActionOptions` return type is
 * only valid if the client signals code action literal support via the
 * property `textDocument.codeAction.codeActionLiteralSupport`.
 */
typedef struct LSTalk_CodeActionOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;

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
    int resolve_provider;
} LSTalk_CodeActionOptions;

/**
 * The server provides code lens.
 */
typedef struct LSTalk_CodeLensOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;

    /**
     * Code lens has a resolve provider as well.
     */
    int resolve_provider;
} LSTalk_CodeLensOptions;

/**
 * The server provides document link support.
 */
typedef struct LSTalk_DocumentLinkOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;

    /**
     * Document links have a resolve provider as well.
     */
    int resolve_provider;
} LSTalk_DocumentLinkOptions;

/**
 * The server provides color provider support.
 *
 * @since 3.6.0
 */
typedef struct LSTalk_DocumentColorRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_DocumentColorRegistrationOptions;

/**
 * The server provides document formatting.
 */
typedef struct LSTalk_DocumentFormattingOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;
} LSTalk_DocumentFormattingOptions;

/**
 * The server provides document range formatting.
 */
typedef struct LSTalk_DocumentRangeFormattingOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;
} LSTalk_DocumentRangeFormattingOptions;

/**
 * The server provides document formatting on typing.
 */
typedef struct LSTalk_DocumentOnTypeFormattingOptions {
    /**
     * A character on which formatting should be triggered, like `{`.
     */
    char* first_trigger_character;

    /**
     * More trigger characters.
     */
    char** more_trigger_character;
    int more_trigger_character_count;
} LSTalk_DocumentOnTypeFormattingOptions;

/**
 * The server provides rename support. RenameOptions may only be
 * specified if the client states that it supports
 * `prepareSupport` in its initial `initialize` request.
 */
typedef struct LSTalk_RenameOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;

    /**
     * Renames should be checked and tested before being executed.
     */
    int prepare_provider;
} LSTalk_RenameOptions;

/**
 * The server provides folding provider support.
 *
 * @since 3.10.0
 */
typedef struct LSTalk_FoldingRangeRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_FoldingRangeRegistrationOptions;

/**
 * The server provides execute command support.
 */
typedef struct LSTalk_ExecuteCommandOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;

    /**
     * The commands to be executed on the server
     */
    char** commands;
    int commands_count;
} LSTalk_ExecuteCommandOptions;

/**
 * The server provides selection range support.
 *
 * @since 3.15.0
 */
typedef struct LSTalk_SelectionRangeRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_SelectionRangeRegistrationOptions;

/**
 * The server provides linked editing range support.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_LinkedEditingRangeRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_LinkedEditingRangeRegistrationOptions;

/**
 * The server provides call hierarchy support.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_CallHierarchyRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_CallHierarchyRegistrationOptions;

typedef struct LSTalk_SemanticTokensLegend {
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
} LSTalk_SemanticTokensLegend;

typedef struct LSTalk_SemanticTokensOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;

    /**
     * The legend used by the server
     */
    LSTalk_SemanticTokensLegend legend;

    /**
     * Server supports providing semantic tokens for a specific range
     * of a document.
     */
    int range;

    /**
     * Server supports providing semantic tokens for a full document.
     *
     * full:
     * 
     * The server supports deltas for full documents.
     */
    int full_delta;
} LSTalk_SemanticTokensOptions;

/**
 * The server provides semantic tokens support.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_SemanticTokensRegistrationOptions {
    LSTalk_SemanticTokensOptions semantic_tokens;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
} LSTalk_SemanticTokensRegistrationOptions;

/**
 * Whether server provides moniker support.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_MonikerRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    int is_supported;
} LSTalk_MonikerRegistrationOptions;

/**
 * The server provides type hierarchy support.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_TypeHierarchyRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_TypeHierarchyRegistrationOptions;

/**
 * The server provides inline values.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_InlineValueRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_InlineValueRegistrationOptions;

/**
 * Inlay hint options used during static or dynamic registration.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_InlayHintRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;
    int is_supported;
} LSTalk_InlayHintRegistrationOptions;

/**
 * Diagnostic registration options.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_DiagnosticRegistrationOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    LSTalk_TextDocumentRegistrationOptions text_document_registration;
    LSTalk_StaticRegistrationOptions static_registration;

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
    int inter_file_dependencies;

    /**
     * The server provides support for workspace diagnostics as well.
     */
    int workspace_diagnostics;
} LSTalk_DiagnosticRegistrationOptions;

/**
 * The server provides workspace symbol support.
 */
typedef struct LSTalk_WorkspaceSymbolOptions {
    LSTalk_WorkDoneProgressOptions work_done_progress;
    int is_supported;

    /**
     * The server provides support to resolve additional
     * information for a workspace symbol.
     *
     * @since 3.17.0
     */
    int resolve_provider;
} LSTalk_WorkspaceSymbolOptions;

/**
 * The server supports workspace folder.
 *
 * @since 3.6.0
 */
typedef struct LSTalk_WorkspaceFoldersServerCapabilities {
    /**
     * The server has support for workspace folders
     */
    int supported;

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
    int change_notifications_boolean;
} LSTalk_WorkspaceFoldersServerCapabilities;

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
    LSTALK_FILEOPERATIONPATTERNKIND_FILE = 1 << 0,

    /**
     * The pattern matches a folder only.
     */
    LSTALK_FILEOPERATIONPATTERNKIND_FOLDER = 1 << 1,
} LSTalk_FileOperationPatternKind;

/**
 * Matching options for the file operation pattern.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_FileOperationPatternOptions {
    /**
     * The pattern should be matched ignoring casing.
     */
    int ignore_case;
} LSTalk_FileOperationPatternOptions;

/**
 * The options to register for file operations.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_FileOperationPattern {
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
    LSTalk_FileOperationPatternOptions options;
} LSTalk_FileOperationPattern;

/**
 * A filter to describe in which file operation requests or notifications
 * the server is interested in.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_FileOperationFilter {
    /**
     * A Uri like `file` or `untitled`.
     */
    char* scheme;

    /**
     * The actual file operation pattern.
     */
    LSTalk_FileOperationPattern pattern;
} LSTalk_FileOperationFilter;

/**
 * The options to register for file operations.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_FileOperationRegistrationOptions {
    /**
     * The actual filters.
     */
    LSTalk_FileOperationFilter* filters;
    int filters_count;
} LSTalk_FileOperationRegistrationOptions;

typedef struct LSTalk_FileOperationsServer {
    /**
     * The server is interested in receiving didCreateFiles
     * notifications.
     */
    LSTalk_FileOperationRegistrationOptions did_create;

    /**
     * The server is interested in receiving willCreateFiles requests.
     */
    LSTalk_FileOperationRegistrationOptions will_create;

    /**
     * The server is interested in receiving didRenameFiles
     * notifications.
     */
    LSTalk_FileOperationRegistrationOptions did_rename;

    /**
     * The server is interested in receiving willRenameFiles requests.
     */
    LSTalk_FileOperationRegistrationOptions will_rename;

    /**
     * The server is interested in receiving didDeleteFiles file
     * notifications.
     */
    LSTalk_FileOperationRegistrationOptions did_delete;

    /**
     * The server is interested in receiving willDeleteFiles file
     * requests.
     */
    LSTalk_FileOperationRegistrationOptions will_delete;
} LSTalk_FileOperationsServer;

/**
 * Workspace specific server capabilities
 */
typedef struct LSTalk_WorkspaceServer {
    /**
     * The server supports workspace folder.
     *
     * @since 3.6.0
     */
    LSTalk_WorkspaceFoldersServerCapabilities workspace_folders;

    /**
     * The server is interested in file notifications/requests.
     *
     * @since 3.16.0
     */
    LSTalk_FileOperationsServer file_operations;
} LSTalk_WorkspaceServer;

/**
 * The capabilities the language server provides.
 */
typedef struct LSTalk_ServerCapabilities {
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
    LSTalk_TextDocumentSyncOptions text_document_sync;

    /**
     * Defines how notebook documents are synced.
     *
     * @since 3.17.0
     */
    LSTalk_NotebookDocumentSyncOptions notebook_document_sync;

    /**
     * The server provides completion support.
     */
    LSTalk_CompletionOptions completion_provider;

    /**
     * The server provides hover support.
     */
    LSTalk_HoverOptions hover_provider;

    /**
     * The server provides signature help support.
     */
    LSTalk_SignatureHelpOptions signature_help_provider;
    /**
     * The server provides go to declaration support.
     *
     * @since 3.14.0
     */
    LSTalk_DeclarationRegistrationOptions declaration_provider;

    /**
     * The server provides goto definition support.
     */
    LSTalk_DefinitionOptions definition_provider;

    /**
     * The server provides goto type definition support.
     *
     * @since 3.6.0
     */
    LSTalk_TypeDefinitionRegistrationOptions type_definition_provider;

    /**
     * The server provides goto implementation support.
     *
     * @since 3.6.0
     */
    LSTalk_ImplementationRegistrationOptions implementation_provider;

    /**
     * The server provides find references support.
     */
    LSTalk_ReferenceOptions references_provider;

    /**
     * The server provides document highlight support.
     */
    LSTalk_DocumentHighlightOptions document_highlight_provider;

    /**
     * The server provides document symbol support.
     */
    LSTalk_DocumentSymbolOptions document_symbol_provider;

    /**
     * The server provides code actions. The `CodeActionOptions` return type is
     * only valid if the client signals code action literal support via the
     * property `textDocument.codeAction.codeActionLiteralSupport`.
     */
    LSTalk_CodeActionOptions code_action_provider;

    /**
     * The server provides code lens.
     */
    LSTalk_CodeLensOptions code_lens_provider;

    /**
     * The server provides document link support.
     */
    LSTalk_DocumentLinkOptions document_link_provider;

    /**
     * The server provides color provider support.
     *
     * @since 3.6.0
     */
    LSTalk_DocumentColorRegistrationOptions color_provider;

    /**
     * The server provides document formatting.
     */
    LSTalk_DocumentFormattingOptions document_formatting_provider;

    /**
     * The server provides document range formatting.
     */
    LSTalk_DocumentRangeFormattingOptions document_range_rormatting_provider;

    /**
     * The server provides document formatting on typing.
     */
    LSTalk_DocumentOnTypeFormattingOptions document_on_type_formatting_provider;

    /**
     * The server provides rename support. RenameOptions may only be
     * specified if the client states that it supports
     * `prepareSupport` in its initial `initialize` request.
     */
    LSTalk_RenameOptions rename_provider;

    /**
     * The server provides folding provider support.
     *
     * @since 3.10.0
     */
    LSTalk_FoldingRangeRegistrationOptions folding_range_provider;

    /**
     * The server provides execute command support.
     */
    LSTalk_ExecuteCommandOptions execute_command_provider;

    /**
     * The server provides selection range support.
     *
     * @since 3.15.0
     */
    LSTalk_SelectionRangeRegistrationOptions selection_range_provider;

    /**
     * The server provides linked editing range support.
     *
     * @since 3.16.0
     */
    LSTalk_LinkedEditingRangeRegistrationOptions linked_editing_range_provider;

    /**
     * The server provides call hierarchy support.
     *
     * @since 3.16.0
     */
    LSTalk_CallHierarchyRegistrationOptions call_hierarchy_provider;

    /**
     * The server provides semantic tokens support.
     *
     * @since 3.16.0
     */
    LSTalk_SemanticTokensRegistrationOptions semantic_tokens_provider;

    /**
     * Whether server provides moniker support.
     *
     * @since 3.16.0
     */
    LSTalk_MonikerRegistrationOptions moniker_provider;

    /**
     * The server provides type hierarchy support.
     *
     * @since 3.17.0
     */
    LSTalk_TypeHierarchyRegistrationOptions type_hierarchy_provider;

    /**
     * The server provides inline values.
     *
     * @since 3.17.0
     */
    LSTalk_InlineValueRegistrationOptions inline_value_provider;

    /**
     * The server provides inlay hints.
     *
     * @since 3.17.0
     */
    LSTalk_InlayHintRegistrationOptions inlay_hint_provider;

    /**
     * The server has support for pull model diagnostics.
     *
     * @since 3.17.0
     */
    LSTalk_DiagnosticRegistrationOptions diagnostic_provider;

    /**
     * The server provides workspace symbol support.
     */
    LSTalk_WorkspaceSymbolOptions workspace_symbol_provider;

    /**
     * Workspace specific server capabilities
     */
    LSTalk_WorkspaceServer workspace;
} LSTalk_ServerCapabilities;

/**
 * Information about the server.
 *
 * @since 3.15.0
 */
typedef struct LSTalk_ServerInfo {
    /**
     * The name of the server as defined by the server.
     */
    char* name;

    /**
     * The server's version as defined by the server.
     */
    char* version;

    /**
     * The capabilities the language server provides.
     */
    LSTalk_ServerCapabilities capabilities;
} LSTalk_ServerInfo;

/**
 * Position in a text document expressed as zero-based line and zero-based
 * character offset. A position is between two characters like an ‘insert’
 * cursor in an editor. Special values like for example -1 to denote the end
 * of a line are not supported.
 */
typedef struct LSTalk_Position {
    /**
     * Line position in a document (zero-based).
     */
    unsigned int line;

    /**
     * Character offset on a line in a document (zero-based). The meaning of this
     * offset is determined by the negotiated `PositionEncodingKind`.
     *
     * If the character value is greater than the line length it defaults back
     * to the line length.
     */
    unsigned int character;
} LSTalk_Position;

/**
 * A range in a text document expressed as (zero-based) start and end positions.
 * A range is comparable to a selection in an editor. Therefore the end position
 * is exclusive. If you want to specify a range that contains a line including the
 * line ending character(s) then use an end position denoting the start of the
 * next line.
 */
typedef struct LSTalk_Range {
    /**
     * The range's start position.
     */
    LSTalk_Position start;

    /**
     * The range's end position.
     */
    LSTalk_Position end;
} LSTalk_Range;

/**
 * Represents a location inside a resource, such as a line inside a text file.
 */
typedef struct LSTalk_Location {
    char* uri;
    LSTalk_Range range;
} LSTalk_Location;

/**
 * A diagnostic's severity.
 */
typedef enum {
    /**
     * Reports an error.
     */
    LSTALK_DIAGNOSTICSEVERITY_ERROR = 1,

    /**
     * Reports a warning.
     */
    LSTALK_DIAGNOSTICSEVERITY_WARNING = 2,

    /**
     * Reports an information.
     */
    LSTALK_DIAGNOSTICSEVERITY_INFORMATION = 3,

    /**
     * Reports a hint.
     */
    LSTALK_DIAGNOSTICSEVERITY_HINT = 4,
} LSTalk_DiagnosticSeverity;

/**
 * Structure to capture a description for an error code.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_CodeDescription {
    /**
     * An URI to open with more information about the diagnostic error.
     */
    char* href;
} LSTalk_CodeDescription;

/**
 * Represents a related message and source code location for a diagnostic.
 * This should be used to point to code locations that cause or are related to
 * a diagnostics, e.g when duplicating a symbol in a scope.
 */
typedef struct LSTalk_DiagnosticRelatedInformation {
    /**
     * The location of this related diagnostic information.
     */
    LSTalk_Location location;

    /**
     * The message of this related diagnostic information.
     */
    char* message;
} LSTalk_DiagnosticRelatedInformation;

/**
 * Represents a diagnostic, such as a compiler error or warning. Diagnostic
 * objects are only valid in the scope of a resource.
 */
typedef struct LSTalk_Diagnostic {
    /**
     * The range at which the message applies.
     */
    LSTalk_Range range;

    /**
     * The diagnostic's severity. Can be omitted. If omitted it is up to the
     * client to interpret diagnostics as error, warning, info or hint.
     */
    LSTalk_DiagnosticSeverity severity;

    /**
     * The diagnostic's code, which might appear in the user interface.
     */
    char* code;

    /**
     * An optional property to describe the error code.
     *
     * @since 3.16.0
     */
    LSTalk_CodeDescription code_description;

    /**
     * A human-readable string describing the source of this
     * diagnostic, e.g. 'typescript' or 'super lint'.
     */
    char* source;

    /**
     * The diagnostic's message.
     */
    char* message;

    /**
     * Additional metadata about the diagnostic.
     * 
     * This will contain flags from LSTalk_DiagnosticTag.
     *
     * @since 3.15.0
     */
    int tags;

    /**
     * An array of related diagnostic information, e.g. when symbol-names within
     * a scope collide all definitions can be marked via this property.
     */
    LSTalk_DiagnosticRelatedInformation* related_information;
    int related_information_count;
} LSTalk_Diagnostic;

/**
 * Diagnostics notification are sent from the server to the client to signal
 * results of validation runs.
 */
typedef struct LSTalk_PublishDiagnostics {
    /**
     * The URI for which diagnostic information is reported.
     */
    char* uri;

    /**
     * Optional the version number of the document the diagnostics are published
     * for.
     *
     * @since 3.15.0
     */
    int version;

    /**
     * An array of diagnostic information items.
     */
    LSTalk_Diagnostic* diagnostics;
    int diagnostics_count;
} LSTalk_PublishDiagnostics;

/**
 * Represents programming constructs like variables, classes, interfaces etc.
 * that appear in a document. Document symbols can be hierarchical and they
 * have two ranges: one that encloses its definition and one that points to its
 * most interesting range, e.g. the range of an identifier.
 */
typedef struct LSTalk_DocumentSymbol {
    /**
     * The name of this symbol. Will be displayed in the user interface and
     * therefore must not be an empty string or a string only consisting of
     * white spaces.
     */
    char* name;
    
    /**
     * More detail for this symbol, e.g the signature of a function.
     */
    char* detail;

    /**
     * The kind of this symbol.
     */
    LSTalk_SymbolKind kind;

    /**
     * Tags for this document symbol. 
     *
     * @since 3.16.0
     */
    int tags;

    /**
     * The range enclosing this symbol not including leading/trailing whitespace
     * but everything else like comments. This information is typically used to
     * determine if the clients cursor is inside the symbol to reveal in the
     * symbol in the UI.
     */
    LSTalk_Range range;

    /**
     * The range that should be selected and revealed when this symbol is being
     * picked, e.g. the name of a function. Must be contained by the `range`.
     */
    LSTalk_Range selection_range;

    /**
     * Children of this symbol, e.g. properties of a class.
     */
    struct LSTalk_DocumentSymbol* children;
    int children_count;
} LSTalk_DocumentSymbol;

/**
 * Response received from a document_symbol request.
 */
typedef struct LSTalk_DocumentSymbolNotification {
    LSTalk_DocumentSymbol* symbols;
    int symbols_count;
} LSTalk_DocumentSymbolNotification;

/**
 * Different types of notifications/responses from the server.
 */
typedef enum {
    LSTALK_NOTIFICATION_NONE,
    LSTALK_NOTIFICATION_TEXT_DOCUMENT_SYMBOLS,
    LSTALK_NOTIFICATION_PUBLISHDIAGNOSTICS,
} LSTalk_NotificationType;

/**
 * A notification from the server. The struct will contain
 * a type and the data associated with the type.
 */
typedef struct LSTalk_ServerNotification {
    union {
        LSTalk_DocumentSymbolNotification document_symbols;
        LSTalk_PublishDiagnostics publish_diagnostics;
    } data;

    LSTalk_NotificationType type;
    int polled;
} LSTalk_ServerNotification;

#ifdef LSTALK_TESTS
void lstalk_tests();
#endif

#endif
