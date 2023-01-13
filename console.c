#include "lstalk.h"
#include <stdio.h>

int main(int argc, char** argv) {
    if (!lstalk_init()) {
        return -1;
    }

    int major = 0;
    int minor = 0;
    int revision = 0;
    lstalk_version(&major, &minor, &revision);
    printf("LSTalk version %d.%d.%d\n", major, minor, revision);

    lstalk_shutdown();
    return 0;
}
