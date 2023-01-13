#include "lstalk.h"
#include <stdio.h>

int main() {
#if LSTALK_TESTS
    lstalk_tests();
#else
    printf("Did not compile with LSTALK_TESTS enabled!\n");
#endif
    return 0;
}
