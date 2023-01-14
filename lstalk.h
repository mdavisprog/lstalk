
#ifndef __LSTALK_H__
#define __LSTALK_H__

struct LSTalk_Context;

struct LSTalk_Context* lstalk_init();
void lstalk_shutdown(struct LSTalk_Context* context);
void lstalk_version(int* major, int* minor, int* revision);

#ifdef LSTALK_TESTS
void lstalk_tests();
#endif

#endif
