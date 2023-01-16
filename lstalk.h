
#ifndef __LSTALK_H__
#define __LSTALK_H__

struct LSTalk_Context;

struct LSTalk_Context* lstalk_init();
void lstalk_shutdown(struct LSTalk_Context* context);
void lstalk_version(int* major, int* minor, int* revision);
int lstalk_connect(struct LSTalk_Context* context, const char* uri);
int lstalk_process_responses(struct LSTalk_Context* context);

#ifdef LSTALK_TESTS
void lstalk_tests();
#endif

#endif
