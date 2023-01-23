
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
 * Symbol tags are extra annotations that tweak the rendering of a symbol.
 *
 * @since 3.16
 */
typedef enum {
    LSTALK_SYMBOLTAG_DEPRECATED = 1 << 0,
} LSTalk_SymbolTag;

/**
 * Describes the content type that a client supports in various
 * result literals like `Hover`, `ParameterInfo` or `CompletionItem`.
 *
 * Please note that `MarkupKinds` must not start with a `$`. This kinds
 * are reserved for internal usage.
 */
typedef enum {
    /**
     * Plain text is supported as a content format
     */
    LSTALK_MARKUPKIND_PLAINTEXT = 1 << 0,

    /**
     * Markdown is supported as a content format
     */
    LSTALK_MARKUPKIND_MARKDOWN = 1 << 1,
} LSTalk_MarkupKind;

/**
 * Completion item tags are extra annotations that tweak the rendering of a
 * completion item.
 *
 * @since 3.15.0
 */
typedef enum {
    /**
     * Render a completion as obsolete, usually using a strike-out.
     */
    LSTALK_COMPLETIONITEMTAG_DEPRECATED = 1 << 0,
} LSTalk_CompletionItemTag;

/**
 * How whitespace and indentation is handled during completion
 * item insertion.
 *
 * @since 3.16.0
 */
typedef enum {
    /**
     * The insertion or replace strings is taken as it is. If the
     * value is multi line the lines below the cursor will be
     * inserted using the indentation defined in the string value.
     * The client will not apply any kind of adjustments to the
     * string.
     */
    LSTALK_INSERTTEXTMODE_ASIS = 1 << 0,

    /**
     * The editor adjusts leading whitespace of new lines so that
     * they match the indentation up to the cursor of the line for
     * which the item is accepted.
     *
     * Consider a line like this: <2tabs><cursor><3tabs>foo. Accepting a
     * multi line completion item is indented using 2 tabs and all
     * following lines inserted will be indented using 2 tabs as well.
     */
    LSTALK_INSERTTEXTMODE_ADJUSTINDENTATION = 1 << 1,
} LSTalk_InsertTextMode;

/**
 * The kind of a completion entry.
 */
typedef enum {
    LSTALK_COMPLETIONITEMKIND_TEXT = 1 << 0,
    LSTALK_COMPLETIONITEMKIND_METHOD = 1 << 1,
    LSTALK_COMPLETIONITEMKIND_FUNCTION = 1 << 2,
    LSTALK_COMPLETIONITEMKIND_CONSTRUCTOR = 1 << 3,
    LSTALK_COMPLETIONITEMKIND_FIELD = 1 << 4,
    LSTALK_COMPLETIONITEMKIND_VARIABLE = 1 << 5,
    LSTALK_COMPLETIONITEMKIND_CLASS = 1 << 6,
    LSTALK_COMPLETIONITEMKIND_INTERFACE = 1 << 7,
    LSTALK_COMPLETIONITEMKIND_MODULE = 1 << 8,
    LSTALK_COMPLETIONITEMKIND_PROPERTY = 1 << 9,
    LSTALK_COMPLETIONITEMKIND_UNIT = 1 << 10,
    LSTALK_COMPLETIONITEMKIND_VALUE = 1 << 11,
    LSTALK_COMPLETIONITEMKIND_ENUM = 1 << 12,
    LSTALK_COMPLETIONITEMKIND_KEYWORD = 1 << 13,
    LSTALK_COMPLETIONITEMKIND_SNIPPET = 1 << 14,
    LSTALK_COMPLETIONITEMKIND_COLOR = 1 << 15,
    LSTALK_COMPLETIONITEMKIND_FILE = 1 << 16,
    LSTALK_COMPLETIONITEMKIND_REFERENCE = 1 << 17,
    LSTALK_COMPLETIONITEMKIND_FOLDER = 1 << 18,
    LSTALK_COMPLETIONITEMKIND_ENUMMEMBER = 1 << 19,
    LSTALK_COMPLETIONITEMKIND_CONSTANT = 1 << 20,
    LSTALK_COMPLETIONITEMKIND_STRUCT = 1 << 21,
    LSTALK_COMPLETIONITEMKIND_EVENT = 1 << 22,
    LSTALK_COMPLETIONITEMKIND_OPERATOR = 1 << 23,
    LSTALK_COMPLETIONITEMKIND_TYPEPARAMETER = 1 << 24,
} LSTalk_CompletionItemKind;

/**
 * A set of predefined code action kinds.
 */
typedef enum {
    /**
     * Empty kind.
     */
    LSTALK_CODEACTIONKIND_EMPTY = 1 << 0,

    /**
     * Base kind for quickfix actions: 'quickfix'.
     */
    LSTALK_CODEACTIONKIND_QUICKFIX = 1 << 1,

    /**
     * Base kind for refactoring actions: 'refactor'.
     */
    LSTALK_CODEACTIONKIND_REFACTOR = 1 << 2,

    /**
     * Base kind for refactoring extraction actions: 'refactor.extract'.
     *
     * Example extract actions:
     *
     * - Extract method
     * - Extract function
     * - Extract variable
     * - Extract interface from class
     * - ...
     */
    LSTALK_CODEACTIONKIND_REFACTOREXTRACT = 1 << 3,

    /**
     * Base kind for refactoring inline actions: 'refactor.inline'.
     *
     * Example inline actions:
     *
     * - Inline function
     * - Inline variable
     * - Inline constant
     * - ...
     */
    LSTALK_CODEACTIONKIND_REFACTORINLINE = 1 << 4,

    /**
     * Base kind for refactoring rewrite actions: 'refactor.rewrite'.
     *
     * Example rewrite actions:
     *
     * - Convert JavaScript function to class
     * - Add or remove parameter
     * - Encapsulate field
     * - Make method static
     * - Move method to base class
     * - ...
     */
    LSTALK_CODEACTIONKIND_REFACTORREWRITE = 1 << 5,

    /**
     * Base kind for source actions: `source`.
     *
     * Source code actions apply to the entire file.
     */
    LSTALK_CODEACTIONKIND_SOURCE = 1 << 6,

    /**
     * Base kind for an organize imports source action:
     * `source.organizeImports`.
     */
    LSTALK_CODEACTIONKIND_SOURCEORGANIZEIMPORTS = 1 << 7,

    /**
     * Base kind for a 'fix all' source action: `source.fixAll`.
     *
     * 'Fix all' actions automatically fix errors that have a clear fix that
     * do not require user input. They should not suppress errors or perform
     * unsafe fixes such as generating new types or classes.
     *
     * @since 3.17.0
     */
    LSTALK_CODEACTIONKIND_SOURCEFIXALL = 1 << 8,
} LSTalk_CodeActionKind;

typedef enum {
    /**
     * The client's default behavior is to select the identifier
     * according to the language's syntax rule.
     */
    LSTALK_PREPARESUPPORTDEFAULTBEHAVIOR_IDENTIFIER = 1,
} LSTalk_PrepareSupportDefaultBehavior;

/**
 * The diagnostic tags.
 *
 * @since 3.15.0
 */
typedef enum {
    /**
     * Unused or unnecessary code.
     *
     * Clients are allowed to render diagnostics with this tag faded out
     * instead of having an error squiggle.
     */
    LSTALK_DIAGNOSTICTAG_UNNECESSARY = 1 << 0,

    /**
     * Deprecated or obsolete code.
     *
     * Clients are allowed to rendered diagnostics with this tag strike through.
     */
    LSTALK_DIAGNOSTICTAG_DEPRECATED = 1 << 1,
} LSTalk_DiagnosticTag;

/**
 * A set of predefined range kinds.
 */
typedef enum {
    /**
     * Folding range for a comment
     */
    LSTALK_FOLDINGRANGEKIND_COMMENT = 1 << 0,

    /**
     * Folding range for a imports or includes
     */
    LSTALK_FOLDINGRANGEKIND_IMPORTS = 1 << 1,

    /**
     * Folding range for a region (e.g. `#region`)
     */
    LSTALK_FOLDINGRANGEKIND_REGION = 1 << 2,
} LSTalk_FoldingRangeKind;

typedef enum {
    LSTALK_TOKENFORMAT_RELATIVE = 1 << 0,
} LSTalk_TokenFormat;

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
    long long symbol_kind_value_set;

    /**
     * The client supports tags on `SymbolInformation` and `WorkspaceSymbol`.
     * Clients supporting tags have to handle unknown tags gracefully.
     *
     * @since 3.16.0
     *
     * tagSupport:
     * 
     * The tags supported by the client.
     */
    int tag_support_value_set;

    /**
     * The client support partial workspace symbols. The client will send the
     * request `workspaceSymbol/resolve` to the server to resolve additional
     * properties.
     *
     * @since 3.17.0 - proposedState
     *
     * resolveSupport:
     * 
     * The properties that a client can resolve lazily. Usually
     * `location.range`
     */
    char** resolve_support_properties;
    int resolve_support_count;
} LSTalk_WorkspaceSymbolClientCapabilities;

/**
 * Capabilities specific to the `workspace/executeCommand` request.
 */
typedef struct LSTalk_ExecuteCommandClientCapabilities {
    /**
     * Execute command supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_ExecuteCommandClientCapabilities;

/**
 * Capabilities specific to the semantic token requests scoped to the
 * workspace.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_SemanticTokensWorkspaceClientCapabilities {
    /**
     * Whether the client implementation supports a refresh request sent from
     * the server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * semantic tokens currently shown. It should be used with absolute care
     * and is useful for situation where a server for example detect a project
     * wide change that requires such a calculation.
     */
    int refresh_support;
} LSTalk_SemanticTokensWorkspaceClientCapabilities;

/**
 * Capabilities specific to the code lens requests scoped to the
 * workspace.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_CodeLensWorkspaceClientCapabilities {
    /**
     * Whether the client implementation supports a refresh request sent from the
     * server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * code lenses currently shown. It should be used with absolute care and is
     * useful for situation where a server for example detect a project wide
     * change that requires such a calculation.
     */
    int refresh_support;
} LSTalk_CodeLensWorkspaceClientCapabilities;

/**
 * The client has support for file requests/notifications.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_FileOperations {
    /**
     * Whether the client supports dynamic registration for file
     * requests/notifications.
     */
    int dynamic_registration;

    /**
     * The client has support for sending didCreateFiles notifications.
     */
    int did_create;

    /**
     * The client has support for sending willCreateFiles requests.
     */
    int will_create;

    /**
     * The client has support for sending didRenameFiles notifications.
     */
    int did_rename;

    /**
     * The client has support for sending willRenameFiles requests.
     */
    int will_rename;

    /**
     * The client has support for sending didDeleteFiles notifications.
     */
    int did_delete;

    /**
     * The client has support for sending willDeleteFiles requests.
     */
    int will_delete;
} LSTalk_FileOperations;

/**
 * Client workspace capabilities specific to inline values.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_InlineValueWorkspaceClientCapabilities {
    /**
     * Whether the client implementation supports a refresh request sent from
     * the server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * inline values currently shown. It should be used with absolute care and
     * is useful for situation where a server for example detect a project wide
     * change that requires such a calculation.
     */
    int refresh_support;
} LSTalk_InlineValueWorkspaceClientCapabilities;

/**
 * Client workspace capabilities specific to inlay hints.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_InlayHintWorkspaceClientCapabilities {
    /**
     * Whether the client implementation supports a refresh request sent from
     * the server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * inlay hints currently shown. It should be used with absolute care and
     * is useful for situation where a server for example detects a project wide
     * change that requires such a calculation.
     */
    int refresh_support;
} LSTalk_InlayHintWorkspaceClientCapabilities;

/**
 * Workspace client capabilities specific to diagnostic pull requests.
 *
 * @since 3.17.0
 */
typedef struct LSTalk_DiagnosticWorkspaceClientCapabilities {
    /**
     * Whether the client implementation supports a refresh request sent from
     * the server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * pulled diagnostics currently shown. It should be used with absolute care
     * and is useful for situation where a server for example detects a project
     * wide change that requires such a calculation.
     */
    int refresh_support;
} LSTalk_DiagnosticWorkspaceClientCapabilities;

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

    /**
     * Capabilities specific to the `workspace/executeCommand` request.
     */
    LSTalk_ExecuteCommandClientCapabilities execute_command;

    /**
     * The client has support for workspace folders.
     *
     * @since 3.6.0
     */
    int workspace_folders;

    /**
     * The client supports `workspace/configuration` requests.
     *
     * @since 3.6.0
     */
    int configuration;

    /**
     * Capabilities specific to the semantic token requests scoped to the
     * workspace.
     *
     * @since 3.16.0
     */
    LSTalk_SemanticTokensWorkspaceClientCapabilities semantic_tokens;

    /**
     * Capabilities specific to the code lens requests scoped to the
     * workspace.
     *
     * @since 3.16.0
     */
    LSTalk_CodeLensWorkspaceClientCapabilities code_lens;

    /**
     * The client has support for file requests/notifications.
     *
     * @since 3.16.0
     */
    LSTalk_FileOperations file_operations;

    /**
     * Client workspace capabilities specific to inline values.
     *
     * @since 3.17.0
     */
    LSTalk_InlineValueWorkspaceClientCapabilities inline_value;

    /**
     * Client workspace capabilities specific to inlay hints.
     *
     * @since 3.17.0
     */
    LSTalk_InlayHintWorkspaceClientCapabilities inlay_hint;

    /**
     * Client workspace capabilities specific to diagnostics.
     *
     * @since 3.17.0.
     */
    LSTalk_DiagnosticWorkspaceClientCapabilities diagnostics;
} LSTalk_Workspace;

typedef struct LSTalk_TextDocumentSyncClientCapabilities {
    /**
     * Whether text document synchronization supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * The client supports sending will save notifications.
     */
    int will_save;

    /**
     * The client supports sending a will save request and
     * waits for a response providing text edits which will
     * be applied to the document before it is saved.
     */
    int will_save_wait_until;

    /**
     * The client supports did save notifications.
     */
    int did_save;
} LSTalk_TextDocumentSyncClientCapabilities;

/**
 * The client supports the following `CompletionItem` specific
 * capabilities.
 */
typedef struct LSTalk_CompletionItem {
    /**
     * Client supports snippets as insert text.
     *
     * A snippet can define tab stops and placeholders with `$1`, `$2`
     * and `${3:foo}`. `$0` defines the final tab stop, it defaults to
     * the end of the snippet. Placeholders with equal identifiers are
     * linked, that is typing in one will update others too.
     */
    int snippet_support;

    /**
     * Client supports commit characters on a completion item.
     */
    int commit_characters_support;

    /**
     * Client supports the follow content formats for the documentation
     * property. The order describes the preferred format of the client.
     */
    int documentation_format;

    /**
     * Client supports the deprecated property on a completion item.
     */
    int deprecated_support;

    /**
     * Client supports the preselect property on a completion item.
     */
    int preselect_support;

    /**
     * Client supports the tag property on a completion item. Clients
     * supporting tags have to handle unknown tags gracefully. Clients
     * especially need to preserve unknown tags when sending a completion
     * item back to the server in a resolve call.
     *
     * @since 3.15.0
     *
     * tagSupport:
     *
     * The tags supported by the client.
     */
    int tag_support_value_set;

    /**
     * Client supports insert replace edit to control different behavior if
     * a completion item is inserted in the text or should replace text.
     *
     * @since 3.16.0
     */
    int insert_replace_support;

    /**
     * Indicates which properties a client can resolve lazily on a
     * completion item. Before version 3.16.0 only the predefined properties
     * `documentation` and `detail` could be resolved lazily.
     *
     * @since 3.16.0
     *
     * resolveSupport
     * 
     * The properties that a client can resolve lazily.
     */
    char** resolve_support_properties;
    int resolve_support_count;

    /**
     * The client supports the `insertTextMode` property on
     * a completion item to override the whitespace handling mode
     * as defined by the client (see `insertTextMode`).
     *
     * @since 3.16.0
     *
     * insertTextModeSupport
     * 
     */
    int insert_text_mode_support_value_set;

    /**
     * The client has support for completion item label
     * details (see also `CompletionItemLabelDetails`).
     *
     * @since 3.17.0
     */
    int label_details_support;
} LSTalk_CompletionItem;

/**
 * Capabilities specific to the `textDocument/completion` request.
 */
typedef struct LSTalk_CompletionClientCapabilities {
    /**
     * Whether completion supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * The client supports the following `CompletionItem` specific
     * capabilities.
     */
    LSTalk_CompletionItem completion_item;

    /**
     * completionItemKind
     * 
     * The completion item kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     *
     * If this property is not present the client only supports
     * the completion items kinds from `Text` to `Reference` as defined in
     * the initial version of the protocol.
     */
    long long completion_item_kind_value_set;

    /**
     * The client supports to send additional context information for a
     * `textDocument/completion` request.
     */
    int context_support;

    /**
     * The client's default when the completion item doesn't provide a
     * `insertTextMode` property.
     *
     * @since 3.17.0
     */
    int insert_text_mode;

    /**
     * The client supports the following `CompletionList` specific
     * capabilities.
     *
     * @since 3.17.0
     *
     * completionList
     * 
     * The client supports the following itemDefaults on
     * a completion list.
     *
     * The value lists the supported property names of the
     * `CompletionList.itemDefaults` object. If omitted
     * no properties are supported.
     *
     * @since 3.17.0
     */
    char** completion_list_item_defaults;
    int completion_list_item_defaults_count;
} LSTalk_CompletionClientCapabilities;

/**
 * Capabilities specific to the `textDocument/hover` request.
 */
typedef struct LSTalk_HoverClientCapabilities {
    /**
     * Whether hover supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * Client supports the follow content formats if the content
     * property refers to a `literal of type MarkupContent`.
     * The order describes the preferred format of the client.
     */
    int content_format;
} LSTalk_HoverClientCapabilities;

/**
 * The client supports the following `SignatureInformation`
 * specific properties.
 */
typedef struct LSTalk_SignatureInformation {
    /**
     * Client supports the follow content formats for the documentation
     * property. The order describes the preferred format of the client.
     */
    int documentation_format;

    /**
     * Client capabilities specific to parameter information.
     *
     * parameterInformation:
     * 
     * The client supports processing label offsets instead of a
     * simple label string.
     *
     * @since 3.14.0
     */
    int label_offset_support;

    /**
     * The client supports the `activeParameter` property on
     * `SignatureInformation` literal.
     *
     * @since 3.16.0
     */
    int active_parameter_support;
} LSTalk_SignatureInformation;

/**
 * Capabilities specific to the `textDocument/signatureHelp` request.
 */
typedef struct LSTalk_SignatureHelpClientCapabilities {
    /**
     * Whether signature help supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * The client supports the following `SignatureInformation`
     * specific properties.
     */
    LSTalk_SignatureInformation signature_information;

    /**
     * The client supports to send additional context information for a
     * `textDocument/signatureHelp` request. A client that opts into
     * contextSupport will also support the `retriggerCharacters` on
     * `SignatureHelpOptions`.
     *
     * @since 3.15.0
     */
    int context_support;
} LSTalk_SignatureHelpClientCapabilities;

/**
 * Capabilities specific to the `textDocument/declaration` request.
 *
 * @since 3.14.0
 */
typedef struct LSTalk_DeclarationClientCapabilities {
    /**
     * Whether declaration supports dynamic registration. If this is set to
     * `true` the client supports the new `DeclarationRegistrationOptions`
     * return value for the corresponding server capability as well.
     */
    int dynamic_registration;

    /**
     * The client supports additional metadata in the form of declaration links.
     */
    int link_support;
} LSTalk_DeclarationClientCapabilities;

/**
 * Capabilities specific to the `textDocument/definition` request.
 */
typedef struct LSTalk_DefinitionClientCapabilities {
    /**
     * Whether definition supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * The client supports additional metadata in the form of definition links.
     *
     * @since 3.14.0
     */
    int link_support;
} LSTalk_DefinitionClientCapabilities;

/**
 * Capabilities specific to the `textDocument/typeDefinition` request.
 *
 * @since 3.6.0
 */
typedef struct LSTalk_TypeDefinitionClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new `TypeDefinitionRegistrationOptions`
     * return value for the corresponding server capability as well.
     */
    int dynamic_registration;

    /**
     * The client supports additional metadata in the form of definition links.
     *
     * @since 3.14.0
     */
    int link_support;
} LSTalk_TypeDefinitionClientCapabilities;

/**
 * Capabilities specific to the `textDocument/implementation` request.
 *
 * @since 3.6.0
 */
typedef struct LSTalk_ImplementationClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new `ImplementationRegistrationOptions`
     * return value for the corresponding server capability as well.
     */
    int dynamic_registration;

    /**
     * The client supports additional metadata in the form of definition links.
     *
     * @since 3.14.0
     */
    int link_support;
} LSTalk_ImplementationClientCapabilities;

/**
 * Capabilities specific to the `textDocument/references` request.
 */
typedef struct LSTalk_ReferenceClientCapabilities {
    /**
     * Whether references supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_ReferenceClientCapabilities;

/**
 * Capabilities specific to the `textDocument/documentHighlight` request.
 */
typedef struct LSTalk_DocumentHighlightClientCapabilities {
    /**
     * Whether document highlight supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_DocumentHighlightClientCapabilities;

/**
 * Capabilities specific to the `textDocument/documentSymbol` request.
 */
typedef struct LSTalk_DocumentSymbolClientCapabilities {
    /**
     * Whether document symbol supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * Specific capabilities for the `SymbolKind` in the
     * `textDocument/documentSymbol` request.
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
    int symbol_kind_value_set;

    /**
     * The client supports hierarchical document symbols.
     */
    int hierarchical_document_symbol_support;

    /**
     * The client supports tags on `SymbolInformation`. Tags are supported on
     * `DocumentSymbol` if `hierarchicalDocumentSymbolSupport` is set to true.
     * Clients supporting tags have to handle unknown tags gracefully.
     *
     * @since 3.16.0
     *
     * tagSupport:
     * 
     * The tags supported by the client.
     */
    int tag_support_value_set;

    /**
     * The client supports an additional label presented in the UI when
     * registering a document symbol provider.
     *
     * @since 3.16.0
     */
    int label_support;
} LSTalk_DocumentSymbolClientCapabilities;

/**
 * Capabilities specific to the `textDocument/codeAction` request.
 */
typedef struct LSTalk_CodeActionClientCapabilities {
    /**
     * Whether code action supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * The client supports code action literals as a valid
     * response of the `textDocument/codeAction` request.
     *
     * @since 3.8.0
     *
     * codeActionLiteralSupport:
     * 
     * The code action kind is supported with the following value
     * set.
     *
     * codeActionKind:
     * 
     * The code action kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     */
    int code_action_value_set;

    /**
     * Whether code action supports the `isPreferred` property.
     *
     * @since 3.15.0
     */
    int is_preferred_support;

    /**
     * Whether code action supports the `disabled` property.
     *
     * @since 3.16.0
     */
    int disabled_support;

    /**
     * Whether code action supports the `data` property which is
     * preserved between a `textDocument/codeAction` and a
     * `codeAction/resolve` request.
     *
     * @since 3.16.0
     */
    int data_support;

    /**
     * Whether the client supports resolving additional code action
     * properties via a separate `codeAction/resolve` request.
     *
     * @since 3.16.0
     *
     * resolveSupport:
     * 
     * The properties that a client can resolve lazily.
     */
    char** resolve_support_properties;
    int resolve_support_count;

    /**
     * Whether the client honors the change annotations in
     * text edits and resource operations returned via the
     * `CodeAction#edit` property by for example presenting
     * the workspace edit in the user interface and asking
     * for confirmation.
     *
     * @since 3.16.0
     */
    int honors_change_annotations;
} LSTalk_CodeActionClientCapabilities;

/**
 * Capabilities specific to the `textDocument/codeLens` request.
 */
typedef struct LSTalk_CodeLensClientCapabilities {
    /**
     * Whether code lens supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_CodeLensClientCapabilities;

/**
 * Capabilities specific to the `textDocument/documentLink` request.
 */
typedef struct LSTalk_DocumentLinkClientCapabilities {
    /**
     * Whether document link supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * Whether the client supports the `tooltip` property on `DocumentLink`.
     *
     * @since 3.15.0
     */
    int tooltip_support;
} LSTalk_DocumentLinkClientCapabilities;

/**
 * Capabilities specific to the `textDocument/documentColor` and the
 * `textDocument/colorPresentation` request.
 *
 * @since 3.6.0
 */
typedef struct LSTalk_DocumentColorClientCapabilities {
    /**
     * Whether document color supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_DocumentColorClientCapabilities;

/**
 * Capabilities specific to the `textDocument/formatting` request.
 */
typedef struct LSTalk_DocumentFormattingClientCapabilities {
    /**
     * Whether formatting supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_DocumentFormattingClientCapabilities;

/**
 * Capabilities specific to the `textDocument/rangeFormatting` request.
 */
typedef struct LSTalk_DocumentRangeFormattingClientCapabilities {
    /**
     * Whether formatting supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_DocumentRangeFormattingClientCapabilities;

/**
 * Capabilities specific to the `textDocument/onTypeFormatting` request.
 */
typedef struct LSTalk_DocumentOnTypeFormattingClientCapabilities {
    /**
     * Whether on type formatting supports dynamic registration.
     */
    int dynamic_registration;
} LSTalk_DocumentOnTypeFormattingClientCapabilities;

/**
 * Capabilities specific to the `textDocument/rename` request.
 */
typedef struct LSTalk_RenameClientCapabilities {
    /**
     * Whether rename supports dynamic registration.
     */
    int dynamic_registration;

    /**
     * Client supports testing for validity of rename operations
     * before execution.
     *
     * @since version 3.12.0
     */
    int prepare_support;

    /**
     * Client supports the default behavior result
     * (`{ defaultBehavior: boolean }`).
     *
     * The value indicates the default behavior used by the
     * client.
     *
     * @since version 3.16.0
     */
    LSTalk_PrepareSupportDefaultBehavior prepare_support_default_behavior;

    /**
     * Whether the client honors the change annotations in
     * text edits and resource operations returned via the
     * rename request's workspace edit by for example presenting
     * the workspace edit in the user interface and asking
     * for confirmation.
     *
     * @since 3.16.0
     */
    int honors_change_annotations;
} LSTalk_RenameClientCapabilities;

/**
 * Capabilities specific to the `textDocument/publishDiagnostics`
 * notification.
 */
typedef struct LSTalk_PublishDiagnosticsClientCapabilities {
    /**
     * Whether the clients accepts diagnostics with related information.
     */
    int related_information;

    /**
     * Client supports the tag property to provide meta data about a diagnostic.
     * Clients supporting tags have to handle unknown tags gracefully.
     *
     * @since 3.15.0
     *
     * tagSupport:
     * 
     * The tags supported by the client.
     */
    int value_set;

    /**
     * Whether the client interprets the version property of the
     * `textDocument/publishDiagnostics` notification's parameter.
     *
     * @since 3.15.0
     */
    int version_support;

    /**
     * Client supports a codeDescription property
     *
     * @since 3.16.0
     */
    int code_description_support;

    /**
     * Whether code action supports the `data` property which is
     * preserved between a `textDocument/publishDiagnostics` and
     * `textDocument/codeAction` request.
     *
     * @since 3.16.0
     */
    int data_support;
} LSTalk_PublishDiagnosticsClientCapabilities;

/**
 * Capabilities specific to the `textDocument/foldingRange` request.
 *
 * @since 3.10.0
 */
typedef struct LSTalk_FoldingRangeClientCapabilities {
    /**
     * Whether implementation supports dynamic registration for folding range
     * providers. If this is set to `true` the client supports the new
     * `FoldingRangeRegistrationOptions` return value for the corresponding
     * server capability as well.
     */
    int dynamic_registration;

    /**
     * The maximum number of folding ranges that the client prefers to receive
     * per document. The value serves as a hint, servers are free to follow the
     * limit.
     */
    unsigned int range_limit;

    /**
     * If set, the client signals that it only supports folding complete lines.
     * If set, client will ignore specified `startCharacter` and `endCharacter`
     * properties in a FoldingRange.
     */
    int line_folding_only;

    /**
     * Specific options for the folding range kind.
     *
     * @since 3.17.0
     *
     * foldingRangeKind:
     * 
     * The folding range kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     */
    int value_set;

    /**
     * Specific options for the folding range.
     * @since 3.17.0
     *
     * foldingRange:
     * 
    * If set, the client signals that it supports setting collapsedText on
    * folding ranges to display custom labels instead of the default text.
    *
    * @since 3.17.0
    */
    int collapsed_text;
} LSTalk_FoldingRangeClientCapabilities;

/**
 * Capabilities specific to the `textDocument/selectionRange` request.
 *
 * @since 3.15.0
 */
typedef struct LSTalk_SelectionRangeClientCapabilities {
    /**
     * Whether implementation supports dynamic registration for selection range
     * providers. If this is set to `true` the client supports the new
     * `SelectionRangeRegistrationOptions` return value for the corresponding
     * server capability as well.
     */
    int dynamic_registration;
} LSTalk_SelectionRangeClientCapabilities;

/**
 * Capabilities specific to the `textDocument/linkedEditingRange` request.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_LinkedEditingRangeClientCapabilities {
    /**
     * Whether the implementation supports dynamic registration.
     * If this is set to `true` the client supports the new
     * `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
     * return value for the corresponding server capability as well.
     */
    int dynamic_registration;
} LSTalk_LinkedEditingRangeClientCapabilities;

/**
 * Capabilities specific to the various call hierarchy requests.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_CallHierarchyClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new `(TextDocumentRegistrationOptions &
     * StaticRegistrationOptions)` return value for the corresponding server
     * capability as well.
     */
    int dynamic_registration;
} LSTalk_CallHierarchyClientCapabilities;

/**
 * Capabilities specific to the various semantic token requests.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_SemanticTokensClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new `(TextDocumentRegistrationOptions &
     * StaticRegistrationOptions)` return value for the corresponding server
     * capability as well.
     */
    int dynamic_registration;

    /**
     * Which requests the client supports and might send to the server
     * depending on the server's capability. Please note that clients might not
     * show semantic tokens or degrade some of the user experience if a range
     * or full request is advertised by the client but not provided by the
     * server. If for example the client capability `requests.full` and
     * `request.range` are both set to true but the server only provides a
     * range provider the client might not render a minimap correctly or might
     * even decide to not show any semantic tokens at all.
     *
     * requests:
    /**
     * The client will send the `textDocument/semanticTokens/range` request
     * if the server provides a corresponding handler.
     *
     * range?: boolean | {}
     */
    int range;

    /**
     * requests:
     * 
     * The client will send the `textDocument/semanticTokens/full` request
     * if the server provides a corresponding handler.
     *
     * full?: boolean | {}
     * 
     * The client will send the `textDocument/semanticTokens/full/delta`
     * request if the server provides a corresponding handler.
     */
    int delta;

    /**
     * The token types that the client supports.
     */
    char** token_types;
    int token_types_count;

    /**
     * The token modifiers that the client supports.
     */
    char** token_modifiers;
    int token_modifiers_count;

    /**
     * The formats the clients supports.
     */
    int formats;

    /**
     * Whether the client supports tokens that can overlap each other.
     */
    int overlapping_token_support;

    /**
     * Whether the client supports tokens that can span multiple lines.
     */
    int multiline_token_support;

    /**
     * Whether the client allows the server to actively cancel a
     * semantic token request, e.g. supports returning
     * ErrorCodes.ServerCancelled. If a server does the client
     * needs to retrigger the request.
     *
     * @since 3.17.0
     */
    int server_cancel_support;

    /**
     * Whether the client uses semantic tokens to augment existing
     * syntax tokens. If set to `true` client side created syntax
     * tokens and semantic tokens are both used for colorization. If
     * set to `false` the client only uses the returned semantic tokens
     * for colorization.
     *
     * If the value is `undefined` then the client behavior is not
     * specified.
     *
     * @since 3.17.0
     */
    int augments_syntax_tokens;
} LSTalk_SemanticTokensClientCapabilities;

/**
 * Capabilities specific to the `textDocument/moniker` request.
 *
 * @since 3.16.0
 */
typedef struct LSTalk_MonikerClientCapabilities {
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new `(TextDocumentRegistrationOptions &
     * StaticRegistrationOptions)` return value for the corresponding server
     * capability as well.
     */
    int dynamic_registration;
} LSTalk_MonikerClientCapabilities;

/**
 * Text document specific client capabilities.
 */
typedef struct LSTalk_TextDocumentClientCapabilities {
    LSTalk_TextDocumentSyncClientCapabilities synchronization;

    /**
     * Capabilities specific to the `textDocument/completion` request.
     */
    LSTalk_CompletionClientCapabilities completion;

    /**
     * Capabilities specific to the `textDocument/hover` request.
     */
    LSTalk_HoverClientCapabilities hover;

    /**
     * Capabilities specific to the `textDocument/signatureHelp` request.
     */
    LSTalk_SignatureHelpClientCapabilities signature_help;

    /**
     * Capabilities specific to the `textDocument/declaration` request.
     *
     * @since 3.14.0
     */
    LSTalk_DeclarationClientCapabilities declaration;

    /**
     * Capabilities specific to the `textDocument/definition` request.
     */
    LSTalk_DefinitionClientCapabilities definition;

    /**
     * Capabilities specific to the `textDocument/typeDefinition` request.
     *
     * @since 3.6.0
     */
    LSTalk_TypeDefinitionClientCapabilities type_definition;

    /**
     * Capabilities specific to the `textDocument/implementation` request.
     *
     * @since 3.6.0
     */
    LSTalk_ImplementationClientCapabilities implementation;

    /**
     * Capabilities specific to the `textDocument/references` request.
     */
    LSTalk_ReferenceClientCapabilities references;

    /**
     * Capabilities specific to the `textDocument/documentHighlight` request.
     */
    LSTalk_DocumentHighlightClientCapabilities document_highlight;

    /**
     * Capabilities specific to the `textDocument/documentSymbol` request.
     */
    LSTalk_DocumentSymbolClientCapabilities document_symbol;

    /**
     * Capabilities specific to the `textDocument/codeAction` request.
     */
    LSTalk_CodeActionClientCapabilities code_action;

    /**
     * Capabilities specific to the `textDocument/codeLens` request.
     */
    LSTalk_CodeLensClientCapabilities code_lens;

    /**
     * Capabilities specific to the `textDocument/documentLink` request.
     */
    LSTalk_DocumentLinkClientCapabilities document_link;

    /**
     * Capabilities specific to the `textDocument/documentColor` and the
     * `textDocument/colorPresentation` request.
     *
     * @since 3.6.0
     */
    LSTalk_DocumentColorClientCapabilities color_provider;

    /**
     * Capabilities specific to the `textDocument/formatting` request.
     */
    LSTalk_DocumentFormattingClientCapabilities formatting;

    /**
     * Capabilities specific to the `textDocument/rangeFormatting` request.
     */
    LSTalk_DocumentRangeFormattingClientCapabilities range_formatting;

    /**
     * Capabilities specific to the `textDocument/onTypeFormatting` request.
     */
    LSTalk_DocumentOnTypeFormattingClientCapabilities on_type_formatting;

    /**
     * Capabilities specific to the `textDocument/rename` request.
     */
    LSTalk_RenameClientCapabilities rename;

    /**
     * Capabilities specific to the `textDocument/publishDiagnostics`
     * notification.
     */
    LSTalk_PublishDiagnosticsClientCapabilities publish_diagnostics;

    /**
     * Capabilities specific to the `textDocument/foldingRange` request.
     *
     * @since 3.10.0
     */
    LSTalk_FoldingRangeClientCapabilities folding_range;

    /**
     * Capabilities specific to the `textDocument/selectionRange` request.
     *
     * @since 3.15.0
     */
    LSTalk_SelectionRangeClientCapabilities selection_range;

    /**
     * Capabilities specific to the `textDocument/linkedEditingRange` request.
     *
     * @since 3.16.0
     */
    LSTalk_LinkedEditingRangeClientCapabilities linked_editing_range;

    /**
     * Capabilities specific to the various call hierarchy requests.
     *
     * @since 3.16.0
     */
    LSTalk_CallHierarchyClientCapabilities call_hierarchy;

    /**
     * Capabilities specific to the various semantic token requests.
     *
     * @since 3.16.0
     */
    LSTalk_SemanticTokensClientCapabilities semantic_tokens;

    /**
     * Capabilities specific to the `textDocument/moniker` request.
     *
     * @since 3.16.0
     */
    LSTalk_MonikerClientCapabilities moniker;
} LSTalk_TextDocumentClientCapabilities;

/**
 * The capabilities provided by the client (editor or tool)
 */
typedef struct LSTalk_ClientCapabilities {
    /**
     * Workspace specific client capabilities.
     */
    LSTalk_Workspace workspace;

    /**
     * Text document specific client capabilities.
     */
    LSTalk_TextDocumentClientCapabilities text_document;
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
