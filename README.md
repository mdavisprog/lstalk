# lstalk
LSTalk is a C library that enables communication with a language server. This can empower applications to have features of a text editor. This includes features like auto-complete, go to definition, find all references, and many more. More information about language servers can be found [here](https://microsoft.github.io/language-server-protocol/).

## Example
```C++
exmaple.cpp

class Shape
{
};

class Circle : public Shape
{
};

class Rectangle : public Shape
{
};

enum Color
{
    Red,
    Green,
    Blue,
};

int main()
{
    return 0;
}
```

```C
struct LSTalk_Context* context = lstalk_init();
if (context == NULL) {
    return -1;
}

LSTalk_ConnectParams params;
params.root_uri = NULL;
params.trace = LSTALK_TRACE_VERBOSE;
params.seek_path_env = 1;
LSTalk_ServerID server = lstalk_connect(context, CLANGD, &params);
if (server != LSTALK_INVALID_SERVER_ID) {
    lstalk_text_document_did_open(context, server, "example.cpp");
    lstalk_text_document_symbol(context, server, "example.cpp");
    while (1) {
        lstalk_process_responses(context);

        LSTalk_Notification notification;
        lstalk_poll_notification(context, server, &notification);
        if (notification.type == LSTALK_NOTIFICATION_TEXT_DOCUMENT_SYMBOLS) {
            printf("Document symbols count: %d\n", notification.data.document_symbols.symbols_count);
            for (int i = 0; i < notification.data.document_symbols.symbols_count; i++) {
                printf("   %s - %s\n", notification.data.document_symbols.symbols[i].name,
                    lstalk_symbol_kind_to_string(notification.data.document_symbols.symbols[i].kind));
            }
            break;
        }
    }

    lstalk_close(context, server);
}

lstalk_shutdown(context);
```
```
Output:

Document symbols count: 8
   Shape - class
   Circle - class
   Rectangle - class
   Color - enum
   Red - enum
   Green - enum
   Blue - enum
   main - function
```
