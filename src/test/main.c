/*

MIT License

Copyright (c) 2026 Mitchell Davis <mdavisprog@gmail.com>

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

#include "../lib/internal/array.h"
#include <stdlib.h>
#include <stdio.h>
#include "test.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    int major = 0;
    int minor = 0;
    int revision = 0;
    lstalk_version(&major, &minor, &revision);

    LSTalk_Allocator allocator = {
        .malloc = malloc,
        .calloc = calloc,
        .realloc = realloc,
        .free = free
    };

    LSTalk_Context* context = lstalk_init(allocator);

    Array test_suites = array_create(sizeof(TestSuite));

    printf("Running %zu test suites for lstalk version %d.%d.%d.\n",
        test_suites.length,
        major,
        minor,
        revision
    );

    for (size_t i = 0; i < test_suites.length; ++i) {
        TestSuite* suite = (TestSuite*)array_get(&test_suites, i);

        printf("Running %zu '%s' tests...\n", suite->tests.length, suite->name);

        for (size_t test_index = 0; test_index < suite->tests.length; ++test_index) {
            TestCase* test = (TestCase*)array_get(&suite->tests, test_index);

            const bool success = test->fn(allocator);
            printf("   '%s' %s\n", test->name, success == false ? "false" : "true");
        }

        printf("   completed\n");
    }

    for (size_t i = 0; i < test_suites.length; ++i) {
        TestSuite* suite = (TestSuite*)array_get(&test_suites, i);
        array_destroy(&suite->tests, allocator);
    }
    array_destroy(&test_suites, allocator);

    lstalk_shutdown(context);

    return 0;
}
