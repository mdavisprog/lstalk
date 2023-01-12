#include "lstalk.h"

int main(int argc, char** argv) {
    if (!lstalk_init()) {
        return -1;
    }

    lstalk_shutdown();

    return 0;
}
