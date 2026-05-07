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

#ifndef __ARRAY_H__
#define __ARRAY_H__

#include "../lstalk.h"

typedef struct Array {
    char* data;
    size_t element_size;
    size_t length;
    size_t capacity;
} Array;

Array array_create(size_t element_size);
void array_destroy(Array* array, LSTalk_Allocator allocator);
void array_resize(Array* array, size_t capacity, LSTalk_Allocator allocator);
void array_push(Array* array, void* element, LSTalk_Allocator allocator);
void array_append(Array* array, void* elements, size_t count, LSTalk_Allocator allocator);
int array_remove(Array* array, size_t index);
char* array_get(Array* array, size_t index);

#endif // __ARRAY_H__
