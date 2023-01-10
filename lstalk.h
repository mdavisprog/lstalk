
#ifndef __LSTALK_H__
#define __LSTALK_H__

int lstalk_init();
void lstalk_shutdown();

#ifdef LSTALK_TESTS
void lstalk_tests();
#endif

#endif
