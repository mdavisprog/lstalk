#include "../lstalk.h"
#include <stdio.h>

int main() {
    struct LSTalk_Context* context = lstalk_init();
    if (context == NULL) {
        return -1;
    }

    int major = 0;
    int minor = 0;
    int revision = 0;
    lstalk_version(&major, &minor, &revision);
    printf("LSTalk version %d.%d.%d\n", major, minor, revision);

    lstalk_shutdown(context);
    return 0;
}
