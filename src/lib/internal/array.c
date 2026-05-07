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

#include "array.h"

static void copy(char* dest, char* src, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        dest[i] = src[i];
    }
}

Array array_create(size_t element_size) {
    Array result = {};
    result.element_size = element_size;
    result.length = 0;
    result.capacity = 0;
    result.data = NULL;
    return result;
}

void array_destroy(Array* array, LSTalk_Allocator allocator) {
    if (array == NULL) return;

    if (array->data != NULL) {
        allocator.free(array->data);
    }

    array->data = NULL;
    array->element_size = 0;
    array->length = 0;
    array->capacity = 0;
}

void array_resize(Array *array, size_t capacity, LSTalk_Allocator allocator) {
    if (array == NULL || array->element_size == 0) {
        return;
    }

    array->capacity = capacity;
    array->data = allocator.realloc(array->data, array->element_size * array->capacity);
}

void array_push(Array *array, void *element, LSTalk_Allocator allocator) {
    if (array == NULL || element == NULL || array->element_size == 0) {
        return;
    }

    if (array->length == array->capacity) {
        const int half_capacity = array->capacity / 2;
        array_resize(array, array->capacity + half_capacity, allocator);
    }

    size_t offset = array->length * array->element_size;
    copy(array->data + offset, element, array->element_size);
    array->length++;
}

void array_append(Array *array, void *elements, size_t count, LSTalk_Allocator allocator) {
    if (array == NULL || elements == NULL || array->element_size == 0) {
        return;
    }

    size_t remaining = array->capacity - array->length;

    if (count > remaining) {
        array_resize(array, array->capacity + (count - remaining) * 2, allocator);
    }

    size_t offset = array->length * array->element_size;
    copy(array->data + offset, elements, count * array->element_size);
    array->length += count;
}

int array_remove(Array* array, size_t index) {
    if (array == NULL || array->element_size == 0 || index >= array->length) {
        return 0;
    }

    // If removed the final index, then do nothing.
    if (index == array->length - 1) {
        array->length--;
        return 1;
    }

    char* start = array->data + index * array->element_size;
    char* end = start + array->element_size;
    size_t count = array->length - (index + 1);
    size_t size = count * array->element_size;
    copy(start, end, size);
    array->length--;
    return 1;
}

char* array_get(Array* array, size_t index) {
    if (array == NULL || array->element_size == 0 || index >= array->length) {
        return NULL;
    }

    return &array->data[array->element_size * index];
}
